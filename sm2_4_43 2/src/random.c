/*
 * SM's random number generators are based on the ones suggested by
 * Marsaglia and Zaman in Computers in Physics; Bill Press pointed them
 * out to us.
 *
 * Note that we do not claim a copyright on this code
 */
#include <stdio.h>
#include "options.h"
#include "sm_declare.h"

#define RNMAX (1 - 1.2e-7)		/* ~ largest float less than 1 */

static unsigned int x = 521288629;
static unsigned int y = 362436069; 
static unsigned int z = 16163801;
static unsigned int c = 1;
static unsigned int n = 1131199299;

/*
 * This generator uses 32-bit arithmetic. On most machines, an int is
 * 4 bytes so the ANDing with 0xffffffff is a NOP, but in that case the
 * optimiser should catch it. Increasingly, long is a 64-bit type so
 * it is not used here.
 */
static void
mzranset(xs,ys,zs,ns)
int xs, ys, zs, ns;
{
   x = xs; y = ys; z = zs; n = ns;
   c = (y > z) ? 1 : 0;
}

static float
mzran()
{
   unsigned int s;
   float val;

   if(y > x + c) {
      s = y - (x + c);
      c = 0;
   } else {
      s = y - (x + c) - 18;
      c = 1;
   }
   s &= 0xffffffff;
   x = y;
   y = z;
   z = s;
   n = 69069*n + 1013904243;
   n &= 0xffffffff;

   s = (s + n) & 0xffffffff;		/* here's a 32-bit random number */
   
#if defined(__STDC__) 
   val = s*(1/(float)(0xfffffffeU));
#else
   val = s*(1/4294967294.0);
#endif
   return(val > RNMAX ? RNMAX : val);
}

/*****************************************************************************/
/*
 * Set the random seed 
 */
void
sm_srand(seed)
long seed;
{
   srand(seed);
   mzranset((int)seed,rand(),rand(),rand());
}

float
sm_rand()
{
   return(mzran());
}
