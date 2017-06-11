#include "copyright.h"
/*
 * Do file and tty i/o.
 * We need these routines, partly for efficiency, and partly because io
 * gets a bit formatted by VMS
 */
#include <stdio.h>
#include "options.h"
#include "sm.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm_declare.h"
#if defined(__BORLANDC__)
#  include <fcntl.h>
#endif

#define BUFSIZE 512		/* size for output buffer */
#define IM_NOOP 254		/* noop for padding imagen file */

static char buffer[BUFSIZE],
	    *bptr = buffer;

#if defined(unix) || defined(__MSDOS__) || defined(__OS2__)
/*
 * Use open/write/close but do some buffering
 */
int
ttclose(fd)
int fd;
{
   ttflush(fd);

   if(fd < 0) {
      return(-1);
   }

   if(termout) {
      return(0);			/* don't close stdout */
   } else {
      return(close(fd));
   }
}

int
ttopen(file)
char *file;
{
   if(file == NULL) {
      return(1);				/* use stdout */
   } else {
#if defined(__BORLANDC__)
      return(open(file,O_WRONLY|O_BINARY|O_CREAT,0644));
#else
      return(creat(file,0644));
#endif
   }
}
#endif

#ifdef vms
/*
 * We'll have to use qio calls to cicumvent wrapping every 80 chars by the
 * terminal driver
 */
#   include <descrip.h>
#   include <ttdef.h>
#   include <iodef.h>
   
int
ttclose(fd)
int fd;
{
   ttflush(fd);
   if(termout) {
      sys$dassgn(fd);
   } else {
      if(fd < 0) {
         return(-1);
      } else {
         return(close(fd));
      }
   }
}

int
ttopen(file)			/* NOT ttyopen */
char *file;
{
   char dt[20];				/* Device Type */
   short chan;				/* channel to terminal */
   struct dsc$descriptor_s descrip;	/* VMS descriptor */

   if(file == NULL) {
      descrip.dsc$w_length  = strlen("tt:");	/* What VMS requires */
      descrip.dsc$b_dtype   = DSC$K_DTYPE_T;
      descrip.dsc$b_class   = DSC$K_CLASS_S;
      descrip.dsc$a_pointer = "tt:";

      (void)sys$assign(&descrip,&chan,0,0);
      return(chan);
   } else {
      if(ttygets(g_tty,"DT",dt,20) == 0) {	/* DT is not defined */
         return(creat(file,0644));
      } else if(!strcmp(dt,"qms")) {
         return(creat(file,0644));
      } else if(!strcmp(dt,"imagen")) {
         return(creat(file,0644));
      } else {
 	 msg_1s("Unknown DT from graphcap: %s\n",dt);
         return(creat(file,0644));
      }
   }
}
#endif

void
ttwrite(fd,buff,n)
int fd;
char *buff;
int n;
{
   char *bend;

   bend = buff + n;			/* end of data to write */
   while(buff < bend) {
      if(bptr >= buffer + BUFSIZE) {	/* flush buffer */
#ifdef vms
         if(termout) {
	    sys$qiow(0,fd,IO$_WRITEVBLK|IO$M_NOFORMAT,0,0,0,
						buffer,BUFSIZE,0,0,0,0);
	 } else {
	    (void)write(fd,buffer,BUFSIZE);
	 }
#else
	 (void)write(fd,buffer,BUFSIZE);
#endif
	 bptr = buffer;
      }
      *bptr++ = *buff++;
   }
}

void
ttflush(fd)
int fd;
{
#ifdef vms
   char dt[20];				/* Device Type */
#endif

   if(bptr == buffer) return;

#ifdef vms
   if(termout) {
      sys$qiow(0,fd,IO$_WRITEVBLK|IO$M_NOFORMAT,0,0,0,buffer,
							bptr-buffer,0,0,0,0);
   } else {
      if(ttygets(g_tty,"DT",dt,20) > 0) {	/* DT is defined */
	 if(strcmp(dt,"imagen") == 0) {		/* and it's "imagen" */
	    while(bptr < buffer + BUFSIZE) *bptr++ = IM_NOOP;
	 }
      }
      (void)write(fd,buffer,bptr - buffer);
   }
#else
   (void)write(fd,buffer,bptr - buffer);
#endif
   bptr = buffer;
}

/*****************************************************/
/*
 * Make a filename. Replace trailing "XXXXXX" by unique characters,
 * and prepend any temporary directory path
 */
char *
get_full_name(file)
char file[];			/* name of file */
{
   char buff[100],			/* Temporary buffer */
	*ptr;
   int i;

   if(file[0] != '/' && *(ptr = print_var("temp_dir")) != '\0') {
      (void)sprintf(buff,"%s%s",ptr,file);
   } else {
      (void)strcpy(buff,file);
   }
   i = strlen(buff);
   if(i > 6 && !strcmp(&(buff[i-6]),"XXXXXX")) {
/*
 * We used to use mktemp, but on some machines we only get 26 names,
 * so use our own
 */
      (void)strcpy(file,make_temp(buff));
   } else {
      (void)strcpy(file,buff);
   }
   return(file);
}
