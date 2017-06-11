#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "tty.h"
#include "stdgraph.h"
#include "sm.h"
#include "devices.h"
#include "sm_declare.h"

#define FREE(I) { free((char *)I); I = NULL; }

static char *stg_gstring();
static int scale();

/*
 * Now actually declare the stdgraph.h variables
 */
SG *g_sg = NULL;		/* stdgraph graphics descriptor */
TTY *g_tty = NULL;		/* graphcap descriptor */
char *g_xy = NULL;		/* pointer to coord encoding string */
int g_in = -1, g_out = -1;	/* file descriptors for device */
int g_xc = -1, g_yc = -1;	/* current window position */
REGISTER g_reg[NREGISTERS] = {};/* encoder registers */
char g_mem[SZ_MEMORY+2] = {};	/* encoder memory */

#define SIZE 80			/* size of buffers */
#define SIZE_ARGS 150		/* size of argument buffer */
/*
 * stg_setup -- Open the named device. Opening a device involves initialization
 * of the data structures, following by initialization of the device itself.
 */
int
stg_setup(c_line)
char *c_line;				/* rest of DEVICE command line */
{
   char *av[N_ARGS+1],			/* args == g_sg->sg_av */
        buff[SIZE],		        /* temp buffers */
	*ptr;
   static char args[SIZE_ARGS];		/* arguments to command */
   int ac,				/* number of args == g_sg->sg_ac */
       fd,
       i,j;

   if(g_sg != NULL) {
      FREE(g_sg);
   }
   if(g_tty != NULL) {
      FREE(g_tty);
   }

   kill_sy_macros();			/* forget any graphcap (SY) macros */
/*
 * Setup pointers to arguments
 */
   args[SIZE_ARGS - 1] = '\0';
   (void)strncpy(args,c_line,SIZE_ARGS - 1);
   ptr = args;
   ac = 0;
   for(i = 0;i <= N_ARGS;i++) {
      while(isspace(*ptr)) ptr++;	/* skip whitespace */
      if(*ptr == '\0') break;
      av[i] = ptr;
      ac++;
      if(*ptr == ':') {			/* a graphcap entry */
	 ptr++;				/* read the ':' */
	 while(*ptr != '\0') {
	    if(*ptr++ == ':' && (*ptr == '\0' || isspace(*ptr))) {
	       break;
	    }
	 }
      } else {
	 while(*ptr != '\0' && !isspace(*ptr)) ptr++;
      }
      if(*ptr == '\0') break;
      *ptr++ = '\0';
   }
   if(ac <= 0) {
      msg("No device specified for stdgraph\n");
      return(-1);
   }
/*
 * Initialize the kernel datastructures. Open graphcap descriptor
 * for the named device, allocate and initialize descriptor and SG.
 */
   if((g_tty = ttygdes(ac,av)) == NULL) {
      msg_1s("Can't get graphcap entry for %s\n",av[0]);
      return(-1);
   }
   if(initialise(g_tty,av[0]) < 0) {	/* Initialize data structures*/
      FREE(g_sg);
      FREE(g_tty);
      return(-1);
   }

   for(i = j = 0;i < ac;i++) {		/* copy the arguments into g_sg */
      if(av[i][0] != ':') {		/* don't save :...: args */
	 g_sg->sg_av[j++] = av[i];
      }
   }
   g_sg->sg_ac = j;
   
   if(scale() < 0) {			/* get SCREEN -> device coords */
      FREE(g_sg);
      FREE(g_tty);
      return(-1);
   }
/*
 * Now setup input/output files
 */
   g_in = 0;					/* read from terminal */
   if(ttygets(g_tty,"OF",buff,SIZE) == 0) {	/* no output file */
      termout = 1;
      if(!ttygetb(g_tty,"DV") && ac > 1 && av[ac - 1][0] != ':') {
	 (void)strcpy(g_sg->sg_outfile,av[ac - 1]);
	 if((g_out = ttopen(g_sg->sg_outfile)) < 0) {
	    msg_1s("Can't open %s\n",g_sg->sg_outfile);
	    g_sg->sg_outfile[0] = '\0';
	    if((g_out = ttopen((char *) NULL)) < 0) {	/* use stdout */
	       g_out = 1;
	    }
	 } 
      } else {
	 g_sg->sg_outfile[0] = '\0';
	 if((g_out = ttopen((char *) NULL)) < 0) {	/* use stdout */
	    g_out = 1;
	 }
      }
   } else {
      (void)strcpy(g_sg->sg_outfile,get_full_name(buff));
      if((g_out = ttopen(g_sg->sg_outfile)) < 0) {
	 msg_1s("Can't open %s\n",g_sg->sg_outfile);
	 g_sg->sg_outfile[0] = '\0';
	 FREE(g_sg);
	 FREE(g_tty);
	 return(-1);
      } else {
	 termout = 0;			/* we are NOT talking to a terminal */
      }
   }
/*
 * Initialize the device.  Output initialization string followed by
 * contents of initialization file, if named.
 */
   stg_ctrl("OW");
   if(ttygets(g_tty,"IF",buff,SIZE) > 0) {
      if((fd = open(buff,0)) < 0) {
 	 msg_1s("Can't open %s\n",buff);
      } else {
	 fcopyo(fd,g_out);
	 (void)close(fd);
      }
   }
   stg_ctrl("OX");
   stg_ctrl("OY");
   stg_ctrl("OZ");

   g_xc = -1;				/* current device coordinates */
   g_yc = -1;
/*
 * Now set some variables for SM
 */
   if(ttygets(g_tty,"DC",buff,SIZE) > 0) { /* set default colour */
      default_ctype(buff);
   } else {
      default_ctype("white");
   }
/*
 * This may not really be a stdgraph device at all, let's find out
 *
 * If DV contains any spaces, treat anything after the first space
 * as extra (leading) arguments for the DEVICE command
 */
   if(ttygets(g_tty,"DV",buff,SIZE - 1) > 0) {
      if((ptr = strchr(buff,' ')) != NULL) {
	 *ptr = '\0';
      }
      for(i = 0;i < ndev;i++) {
	 if(strcmp(devices[i].d_name,buff) == 0) {
	    char buff2[SIZE + 1];	/* temp buffer */
	    c_line = strncpy(buff2, c_line, SIZE); buff2[SIZE] = '\0';

	    while(*c_line != '\0' && !isspace(*c_line)) c_line++;
	    while(isspace(*c_line)) c_line++;
	    if(ptr != NULL) {
	       ptr++;
	       strncat(ptr, " ", SIZE - 1 - (ptr - buff));
	       c_line = strncat(ptr, c_line, SIZE - 1 - (ptr - buff));
	    }
	    
	    if((*devices[i].dev_setup)(c_line) < 0) {
	       FREE(g_sg);
	       FREE(g_tty);
	       return(-1);
	    }
	    return(i);
	 }
      }
      msg_2s("Device %s requires the %s driver, which is not installed\n",
	     av[0],buff);
      FREE(g_sg);
      FREE(g_tty);
      return(-1);
   }
   return(STDGRAPH);
}

