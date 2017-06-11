/*
 * This is the SM metafile driver 
 */
#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "sm.h"
#include "devices.h"
#include "sm_declare.h"

enum meta_codes {
   META_EOF, META_HEADER,
   META_CHAR, META_DOT, META_DRAW, META_LWEIGHT,
   META_ENABLE, META_FILL_PT, META_GFLUSH, META_IDLE, META_LINE,
   META_PAGE, META_REDRAW, META_RELOC
  };

extern int sm_verbose;			/* how chatty should I be? */

static int read_d P_ARGS(( FILE *, int * ));
static int write_d P_ARGS(( FILE *, int ));
static int read_f P_ARGS(( FILE *, float * ));
static int write_f P_ARGS(( FILE *, double ));

/*****************************************************************************/
/*
 * Here's the function to read a metafile
 */
#define LEN 100
static char *meta_version = "SM Metafile, version 1.0\n";
static char metafile[LEN] = "";		/* name of metafile */
static FILE *mfil = NULL;		/* file descriptor for metafile */
static int real_devnum = 0;		/* devnum for the real current device*/

/*****************************************************************************/
/*
 * create and initialiase a metafile
 */
static int
creat_metafile(file)
char *file;
{
   int len;

   if((mfil = fopen(file,"wb")) == NULL) {
      msg_1s("Can't create %s\n",file);
      return(-1);
   }
   strncpy(metafile,file,LEN - 1);
/*
 * write header
 */
   len = strlen(meta_version);
   if(write_d(mfil,(int)META_HEADER) != 0 ||
      fwrite(meta_version,1,len,mfil) != len) {
      msg_1s("Can't write header for %s\n",metafile);
      *metafile = '\0';
      fclose(mfil);
      return(-1);
   }
   return(0);
}

/*****************************************************************************/
/*
 * open a metafile
 */
int
open_metafile(file)
char *file;
{
   if(mfil != NULL) {
      msg_1s("Metafile %s is already open\n",metafile);
      return(-1);
   }
   creat_metafile(file);

   real_devnum = devnum;
   devnum = METAFILE;
   return(0);
}

/*****************************************************************************/
/*
 * close a metafile
 */
void
close_metafile()
{
   if(mfil != NULL) {
      fclose(mfil);
      mfil = NULL;
   }
   *metafile = '\0';
   devnum = real_devnum;		/* restore original device */
   real_devnum = 0;
}

/*****************************************************************************/
/*
 * playback a metafile
 */
int
playback_metafile(file)
char *file;
{
   char buff[LEN];
   int error;				/* have we seen an error? */
   FILE *fil;				/* file descriptor for metafile */
   int len;
   enum meta_codes code;
   int x1,y1,x2,y2;
   float lw;

   if(strcmp(file,metafile) == 0) {
      msg("You cannot read from a file with the same name as the open metafile\n");
      return(-1);
   }

   if((fil = fopen(file,"rb")) == NULL) {
      msg_1s("Can't open %s\n",file);
      return(-1);
   }
/*
 * read and check header
 */
   len = strlen(meta_version);
   if(read_d(fil,(int *)&code) != 0 || code != META_HEADER ||
      fread(buff,1,len,fil) != len){
      msg_1s("Can't read header from %s\n",file);
      fclose(fil);
      return(-1);
   }
   if(strncmp(buff,meta_version,len) != 0) {
      msg_2s("Header for file %s is wrong; should be %s\n",file,meta_version);
      fclose(fil);
      return(-1);
   }
/*
 * and now read and dispatch the commands
 */
   error = 0;
   while(read_d(fil,(int *)&code) == 0) {
      switch (code) {
       case META_EOF:
	 fclose(fil);
	 fil = NULL;
	 return(0);
       case META_HEADER:		/* skip the header */
	 error += fseek(fil,strlen(meta_version),1);
	 break;
       case META_DOT:
	 error += read_d(fil,&x1);
	 error += read_d(fil,&y1);
	 RELOC(x1,y1);			/* DOT() may fail */
	 xp = x1; yp = y1;
	 sm_dot();
	 break;
       case META_DRAW:
	 error += read_d(fil,&x1);
	 error += read_d(fil,&y1);
	 DRAW(x1,y1);
	 xp = x1; yp = y1;
	 break;
       case META_ENABLE:
	 ENABLE();
	 break;
       case META_IDLE:
	 IDLE();
	 break;
       case META_GFLUSH:
	 GFLUSH();
	 break;
       case META_LINE:
	 error += read_d(fil,&x1);
	 error += read_d(fil,&y1);
	 error += read_d(fil,&x2);
	 error += read_d(fil,&y2);
	 LINE(x1,y1,x2,y2);
	 xp = x2; yp = y2;
	 break;
       case META_LWEIGHT:
	 error += read_f(fil,&lw);
	 LWEIGHT(lw);
	 break;
       case META_PAGE:
	 PAGE();
	 break;
       case META_RELOC:
	 error += read_d(fil,&x1);
	 error += read_d(fil,&y1);
	 xp = x1; yp = y1;
	 RELOC(x1,y1);
	 break;
       default:
	 msg_1d("Unknown metafile code %d",(int)code);
	 msg_1s(" from file %s\n",file);
	 error++;
	 break;
      }
      if(error) {
	 msg("Error reading metafile\n");
	 break;
      }
   }
/*
 * done
 */
   fclose(fil);
   return(0);
}

/*****************************************************************************/
/*
 * functions to do metafile i/o
 */
static int
read_d(fil,i)
FILE *fil;
int *i;
{
   return((fread((char *)i,sizeof(int),1,fil) == 1) ? 0 : 1);
}
      
