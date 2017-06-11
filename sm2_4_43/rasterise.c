#include "copyright.h"
/*
 * Rasterise a set of vectors produced by the SM `raster' device.
 * The input file format is groups of 4 shorts giving x1 y1 x2 y2.
 * The output file may be specified as `-', meaning use standard
 * output.
 *
 * Flags:	'R' for Rotate plot through 90deg
 *		`r' for remove-infile-after-succesfully-plotting
 * 		`v' for sm_verbose
 *
 * We access graphcap to get the O[WXYZ] and CW strings and the device
 * dimensions xr and yr, and uses OF and SY to do the plotting.
 * Graphcap capabilities specific to rasterise are:
 *	BP (Bit Pattern)	String of Bit patterns to turn on
 * 				1st, 2nd, ... pixels. Must be <= NBITS
 *	BR (Begin Row)		Begin a rastered row
 *	EP (Empty Pixel)	Bit pattern for an empty char (no dots)
 *	ER (End Row)		End a rastered row
 *	ll (lINE lENGTH)	Length of a line; RD=hex only
 *	nb (nUM bYTES)		Number of bytes to process together in MR
 *	MR (Many Rows)		Output several rows at once
 *	RD (Raster Device)	Type of device if not generic; currently
 *				only "default" and "hex" are supported
 *
 * This code assumes that there are NBITS bits in a byte (but hasn't
 * been tested for NBITS != 8).
 */
#define STANDALONE			/* this is a stand-alone executable */
#include "options.h"
#include <stdio.h>
#include <signal.h>
#if !defined(VMS)
#include <fcntl.h>
#endif
#include "devices.h"
#include "tty.h"
#include "stdgraph.h"
#include "raster.h"
#include "sm_declare.h"

#define NBITS 8				/* number of bits in byte */
#define NBYTES 4			/* max number of bytes MR can handle */
#define SYNTAX fprintf(stderr,"rasterise [-rRv] device infile outfile\n");
#define SIZE 40				/* max size of RD entry */

/*
 * These #defines are a little odd, as we have to use the same
 * structure as the `real' graphcap devices as we use their libraries.
 * Some of the functions return void and some int, hence the weird
 * choice (e.g. ROW is dev_ltype?)
 */
#undef CLOSE
#undef OPEN
#define INIT (*devices[rast_devnum].dev_setup)
#define CLOSE (*devices[rast_devnum].dev_close)
#define OPEN (*devices[rast_devnum].dev_char)
#define ROW (*devices[rast_devnum].dev_ltype)
/*
 * Here's a struct to describe devices in case any are too perverse
 * for graphcap to handle
 */
int unused_int() { return(0); }		/* don't do anything */
void unused_void() { ; }

int gen_init(), gen_open(), gen_row();	/* generic device */
void gen_close();
int hex_init(), hex_row();		/* hexadecimal device */

DEVICES devices[] = {
   {					/* generic device */
      "default",
      gen_init,		unused_void, 	unused_void, 	unused_void,
      unused_void, 	gen_open, 	gen_row,	unused_int,
      unused_void, 	unused_void, 	unused_int,	gen_close,
      unused_int, 	unused_int, 	unused_int, 	unused_void,	unused_int,
      unused_void, 	unused_void, 	unused_void,
   },
   {
      "hex",				/* write output in hex */
      hex_init,		unused_void, 	unused_void, 	unused_void,
      unused_void, 	gen_open, 	hex_row,	unused_int,
      unused_void, 	unused_void, 	unused_int,	gen_close,
      unused_int, 	unused_int, 	unused_int, 	unused_void,	unused_int,
      unused_void, 	unused_void, 	unused_void,
   },
};

static int rast_devnum,			/* device in use */
  	   rast_ndev = sizeof(devices)/sizeof(devices[0]),
					/* number of raster devices */
           row_by,			/* number of bytes to make up row */
  	   xr;				/* x size of device */

static char graphcap[200];		/* graphcap file */

/*********************************************************************/
/*
 * We have to declare the variables and functions from devices.a. Most
 * are not used.
 */
#include "dummies.h"