/***************************************************/
/*
 * Enable graphics on a device
 */
void
stg_enable()
{
   if(g_tty == NULL) {			/* this can be called while setting
					   up a new g_tty */
      return;
   }
   stg_ctrl("GE");
/*
 * Tektronix terminals seem to forget where they are after a
 * graphics enable. This code will remind them
 */
   if(termout) {
      ttyputs(g_out, g_tty, g_sg->sg_startmove,strlen(g_sg->sg_startmove),1);
      g_reg[1].i = g_xc;
      g_reg[2].i = g_yc;
      g_reg[E_IOP].i = 0;
      stg_encode(g_xy, g_mem, g_reg);
      ttwrite(g_out, g_mem, g_reg[E_IOP].i);
      ttyputs(g_out, g_tty, g_sg->sg_endmove,strlen(g_sg->sg_endmove),1);
   }
}


/***************************************************/
/*
 * Flush graphics on a device
 */
void
stg_gflush()
{
   ttflush(g_out);
}

/***************************************************/
/*
 * Copy file with fd in to file with fd out
 */
void
fcopyo(in,out)
int in,out;
{
#define BSIZE 512
   char buff[BSIZE];
   int i;
   
   i = BSIZE;
   while((i = read(in,buff,BSIZE)) > 0) ttwrite(out,buff,i);
}