static int
write_d(fil,i)
FILE *fil;
int i;
{
   return((fwrite((char *)&i,sizeof(int),1,fil) == 1) ? 0 : 1);
}
      
static int
read_f(fil,f)
FILE *fil;
float *f;
{
   return((fread((char *)f,sizeof(float),1,fil) == 1) ? 0 : 1);
}
      
static int
write_f(fil,d)
FILE *fil;
double d;
{
   float f = d;

   return((fwrite((char *)&f,sizeof(float),1,fil) == 1) ? 0 : 1);
}
      
/*****************************************************************************/
/*
 * and now the metafile device calls
 *
 * In all cases where SM interrogates the device for its capabilities,
 * we must make the most pessimistic reply
 */
int
meta_char(s,x,y)
char *s;
int x,y;
{
   return(-1);				/* must disable hardware chars */
}

void
meta_close()
{
   if(mfil != NULL) {
      msg_1s("Closing metafile %s\n",metafile);
      fclose(mfil);
      mfil = NULL;
      *metafile = '\0';
      real_devnum = 0;
   }

   (*devices[real_devnum].dev_close)();
}

void
meta_ctype(r,g,b)
int r,g,b;
{
   (*devices[real_devnum].dev_ctype)(r,g,b);
}

int
meta_cursor(get_pos,x,y)
int get_pos;				/* simply return the current position */
int *x,*y;
{
   return((*devices[real_devnum].dev_cursor)(get_pos,x,y));
}

int
meta_dot(x,y)
int x,y;
{
   int dot_ret;				/* did real_device draw a DOT? */
   int error;

   dot_ret = (*devices[real_devnum].dev_dot)(x,y);
   if(dot_ret == 0) {			/* device drew a dot */
      error = write_d(mfil,(int)META_DOT);
      error += write_d(mfil,x);
      error += write_d(mfil,y);
      if(error != 0 && sm_verbose > 0) {
	 msg("Error in writing DOT to metafile\n");
      }
   }
   return(dot_ret);
}

void
meta_draw(x,y)
int x,y;
{
   int error;
   
   error = write_d(mfil,(int)META_DRAW);
   error += write_d(mfil,x);
   error += write_d(mfil,y);
   if(error != 0 && sm_verbose > 0) {
      msg("Error in writing DRAW to metafile\n");
   }
   (*devices[real_devnum].dev_draw)(x,y);
}

void
meta_enable()
{
   if(write_d(mfil,(int)META_ENABLE) && sm_verbose > 0) {
      msg("Error in writing ENABLE to metafile\n");
   }
   (*devices[real_devnum].dev_enable)();
}

void
meta_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

   if(mfil != NULL) {
      fclose(mfil);
      mfil = NULL;
      creat_metafile(metafile);
   }

   (*devices[real_devnum].dev_erase)();
}

int
meta_fill_pt(n)
int n;
{
   return(-1);				/* must disable fill_pt */
}

void
meta_gflush()
{
   if(write_d(mfil,(int)META_GFLUSH) && sm_verbose > 0) {
      msg("Error in writing GFLUSH to metafile\n");
   }
   (*devices[real_devnum].dev_gflush)();
}

void
meta_idle()
{
   if(write_d(mfil,(int)META_IDLE) && sm_verbose > 0) {
      msg("Error in writing IDLE to metafile\n");
   }
   (*devices[real_devnum].dev_idle)();
}

void
meta_line(x1,y1,x2,y2)
int x1,y1,x2,y2;
{
   int error;
   
   error = write_d(mfil,(int)META_LINE);
   error += write_d(mfil,x1);
   error += write_d(mfil,y1);
   error += write_d(mfil,x2);
   error += write_d(mfil,y2);
   if(error != 0 && sm_verbose > 0) {
      msg("Error in writing LINE to metafile\n");
   }
   (*devices[real_devnum].dev_line)(x1,y1,x2,y2);
}

int
meta_ltype(lt)
int lt;
{
   return(-1);				/* must disable hardware linetypes */
}

int
meta_lweight(lw)
double lw;
{
   int lw_ret;				/* did real_device draw a DOT? */
   int error;

   lw_ret = (*devices[real_devnum].dev_lweight)(lw);
   if(lw_ret == 0) {			/* device drew a lw */
      error = write_d(mfil,(int)META_LWEIGHT);
      error += write_f(mfil,lw);
      if(error != 0 && sm_verbose > 0) {
	 msg("Error in writing LWEIGHT to metafile\n");
      }
   }
   return(lw_ret);
}

void
meta_page()
{
   if(write_d(mfil,(int)META_PAGE) && sm_verbose > 0) {
      msg("Error in writing PAGE to metafile\n");
   }
   (*devices[real_devnum].dev_page)();
}

void
meta_redraw(fil)
int fil;
{
   (*devices[real_devnum].dev_redraw)(fil); /* don't put this in a metafile! */
}

void
meta_reloc(x,y)
int x,y;
{
   int error = write_d(mfil,(int)META_RELOC);
   error += write_d(mfil,x);
   error += write_d(mfil,y);
   if(error != 0 && sm_verbose > 0) {
      msg("Error in writing RELOC to metafile\n");
   }
   (*devices[real_devnum].dev_reloc)(x,y);
}

int
meta_setup(s)
char *s;
{
   msg("You cannot open a metafile directly -- use DEVICE META filename\n");
   return(-1);
}

int
meta_set_ctype(colors,n)
COLOR *colors;
int n;
{
   return((*devices[real_devnum].dev_set_ctype)(colors,n));
}