int
main(ac,av)
int ac;
char *av[];
{
   char *aptr,				/* == av[1] */
        ctype,				/* current ctype (if a colour device) */
	device_type[SIZE];		/* the type of raster device */
   unsigned char empty[2],		/* empty[0] => no pixels turned on */
                 mask[NBITS*NBYTES + 2], /* mask[i] turns on i'th pixel */
                 **page,		 /* the entire page being drawn */
                 red[256],
                 green[256], blue[256]; /* definitions of colours */
   int color,				/* is this a colour plot? */
       diagx,dlg,dsml,dx,dy,		/* used by line drawing algorithm */
       init = 0,			/* do we needto initialise page[][]? */
       i,j,
       mr,				/* MR is true */
       nb,				/* num. of bytes at once */
       nbits = 0,			/* num. of significant bits per char */
       nc,				/* num. of bits at once (`per char') */
       nrow,				/* number of lines of output data */
       remove_file = 0,			/* remove infile after use */
       rotate = 0,			/* rotate plot */
       std_in,std_out,			/* in/output is going to stdin/out */
       x1,y1,x2,y2,			/* ends of vector */
       yr;				/* y-dimension of device */
   short coords[4];			/* read coordinates of vector */

   if(ac <= 1) {
      SYNTAX;
      exit(EXIT_BAD);
   }
   while(ac > 3 && *av[1] == '-') {
      aptr = &av[1][1];
      for(;*aptr != '\0';aptr++) {
	 switch (*aptr) {
	  case 'R':			/* Rotate plot through 90 deg */
	    rotate = 1;
	    break;
	  case 'r':			/* rm infile after reading it */
	    remove_file = 1;
	    break;
	  case 'v':			/* verbose */
	    sm_verbose = 1;
	    break;
	  default:
	    fprintf(stderr,"Unknown flag %c\n",*aptr);
	    break;
	 }
      }
      ac--; av++;
   }
   if(ac <= 3) {
      SYNTAX;
      exit(EXIT_BAD);
   }
   set_defaults_file((char *)NULL,(char *)NULL);
   setvar("graphcap",get_val("graphcap"));
/*
 * Open graphcap entry for device
 */
   if((g_tty = ttygdes(1,&av[1])) == NULL) {
      fprintf(stderr,"rasterise: can't get graphcap entry for %s\n",av[1]);
      exit(EXIT_BAD);
   }
   if(initialise(g_tty,av[1]) < 0) {
      free((char *)g_tty);
      exit(EXIT_BAD);
   }
/*
 * See which device driver to use
 */
   if(ttygets(g_tty,"RD",device_type,SIZE) <= 0) {
      rast_devnum = 0;
   } else {
      for(i = 0;i < rast_ndev;i++) {
	 if(strcmp(device_type,devices[i].d_name) == 0) {
	    break;
	 }
      }
      if(i == rast_ndev) {
	 fprintf(stderr,
		 "Rasterise: Unknown device type \"%s\", using default\n",
		 device_type);
	 rast_devnum = 0;
      } else {
	 rast_devnum = i;
      }
   }

   if(rotate) {
      yr = ttygeti(g_tty,"xr");
      xr = ttygeti(g_tty,"yr");
   } else {
      xr = ttygeti(g_tty,"xr");
      yr = ttygeti(g_tty,"yr");
   }
   if(xr == 0 || yr == 0) {
      fprintf(stderr,"rasterise: X- or Y- resolution for %s is 0; giving up\n",
	      av[1]);
      exit(EXIT_BAD);
   }
/*
 * See if the hardware likes to print a number of rows at a time. If it does
 * (that is, "MR" is in graphcap) look to see how many bytes are involved
 * (which is given by "nb"). Assume that the bits in those bytes all apply
 * to different y values at constant x (i.e. we assume that the mapping
 * of pixels to bytes is either vertical or horizontal, but not rectangular)
 */
   if((mr = ttygetb(g_tty,"MR")) != 0) {
      if((nb = ttygeti(g_tty,"nb")) <= 0) {
	 nb = 1;
      }
   } else {
      nb = 1;				/* in theory other values are possible,
					   but they are not supported */
   }
/*
 * Now look for the bit representation of turned on pixels. For example,
 * "10000000" is often \001, but not always. There are two problems, the number
 * of bits specified in each char, and their layout. For example, a printronix
 * puts 6 pixels in an (8-bit) byte, and "010000" is written \102.
 * To deal with this there is a capability BP (Bit Pattern) in graphcap which
 * gives the bit pattern to turn on the first, second, etc. bit in the char.
 *
 * We also need the char representing no pixels turned on, which is in EP
 *
 * If MR is specified these bit patterns refer to different rows at constant
 * x, as explained above
 */
   if((color = ttygetb(g_tty,"CL")) != 0) { /* a colour device */
      nc = 1;				/* we'll store the colour index */
      ctype = 0;			/* this'll be reset */
   } else {
       if((nc = ttygets(g_tty,"BP",(char *)mask,NBITS*NBYTES + 2)) <= 0) {
	 nc = NBITS*nb;
	 for(i = 0;i < nc;i++) {
	    mask[i] = 1 << (i%NBITS);
	 }
      } else if(nc > NBITS*NBYTES) {
	 msg_1s("Too many bits specified in \"BP\" in graphcap for %s\n",
									av[1]);
	 nc = NBITS*NBYTES;
      }
      if(nc%nb != 0) {
	 msg_1d("I can't divide the BP bit pattern evenly into %d bytes\n",nb);
      }
      nbits = nc/nb;
      
      if(!mr && nc > NBITS) {
	 msg("Only MR devices may specify bit patterns more than 1by wide\n");
	 nc = nbits = NBITS;
      }
   }
   (void)ttygets(g_tty,"EP",(char *)empty,2);
/*
 * Because this programme will only run for a short time, I'm going
 * to do this the memory-inefficient way, and define an array big
 * enough to represent the entire device.
 * It surely can't be much bigger than 1Mby.
 *
 * This should be faster than playing games with linked lists or
 * whatever to do it one line at a time.
 */
   if(mr) {
      nrow = yr/nc + (yr%nc == 0 ? 0 : 1);
      row_by = xr*nb;
   } else {
      nrow = yr;
      row_by = xr/nc + (xr%nc == 0 ? 0 : 1);
   }
   if(color) {
      row_by *= 3;			/* 3 bytes per pixel */
   }
   
   if(sm_verbose) {
      fprintf(stderr,"Array in rasterise is %ldby (%dx%d)\n",
                        (long)row_by*nrow,row_by,nrow);
   }

   if((page = malloc(nrow*sizeof(unsigned char *))) == NULL) {
      fprintf(stderr,"rasterise: can't allocate storage for page\n");
      exit(EXIT_BAD);
   }
   for(i = 0;i < nrow;i++) {
      if((page[i] = (unsigned char *)malloc((unsigned)row_by)) == NULL) {
         fprintf(stderr,"rasterise: cannot allocate %dth row\n",i);
         exit(EXIT_BAD);
      }
   }
/*
 * Open input file
 */
   if(!strcmp(av[2],"-")) {		/* read from stdin */
      std_in = 1;
      g_in = 0;
   } else {
      std_in = 0;
      if((g_in = open(av[2],0|O_BINARY)) < 0) {
	 fprintf(stderr,"Can't open %s\n",av[2]);
	 exit(EXIT_BAD);
      }
   }
/*
 * Open output device
 */
   if(!strcmp(av[3],"-")) {		/* write to stdout */
      std_out = 1;
      g_out = 1;
   } else {
      std_out = 0;
      if((g_out = OPEN(av[3])) < 0) {
	 fprintf(stderr,"rasterise: can't open %s\n",av[3]);
	 if(std_in) close(g_in);
	 exit(EXIT_BAD);
      }
   }
   stg_ctrl("OW");
   stg_ctrl("OX");
   stg_ctrl("OY");
   stg_ctrl("OZ");
/*
 * We are probably running in the background, so ignore signals
 */
   (void)signal(SIGINT,SIG_IGN);             /* ignore ^C */
#ifdef SIGTSTP
   (void)signal(SIGTSTP,SIG_IGN);            /* ignore ^Z */
#endif
/*
 * We have now got the files open and initialised,
 * the page allocated, and the pointers set up.
 * Read the vectors, and draw the lines on the page
 *
 * Line drawing algorithm originally found (in assembler) in the driver for
 * a calcomp by Bill Sebok, rewritten in Fortran by Tom Quinn, converted to
 * C by Robert Lupton, and now put into SM
 */
   if(INIT() < 0) {
      exit(EXIT_BAD);
   }
   
   {					/* Make the register variables local */
      register int a,strx,stry,px,py;
      long nvec = 0;
      
      while(read(g_in,(char *)coords,4*sizeof(short)) == 4*sizeof(short)) {
         if(sm_verbose) {
            printf("%ld\r",nvec);
            nvec++;
         }
	 if(coords[0] < 0) {
	    if(coords[0] == RASTER_EOF) {
	       break;
	    } else if(coords[0] == RASTER_SET_CTYPE) {
	       union {
		  char c[2];
		  short s;
	       } conv;
	       conv.s = coords[2];
	       red[coords[1]] = conv.c[0];
	       green[coords[1]] = conv.c[1];
	       conv.s = coords[3];
	       blue[coords[1]] = conv.c[0];
	    } else if(coords[0] == RASTER_CTYPE) {
	       ctype = coords[1];
	    } else {
	       fprintf(stderr,"Unknown code in rasterise: %d\n",coords[0]);
	    }
	    continue;
	 }

	 if(!init) {			/* now's the time to initialise,
					   after reading initial ctypes */
	    init = 1;

	    if(color) {
	       for(i = 0; i < row_by;) { /* fill first row with `empty' */
		  page[0][i++] = red[(int)*empty];
		  page[0][i++] = green[(int)*empty];
		  page[0][i++] = blue[(int)*empty];
	       }
	    } else {
	       for(i = 0;i < row_by;) {	/* fill first row with `empty' */
		  page[0][i++] = *empty;
	       }
	    }

	    for(i = 1;i < nrow;i++) {
	       memcpy(page[i],page[0],row_by);	/* fill rest of page */
	    }
	 }

	 if(rotate) {
	    x1 = xr - 1 - coords[1]; y1 = coords[0];
	    x2 = xr - 1 - coords[3]; y2 = coords[2];
	 } else {
	    x1 = coords[0]; y1 = coords[1];
	    x2 = coords[2]; y2 = coords[3];
	 }
	 
	 dx = x2 - x1;
	 dy = y2 - y1;
	 
	 if(dy >= 0) {		/* find which octant and assign accordingly */
	    py = y1;
	    px = x1;
	 } else {
	    px = x2;
	    py = y2;
	    dy = -dy;
	    dx = -dx;
	 }
	 if(dx >= 0) {
	    if(dy >= dx) {
	       a = dy/2;
	       dlg = dy;
	       dsml = dx;
	       strx = 0;
	       stry = 1;
	       diagx = 1;
	    } else {
	       dlg = dx;
	       dsml = dy;
	       a = dx/2;
	       strx = 1;
	       stry = 0;
	       diagx = 1;
	    }
	 } else {
	    if(dy >= -dx) {
	       a = dy/2;
	       dlg = dy;
	       dsml = -dx;
	       strx = 0;
	       stry = 1;
	       diagx = -1;
	    } else {
	       dlg = -dx;
	       dsml = dy;
	       a = -dx/2;
	       strx = -1;
	       stry = 0;
	       diagx = -1;
	    }
	 }
/*
 * Now set the bits corresponding to that line. The following loops should
 * be identical except for the line containing the "|=", but I wanted to
 * pull the test outside the loop
 */
	 if(mr) {
	    for(j = 0;j <= dlg;j++) {	/* write the vector */
	       page[py/nc][px*nb + (py%nc)/nbits] |= mask[py%nc];
	       a -= dsml;
	       if (a >= 0) {
		  px += strx;
		  py += stry;
	       } else {
		  px += diagx;
		  py++;
		  a += dlg;
	       }    
	    }
	 } else if(color) {
	    int col;			/* desired colour */
	    int ipx;			/* page index corresponding to px */
	    for(j = 0;j <= dlg;j++) {	/* write the vector */
	       ipx = px*3;		/* index into page */
	       col = ctype;

	       page[py][ipx] =     red[col];
	       page[py][ipx + 1] = green[col];
	       page[py][ipx + 2] = blue[col];

	       a -= dsml;
	       if (a >= 0) {
		  px += strx;
		  py += stry;
	       } else {
		  px += diagx;
		  py++;
		  a += dlg;
	       }    
	    }
	 } else {
	    for(j = 0;j <= dlg;j++) {	/* write the vector */
	       page[py][px/nc] |= mask[px%nc];
	       a -= dsml;
	       if (a >= 0) {
		  px += strx;
		  py += stry;
	       } else {
		  px += diagx;
		  py++;
		  a += dlg;
	       }    
	    }
	 }
      }
      if(sm_verbose) {
         fprintf(stderr,"\nRead %ld vectors\n",nvec);
      }
   }
   if(!std_in) close(g_in);
   if(remove_file) {
#ifdef vms
      delete(av[2]);
#else
      unlink(av[2]);
#endif
   }
/*
 * We have now filled the page with vectors, so call the output
 * routine line by line. Note that it's the output routines's job
 * to deal with BR and ER
 *
 * If MR is true then the `line' is a row of data containing the
 * pixels of nc physical lines
 */
   for(i = nrow - 1;i >= 0;i--) {
      ROW(page[i],i);
   }

   stg_ctrl("CW");			/* Close Workstation string */
   if(std_out) {
      ttflush(g_out);			/* don't close stdout! */
   } else {
      CLOSE(g_out);
   }
   
   free((char *)page);
   free((char *)g_tty);
   return(EXIT_OK);
}

