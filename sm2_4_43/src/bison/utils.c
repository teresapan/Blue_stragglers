/*
 * Provide unprovided utility routines
 *
 * Added Robert Lupton March 1987
 */
#include <string.h>
#include "../options.h"
#ifdef vms
unlink(file)
char file[];
{
   return(delete(file));
}
#endif

void bcopy(orig,dest,nbyte)
Const Void *orig;			/* original location */
Void *dest;				/* final destination */
size_t nbyte;				/* number of bytes */
{
   (void)memcpy(dest,orig,nbyte);
}

#ifdef NO_MEMCPY
memcpy(dest,orig,nbyte)
register char *orig,			/* original location */
	      *dest;			/* final destination */
register unsigned int nbyte;		/* number of bytes */
{
   register char *end = orig + nbyte;
   
   if(dest < orig) {			/* start at beginning */
      while(orig < end) *dest++ = *orig++;
   } else {				/* start at end */
      dest += nbyte;
      while(end > orig) *--dest = *--end;
   }
}
#endif