/****************************************************/
/*
 * initialise - Initialize the stdgraph data structures from the graphcap entry
 * for the device.
 */
#define BTOI(X) (X)		/* boolean -> integer */

int
initialise(tty,devname)
TTY *tty;			/* graphcap descriptor */
char devname[];			/* device name */
{
   char *nextch;
   int maxch, i;
/*
 * Allocate the stdgraph descriptor and the string buffer.
 */
   if((g_sg = (SG *)malloc(sizeof(SG))) == NULL) {
      msg("Can't malloc g_sg in initialise()\n");
      return(-1);
   }
/*
 * Init string buffer parameters.  The first char of the string buffer
 * is reserved as a null string, used for graphcap control strings
 * omitted from the graphcap entry for the device.
 */
   g_sg->sg_szsbuf = SZ_SBUF;
   g_sg->sg_nextch = g_sg->sg_sbuf + 1;
   g_sg->sg_sbuf[0] = '\0';
/*
 * Initialize the encoder.  The graphcap parameter LR contains encoder
 * instructions to perform any device dependent initialization required.
 */
   for(i = 0;i < NREGISTERS;i++) g_reg[i].i = 0;
   nextch = g_sg->sg_nextch;

   g_reg[E_IOP].i = 0;
   g_reg[E_TOP].i = SZ_MEMORY;
   stg_ctrl("LR");
   stg_ctrl("LX");
   stg_ctrl("LY");
   stg_ctrl("LZ");
/*
 * Initialize the output parameters.  All boolean parameters are stored
 * as integer flags.  All string valued parameters are stored in the
 * string buffer, saving a pointer to the string in the stdgraph
 * descriptor.  If the capability does not exist the pointer is set to
 * point to the null string at the beginning of the string buffer.
 */
   g_sg->sg_encodexy = stg_gstring("XY");
   g_xy = g_sg->sg_encodexy;

   g_sg->sg_startdraw  = stg_gstring ("DS");
   g_sg->sg_enddraw    = stg_gstring ("DE");
   g_sg->sg_startdraw_rel = stg_gstring ("dS");
   g_sg->sg_enddraw_rel = stg_gstring ("dE");
   g_sg->sg_startmove  = stg_gstring ("VS");
   g_sg->sg_endmove    = stg_gstring ("VE");
   g_sg->sg_startmove_rel = stg_gstring ("vS");
   g_sg->sg_endmove_rel = stg_gstring ("vE");
   g_sg->sg_startmark  = stg_gstring ("MS");
   g_sg->sg_endmark    = stg_gstring ("ME");
   g_sg->sg_starttext  = stg_gstring ("TB");
   g_sg->sg_endtext    = stg_gstring ("TE");
/*
 * Initialize the input parameters.
 */
   g_sg->sg_lencur = ttygeti (tty, "CN");
   g_sg->sg_delimcur   = stg_gstring ("MC");
   g_sg->sg_decodecur  = stg_gstring ("SC");
/*
 * Save the device string in the descriptor.
 */
   g_sg->sg_devname = nextch = g_sg->sg_nextch;
   maxch = g_sg->sg_sbuf + SZ_SBUF - nextch + 1;
   nextch += gstrcpy(nextch,devname,maxch) + 1;
   g_sg->sg_nextch = nextch;
   return(0);
}
/*
 * STG_GSTRING -- Get a string value parameter from the graphcap table,
 * placing the string at the end of the string buffer.  If the device does
 * not have the named capability return a pointer to the null string,
 * otherwise return a pointer to the string.  Since pointers are used,
 * rather than indices, the string buffer is fixed in size.  The additional
 * degree of indirection required with an index was not considered worthwhile
 * in this application since the graphcap entries are never very large.
 */
