/*
 * This code has been modified from the code by `Pjotr' in comp.sources.unix
 * Volume 18. Modifications consisted of making it deal with pairs of
 * real vectors rather than a complex struct, and putting it all together
 * into a single file (with associated static declarations).
 *
 * We do not claim any copyright on this file.
 */
#include "options.h"
#include <stdio.h>
#include <math.h>
#include "sm.h"
#include "sm_declare.h"

static REAL *W_r = NULL,*W_i;		/* real and imag. parts of phases */
static int nfactors = 0,
	   radix(),
  	   W_init();
static void fourier(),
  	    join(),
  	    split();

/*
 * Forward or reverse Fast Fourier Transform on the n samples of complex
 * array in (represented as two real arrays, in_r[] and in_i[]).
 * The result is placed in out_r and out_i. The number of samples, n,
 * is arbitrary. The W-factors are calculated in advance.
 */
int
fft(in_r,in_i,out_r,out_i,n,dir)
REAL *in_r,*in_i;
REAL *out_r,*out_i;
int n;
int dir;				/* +1: FFT, -1: IFFT */
{
   int i;
   
   if(W_init(n) == -1) {
      return(-1);
   }

   if(dir < 0) {			/* inverse transform */
      fourier(in_r,in_i,out_r,out_i,n);
   } else {				/* forward transform */
      for (i = 0; i < n; i++) {
	 in_i[i] = -in_i[i];
      }
      
      fourier(in_r,in_i,out_r,out_i,n);
      
      for(i = 0; i < n; i++) {
	 out_i[i] = -out_i[i]/n;
	 out_r[i] = out_r[i]/n;
      }
   }   
   return(0);
}

/*
 * Recursive (reverse) complex fast Fourier transform on the n
 * complex samples of array in, with the Cooley-Tukey method.
 * The result is placed in out.  The number of samples, n, is arbitrary.
 * The algorithm costs O (n * (r1 + .. + rk)), where k is the number
 * of factors in the prime-decomposition of n (also the maximum
 * depth of the recursion), and ri is the i-th primefactor.
 */
static void
fourier(in_r,in_i,out_r,out_i,n)
REAL *in_r,*in_i;			/* real and imaginary parts of in[] */
REAL *out_r,*out_i;			/* real and imaginary parts of out[] */
int n;
{
   int r;
   
   if((r = radix(n)) < n) {
      split(in_r,in_i,out_r,out_i,r,n/r);
   }
   join(in_r,in_i,out_r,out_i,n/r,n);
}

/*
 * Give smallest possible radix for n samples.
 * Determines (in a rude way) the smallest primefactor of n.
 */
static int
radix(n)
int n;
{
   int r;
   
   if(n < 2) {
      return(1);
   }
   
   for(r = 2; r < n; r++) {
      if(n % r == 0) break;
   }
   return(r);
}

/*
 * Split array in of r * m samples in r parts of each m samples,
 * such that in [i] goes to out [(i % r) * m + (i / r)].
 * Then call fourier() for each part of out, so the r recursively
 * transformed parts will go back to in.
 */
static void
split(in_r,in_i,out_r,out_i,r,m)
REAL *in_r,*in_i;
REAL *out_r,*out_i;
register int r, m;
{
   register int k, s, i, j;
   
   for(k = 0, j = 0; k < r; k++) {
      for(s = 0, i = k; s < m; s++, i += r, j++) {
	 out_r[j] = in_r[i];
	 out_i[j] = in_i[i];
      }
   }
   
   for(k = 0; k < r; k++, out_r += m, out_i += m, in_r += m, in_i += m) {
      fourier(out_r,out_i,in_r,in_i,m);
   }
}

/*
 * Sum the n / m parts of each m samples of in to n samples in out.
 * 		   r - 1
 * Out [j] becomes  sum  in [j % m] * W (j * k).  Here in is the k-th
 * 		   k = 0   k	       n		 k
 * part of in (indices k * m ... (k + 1) * m - 1), and r is the radix.
 * For k = 0, a complex multiplication with W (0) is avoided.
 */
static void
join(in_r,in_i,out_r,out_i,m,n)
REAL *in_r,*in_i;
REAL *out_r,*out_i;
register int m,n;
{
   register int i, j, k, jk, s;
   
   for(s = 0; s < m; s++) {
      for(j = s; j < n; j += m) {
	 out_r[j] = in_r[s];
	 out_i[j] = in_i[s];
	 for(i = s + m, jk = j; i < n; i += m, jk += j) {
	    k = (jk*(nfactors/n))%nfactors;
	    out_r[j] += in_r[i]*W_r[k] - in_i[i]*W_i[k];
	    out_i[j] += in_r[i]*W_i[k] + in_i[i]*W_r[k];
	 }
      }
   }
}

/*
 * W_init puts Wn ^ k (= e ^ (2pi * i * k / n)) in W_factors [k], 0 <= k < n.
 * If n is equal to nfactors then nothing is done, so the same W_factors
 * array can used for several transforms of the same number of samples.
 * Notice the explicit calculation of sines and cosines, an iterative approach
 * introduces substantial errors.
 */
static int
W_init (n)
int n;
{
   REAL a;
   int k;
   
   if(n == nfactors) {
      return(0);
   }
   
   if(nfactors != 0) {
      if(W_r != NULL) {
	 free((char *)W_r);
	 W_r = NULL;
      }
   }
   if((nfactors = n) == 0) {
      return(0);
   }
   if((W_r = (REAL *)malloc(2*(unsigned)n*sizeof(REAL))) == NULL) {
      return(-1);
   }
   W_i = W_r + n;

   for(k = 0; k < n; k++) {
      a = 2*PI*k/n;
      W_r[k] = cos(a);
      W_i[k] = sin(a);
   }
   
   return(0);
}
