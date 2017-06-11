#include "copyright.h"
/*
 * This subroutine classifies a string into integers, floats, parts of
 * floats, and words (defined as anything else). Parts of floats are defined
 * as a float, up to the +- in the exponent. These are needed if [+-] delimits
 * tokens.
 *
 * The definition of integer and float (written in LEX) is:
 *
 * D  [0-9]
 * E  [Ee][-+]?{D}+
 *
 * [-+]?{D}+               return(1);	integer
 *
 * [-+]?{D}+"."{D}*({E})?  |
 * [-+]?{D}*"."{D}+({E})?  |
 * [-+]?{D}+{E}            return(2);	float
 *
 * [-+]?{D}+"."{D}*[Ee]    |
 * [-+]?{D}*"."{D}+[Ee]    |
 * [-+]?{D}+[Ee]           return(3);	part of float
 *
 * Anything else will return 0
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "sm_declare.h"

int
num_or_word(num)
char num[];
{
   register char  *ptr = num;
   register int   dig = 0;                   /* have we found any digits ? */

   if(*ptr == '-' || *ptr == '+') ptr++;     /* skip +- */

   if(*ptr == '\0') {                        /* only a sign : not a number */
      return(0);
   }

   if(isdigit(*ptr)) dig++;                  /* found a digit */
   while(isdigit(*ptr)) ptr++;               /* skip digits */
   
   if(*ptr == '\0') {                        /* num was integer */
      if(sizeof(LONG) == 4 && ptr - num > 9) { /* might not fit in an integer */
	 return(2);			     /* so call it a float */
      }
      return(1);
   } 
/*
 * should be float
 */
   if(*ptr == '.') {
      ptr++;
      if(isdigit(*ptr)) dig++;               /* found a digit */
      while(isdigit(*ptr)) ptr++;            /* skip digits */
   }

   if(*ptr == '\0') {                        /* a float without exponent */
      if(dig) {                              /* if it has at least 1 digit */
	 return(2);
      } else {                               /* no digits, so not float */
	 return(0);
      }
   } 
/*
 * should be float with exponent still to go
 */
   if(!dig) {				/* need digits to be a float */
      return(0);
   }
   
   if(*ptr != 'D' && *ptr != 'd' &&
      *ptr != 'E' && *ptr != 'e') {          /* not start of exponent */
      return(0);
   } 
   ptr++;
   if(*ptr == '\0') {			     /* part of float */
      return(3);
   }
   if(*ptr == '-' || *ptr == '+') ptr++;     /* skip +- */

   if(!isdigit(*ptr)) {                      /* badly formed exponent */
      return(0);
   } 
   while(isdigit(*ptr)) ptr++;               /* skip digits */

   if(*ptr != '\0') {                        /* something is left */
      return(0);
   } else {                                  /* it was a float */
      if(dig) {                              /* if it has at least 1 digit */
	 return(2);
      } else {                               /* no digits, so not float */
	 return(0);
      }
   } 
}