/******************************************************************/
/*
 * Deal with a line of output in the generic case (when dev_open was gen_open,
 * and dev_close will be gen_close), when one line is output at a time
 */
int gen_init() { return(0); }
int gen_open(file) char *file;
{
   return(ttopen(file));
}
void gen_close(fd) int fd;
{
   (void)ttclose(fd);
}

int
gen_row(line,y)
unsigned char *line;			/* line to be output */
int y;					/* current y value */
{
   stg_ctrl1("BR",y);			/* beginning of row sequence  */
   ttwrite(g_out,(char *)line,row_by);
   stg_ctrl1("ER",y);			/* end of row sequence  */
   return(0);
}

/*********************************************************************/
/*
 * The "hex" device: write the output in ascii hex digits (2 per byte).
 * Uses a graphcap parameter ll which gives the length of a line.
 */
static char *hex_line = NULL;		/* the buffer for writing hex */
static int hex_len;

int
hex_init()
{
   int len;

   if((hex_len = ttygeti(g_tty,"ll")) <= 0) {
      hex_len = 75;
   }
/*
 * Calculate how long a buffer we need to write the data, allowing for
 * a newline every hex_len characters written, and remembering that we'll
 * get a factor of 2 increase in the number of bytes when we convert to hex.
 */
   len = 2*row_by;
   len += len/hex_len + 1;		/* this can be 1 too many... */

   if(hex_line == NULL) {
      if((hex_line = malloc(len)) == NULL) {
	 fprintf(stderr,"Can't allocate storage for hex_line\n");
	 return(-1);
      }
   } else {
      if((hex_line = realloc(hex_line,len)) == NULL) {
	 fprintf(stderr,"Can't reallocate storage for hex_line\n");
	 return(-1);
      }
   }
   return(0);
}

int
hex_row(line,y)
unsigned char *line;			/* line to be output */
int y;					/* current y value */
{
   int i;
   char *end,*ptr;		/* pointers to hex_line */
   
   stg_ctrl1("BR",y);			/* beginning of row sequence  */

   ptr = hex_line;
   for(i = 0;i < row_by;) {
      end = ptr + (2*(row_by - i) > hex_len ? hex_len : 2*(row_by - i));
      while(ptr < end) {
	 sprintf(ptr,"%.02x",line[i++]); ptr += 2;
      }
      *ptr++ = '\n';
   }
   ttwrite(g_out,hex_line,ptr - hex_line);

   stg_ctrl1("ER",y);			/* end of row sequence  */
   return(0);
}

/*****************************************************************************/
/*
 * Define a variable in non-interactive SM, and print the corresponding
 * value.
 */
void
setvar(name,value)
char *name,*value;
{
   if(value == NULL) value = "";
   
   if(!strcmp("graphcap",name)) {
      strcpy(graphcap,value);
   } else {
      msg_1s("You can't define %s in rasterise\n",name);
   }
}

char *
print_var(name)
char *name;
{
   if(!strcmp("graphcap",name)) {
      return(graphcap);
   } else {
      return("");
   }
}
