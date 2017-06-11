/*
 * ftruncate for SYS5. Actually, this isn't possible in general, but
 * we'll try anyway.
 */
#include <stdio.h>
#include "options.h"
#include "sm_declare.h"

#if defined(SYS_V) || defined(NO_FTRUNCATE)
#if defined(vms)

int
ftruncate(fd,len)
int fd;
long len;
{
   return(lseek(fd,0,0L));		/* better than nowt */
}
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(F_FREESP)
/* 
 * This relies on the UNDOCUMENTED F_FREESP argument to 
 * fcntl(2), which truncates the file so that it ends at the
 * position indicated by fl.l_start.
 *
 * If you can't find it listed in your <fcntl.h> or 
 * <sys/fcntl.h>, chances are it isn't implemented in
 * your kernel.
 */

int
ftruncate(fd, length)
int fd;					/* file descriptor */
off_t length;				/* length to set file to */
{
   struct flock fl;
   struct stat filebuf;
   
   if(fstat(fd, &filebuf) < 0) return(-1);
   
   if(filebuf.st_size < length) {	/* extend file length */
      if((lseek(fd,(length - 1),0)) < 0) {
	 return(-1);
      }
      
      if(write(fd,"",1) != 1) {		/* write a "0" byte */
	 return(-1);
      }
   } else {				/* truncate length */
      fl.l_whence = 0;
      fl.l_len = (off_t) 0;
      fl.l_start = length;
      fl.l_type = F_WRLCK;		/* write lock on file space */

      if(fcntl(fd,F_FREESP,&fl) < 0) {
	 return(-1);
      }
   }
   
   return(0);
}
#else
#if defined(F_CHSIZE)			/* works under (some?) Xenixes */
ftruncate(fd,len)
int fd;
off_t len;
{
   return(fcntl(fd,F_CHSIZE,len));
}
#else
ftruncate(fd,len)
int fd;
long len;
{
   return(lseek(fd,len,0L));		/* better than nowt */
}
#endif					/* F_CHSIZE */
#endif					/* F_FREESP */
#endif					/* vms */
#endif					/* SYS_V || NO_FTRUNCATE*/
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif
