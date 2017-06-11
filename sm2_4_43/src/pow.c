#include "copyright.h"
/*
 * This is identical to the system pow() function.
 * The VMS version is incomplete for negative x. (e.g. pow(-2.0,2.0) = 0)
 */
#ifdef vms
#include <errno.h>
#include <math.h>

double
vms_pow(arg1,arg2)
double arg1, arg2;
{
   double temp;
   long l;
   
   if(arg1 <= 0.) {
      if(arg1 == 0.) {
	 if(arg2 <= 0.) errno = EDOM;
	 return(0.);
      }
      l = arg2;
      if(l != arg2) {
	 errno = EDOM;
	 return(0.);
      }
      temp = exp(arg2 * log(-arg1));
      if(l & 1)	temp = -temp;
      return(temp);
   }
   return(exp(arg2 * log(arg1)));
}
#else
/*
 * Include a symbol for picky linkers
 */
#  ifndef lint
      static void linker_func() { linker_func(); }
#  endif
#endif