static char *
stg_gstring (cap)
char	cap[];		/* device capability to be fetched */
{
   char *strp, *nextch;
   int	maxch, nchars;

   nextch = g_sg->sg_nextch;
   maxch = g_sg->sg_sbuf + SZ_SBUF - nextch;

   nchars = ttygets (g_tty, cap, nextch, maxch);
   if (nchars > 0) {
       strp = nextch;
       nextch += nchars + 1;
   } else
       strp = g_sg->sg_sbuf;

   g_sg->sg_nextch = nextch;
   return (strp);
}

/*
 * like stg_gstring, but if the string is of the form "$..." return the value
 */
static char *
stg_gstring_expand(cap)
char	cap[];		/* device capability to be fetched */
{
   int i = 0;
   char *ptr = stg_gstring(cap);

   if(ptr == NULL || ptr[0] != '$' || ptr[1] == '\0') {
      return(ptr);
   }

   ptr++;
   if(isdigit(ptr[0]) && ptr[1] == '\0') {
      int i = ptr[0] - '0';
      if(i < g_sg->sg_ac) {
	 return(g_sg->sg_av[i]);
      }
   }

   msg_1s("Reference to DEVICE %s ", g_sg->sg_av[0]);
   msg_1d("argument %d; ", i);
   msg_1d("only %d were provided\n", g_sg->sg_ac - 1);

   return(NULL);
}

/****************************************************/
/*
 * scale - set up the transformation to device space.
 */
static int
scale()
{
   int nx, ny;				/* number of points */
   char *ptr;
/*
 * get the size of the displayed coordinates
 */
   ptr = stg_gstring_expand("xr");
   if(*ptr == '#') ptr++;
   nx = atoi(ptr);
   
   ptr = stg_gstring_expand("yr");
   if(*ptr == '#') ptr++;
   ny = atoi(ptr);
   
   if(nx <= 1) nx = SCREEN_SIZE/100;
   if(ny <= 1) ny = SCREEN_SIZE/100;
/*
 * SCREEN -> window coords
 */
   xscreen_to_pix = (float)nx/((long)SCREEN_SIZE + 1); /* long for 16bit ints*/
   					/* beware rounding errors */
   yscreen_to_pix = (float)ny/((long)SCREEN_SIZE + 1);
   aspect = (float)ny/nx;
   
   if(g_sg->sg_starttext[0] == '\0') {
      stg_char_size(1,0);
   } else {
      stg_char_size(1,1);
   }
   ldef = SCREEN_SIZE/(float)(nx < ny ? ny : nx) + 0.5;
   
   return(0);
}

void
stg_char_size(init,hard_fonts)
int init,				/* initialise? */
    hard_fonts;				/* hardware fonts? */
{
   static int ch_h,cw_h;		/* hardware font sizes */

   if(hard_fonts) {
      if(init) {
	 ch_h = ttygetr(g_tty,"ch")*SCREEN_SIZE;
	 cw_h = ttygetr(g_tty,"cw")*SCREEN_SIZE;
	 if(ch_h == 0 || cw_h == 0) {
	    msg("For hardware characters you need \"ch\" and \"cw\"\n");
	    g_sg->sg_starttext[0] = '\0';
	 }
      } else {
	 cheight = ch_h;
	 cwidth = cw_h;
      }
   }
}   

/*****************************************************************************/
/*
 * select an SM device
 */
int
set_device(name)
char *name;
{
   int dn;
   
   CLOSE(); stg_close();
   devnum = NODEVICE;

   if((dn = stg_setup(name)) < 0) {
      devnum = NODEVICE;
      msg_1s("No such device %s\n",name);
      return(-1);
   } else {
      devnum = dn;
   }

   return(0);
}
