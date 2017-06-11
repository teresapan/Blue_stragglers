#include <stdio.h>
#include <math.h>
#include "options.h"
#include "sm.h"
#include "sm_declare.h"

/*****************************************************************************/
/*
 * Gamma functions
 *
 * These functions come from netlib, and carry the copyright:
 *
 *  Cephes Math Library Release 2.0:  April, 1987
 *  Copyright 1985, 1987 by Stephen L. Moshier (moshier@world.std.com)
 *  Direct inquiries to 30 Frost Street, Cambridge, MA 02140
 *
 * When I did so, the reply was:
 *
 *  From moshier@world.std.com Wed Jul 10 15:00 EDT 1996
 *  Date: Wed, 10 Jul 1996 14:59:56 -0400 (EDT)
 *  From: Stephen L Moshier <moshier@world.std.com>
 *  To: Robert Lupton the Good <rhl@astro.Princeton.EDU>
 *
 *  Subject: Re: cephes
 *
 *  The cephes files in netlib are freely distributable.  The netlib
 *  people would appreciate a mention.
 *
 * They have been somewhat rewritten since then, so don't blame Stephen.
 *
 * Note that we claim no copyright over this material.
 */

/*****************************************************************************/
/*
 * Here's a log-gamma function, based on  Lanczos' version of Stirling's
 * formula (see, e.g. Press et al. --- whence this code cometh not)
 *
 * Should be good to ~ 1e-10 for all Re(z) > 0.
 */
static double
sm_lngamma(z)
double z;
{
   double sum;

   z--;

   sum=1.000000000190015 +
     76.18009172947146/(z + 1) + -86.50532032941677/(z + 2) +
       24.01409824083091/(z + 3) + -1.231739572450155/(z + 4) + 
	 0.1208650973867e-2/(z + 5) + -0.539523938495e-5/(z + 6);

   return((z + 0.5)*log(z + 5.5) - (z + 5.5) + log(2*PI)/2 + log(sum));
}

/*****************************************************************************/
/*
 *
 * Calculate the complemented incomplete gamma integral
 *
 * The function is defined by
 *
 *
 *  sm_gamma_c(a,x)   =   1 - sm_gamma(a,x)
 *
 *                            inf.
 *                              -
 *                     1       | |  -t  a-1
 *               =   -----     |   e   t   dt.
 *                    -      | |
 *                   | (a)    -
 *                             x
 *
 *
 * In this implementation both arguments must be positive.
 * The integral is evaluated by either a power series or
 * continued fraction expansion, depending on the relative
 * values of a and x.
 */
#define EPSILON	2.22045e-16		/* smallest double such that
					   1 + EPSILON != 1 */

static double
sm_gamma_c(a, x)
double a, x;
{
   double big = 4.503599627370496e15;
   double biginv =  2.22044604925031308085e-16;
   double ans, ax, c, yc, r, t, y, z;
   double pk, pkm1, pkm2, qk, qkm1, qkm2;
   
   if(x <= 0 ||  a <= 0) {
      return(1.0);
   } else if(x < 1.0 || x < a) {
      return(1.0 - sm_gamma(a,x));
   }
   
   ax = a*log(x) - x - sm_lngamma(a);
   if(ax < -MAX_POW) {			/* underflow */
      return(0.0);
   }
   ax = exp(ax);
   
   /* continued fraction */
   y = 1.0 - a;
   z = x + y + 1.0;
   c = 0.0;
   pkm2 = 1.0;
   qkm2 = x;
   pkm1 = x + 1.0;
   qkm1 = z * x;
   ans = pkm1/qkm1;
   
   do {
      c += 1.0;
      y += 1.0;
      z += 2.0;
      yc = y * c;
      pk = pkm1 * z  -  pkm2 * yc;
      qk = qkm1 * z  -  qkm2 * yc;
      if( qk != 0 ) {
	 r = pk/qk;
	 t = fabs( (ans - r)/r );
	 ans = r;
      } else {
	 t = 1.0;
      }
      pkm2 = pkm1;
      pkm1 = pk;
      qkm2 = qkm1;
      qkm1 = qk;
      if( fabs(pk) > big ) {
	 pkm2 *= biginv;
	 pkm1 *= biginv;
	 qkm2 *= biginv;
	 qkm1 *= biginv;
      }
   } while( t > EPSILON );

   return( ans * ax );
}

/*****************************************************************************/
/*
 *
 * Incomplete gamma integral
 *
 *
 * The function is defined by
 *
 *                                x
 *                                -
 *                       1       | |  -t  a-1
 *  sm_gamma(a,x)  =   -----     |   e   t   dt.
 *                      -      | |
 *                     | (a)    -
 *                              0
 *
 *
 * In this implementation both arguments must be positive.
 * The integral is evaluated by either a power series or
 * continued fraction expansion, depending on the relative
 * values of a and x.
 *
 * The power series is used for the left tail of incomplete gamma function:
 *
 *          inf.      k
 *   a  -x   -       x
 *  x  e     >   ----------
 *           -     -
 *          k=0   | (a+k+1)
 */
double
sm_gamma(a,x)
double a, x;
{
   double ans, ax, c, r;

   if( (x <= 0) || ( a <= 0) )
     return( 0.0 );
   
   if( (x > 1.0) && (x > a ) )
     return( 1.0 - sm_gamma_c(a,x) );
   
   /* Compute  x**a * exp(-x) / gamma(a)  */
   ax = a * log(x) - x - sm_lngamma(a);
   if(ax < -MAX_POW) {			/* underflow */
      return( 0.0 );
   }
   ax = exp(ax);
   
   /* power series */
   r = a;
   c = 1.0;
   ans = 1.0;
   
   do {
      r += 1.0;
      c *= x/r;
      ans += c;
   } while(c/ans > EPSILON);
   
   return(ans*ax/a);
}
