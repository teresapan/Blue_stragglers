#include "copyright.h"
/*
 * Given the name of vectors x1 and y1, fit a spline to x1 and y1
 * and interpolate to the position of vector x1, leaving the answers on  y2
 */
#include <stdio.h>
#include "options.h"
#include "vectors.h"
#include "sm_declare.h"

static int initialise_spline(),
   	   interpolate_spline();
  
int
spline(name_x1,name_y1,name_x2,name_y2)
char *name_x1,
     *name_y1,
     *name_x2,
     *name_y2;
{
   REAL *k;				/* spline coefficients */
   int dimen,				/* dimension of x1, y1 and k arrays */
       i;
   VECTOR *x1,*y1,			/* initial x and y arrays */
	  *x2,				/* x where splines are desired */
	  *y2;				/* y after splining */
/*
 * First get x1 and x2, then check if they have the same dimensions
 */
   if((x1 = get_vector_ptr(name_x1)) == NULL ||
				(y1 = get_vector_ptr(name_y1)) == NULL) {
      return(-1);
   }

   if(check_vec(x1, y1, (VECTOR *)NULL) < 0) {
      return(-1);
   }

   if((dimen = x1->dimen) != y1->dimen) {
      msg_2s("Vectors %s and %s have different dimensions\n",name_x1,name_y1);
      return(-1);
   }
   if(dimen <= 2) {
      msg("Dimension of splined arrays must exceed 2\n");
      return(-1);
   }
/*
 * Check that x1 is monotonic increasing, and that x2 exists
 */
   for(i = 0;i < dimen - 1;i++) {
      if(x1->vec.f[i] >= x1->vec.f[i+1]) {
	 msg_1s("%s is not monotonic increasing\n",name_x1);
	 return(-1);
      }
   }
   if((x2 = get_vector_ptr(name_x2)) == NULL) {
      return(-1);
   }
   
   if(check_vec(x2, (VECTOR *)NULL, (VECTOR *)NULL) < 0) {
      return(-1);
   }
/*
 * All is well so far, so get storage for spline coefficients and vector y2
 */
   if((k = (REAL *)malloc((unsigned)dimen*sizeof(REAL))) == NULL) {
      msg("Can't malloc space for k in spline\n");
      return(-1);
   }
   if((y2 = make_vector(name_y2,x2->dimen,VEC_FLOAT)) == NULL) {
      return(-1);
   }
/*
 * Finally call Jim's routines to do all the work, using a natural spline
 */
   if(initialise_spline(x1->vec.f,y1->vec.f,k,dimen,0.,0.) == 0) {
      for(i = 0;i < x2->dimen;i++) {
	 (void)interpolate_spline(x2->vec.f[i],&y2->vec.f[i],x1->vec.f,
				  y1->vec.f,k,dimen);
      }
   }
   
   free((char *)k);
   return(0);
}

/*********************************************/
/*
 * Spline package for interpolation.
 *
 * Written by Jim Gunn, modified by Robert Lupton
 */
#include <stdio.h>
#define d12 .0833333333
static float ddb, dde;		/* second derivatives at each end of spline */

/*****************************************************/
/*
 * returns index i of first element of t >= x
 */
static int
circe(x,t,n)
double x;
register REAL *t ;
int n ;
{
   register int lo, hi, mid ;
   REAL tm ;
   lo = 0 ;
   hi = n-1 ;
   if ( x >= t[0] && x <= t[n-1])  {
      while ( hi - lo > 1 ) {
	 mid = ( lo + hi )/2. ;
	 tm = t[mid] ;
	 if ( x < tm ) hi = mid ;
	 else  lo = mid ;
      }
      return(hi);
   } else {
      return(-1);
   }
}

/**********************************************************/
/*
 * solves tridiagonal linear system . Diag elts mii=ai, subdiag mii-1=bi,
 * superdiag mii+1=ci.
 * it is understood that b0 and cn-1 are zero, but are not referenced.
 * f is rhs, x soln, may be same array; all arrays are trashed
 *
 * Re-written; JEG version incorrect.
 */
static void
tridi(a,b,c,f,x,n)
register REAL *a, *b, *c, *f, *x;
int n ;
{
   int i;

   c[0] /= a[0];
   for(i=1;i < n;i++) {
      c[i] /= (a[i] - b[i]*c[i-1]);
   }
   f[0] /= a[0];
   for(i=1;i < n;i++) {
      f[i] = (f[i] - b[i]*f[i-1])/(a[i] - b[i]*c[i-1]);
   }
   x[n - 1] = f[n - 1];
   for(i = n - 2;i >= 0;i--) {
      x[i] = f[i] - c[i]*x[i+1];
   }
}

/*********************************************************/
/*
 * sets up spline derivative array k for a given x and y array of length
 * n points, n-1 intervals, for given estimates for the second derivatives
 * at the endpoints, q2b and q2e; "natural" boundary conditions for q2b=q2e=0
 */
static int
initialise_spline(x,y,k,n,q2b,q2e)
REAL *x, *y, *k ;
int n ;
double q2b, q2e ;
{
   REAL hio, hip, dio, dip ;
   register REAL *a, *b, *c, *f;
   int i, ip ;

   ddb = q2b; dde = q2e;		/* save end second derivatives */
   if((a = (REAL *)malloc(4*(unsigned)n*sizeof(REAL))) == NULL) {
      msg("Can't malloc space in initialise_spline\n");
      return(-1);
   }
   b = a + n ;
   c = b + n ;
   f = c + n ;

   hio = 0. ;
   dio = 0. ;

/*	BIG BUG in borland compiler: ip is used before its computation !!! */
#ifdef	__BORLANDC__
	for( i=0, ip = 1 ; i < n ; i++, ip++ ) {
      hip = ( ip < n ? x[ip] - x[i] : 0. ) ;
      dip = ( ip < n ? (y[ip] - y[i])/hip : 0. ) ;
      b[i] = ( ip < n ? hip : hio ) ;
      a[i] = 2.*( hip + hio ) ;
      c[i] = ( i > 0 ? hio : hip ) ;
      f[i] = 3.*(hip*dio + hio*dip ) ;
      if( i == 0 ) f[0] = 3.* hip * dip  - hip * hip * q2b * 0.5 ;
      else if ( i == n-1 )
	 f[n-1] = 3.* hio* dio + hio* hio* q2e* 0.5  ;
      dio = dip ;
      hio = hip ;
   }
#else
	for( i=0 ; i < n ; i++ ) {
      hip = ((ip = i+1) < n ? x[ip] - x[i] : 0. ) ;
      dip = ( ip < n ? (y[ip] - y[i])/hip : 0. ) ;
      b[i] = ( ip < n ? hip : hio ) ;
      a[i] = 2.*( hip + hio ) ;
      c[i] = ( i > 0 ? hio : hip ) ;
      f[i] = 3.*(hip*dio + hio*dip ) ;
      if( i == 0 ) f[0] = 3.* hip * dip  - hip * hip * q2b * 0.5 ;
      else if ( i == n-1 )
	 f[n-1] = 3.* hio* dio + hio* hio* q2e* 0.5  ;
      dio = dip ;
      hio = hip ;
   }
#endif
   tridi(a,b,c,f,k,n) ;
   free((char *)a);
   return(0);
}

/*********************************************************/
/*
 * general spline evaluator; xyk are the x,y, and derivative arrays, of
 * dimension n, dx the argument = (x-xi-1), m the index of the next GREATER
 * abscissa.
 */
static int
interpolate_spline(z,val,x,y,k,n)
double z;
REAL *val;			/* value at z */
register REAL *x, *y, *k;
int n;
{
   REAL h, t, d, a, b,
	 dx ;
   int m ;

   if((m = circe( z, x, n)) < 0) {	/* out of bounds */
      if(z < x[0]) {
	 dx = z - x[0];
	 *val = y[0] + dx*(k[0] + 0.5*dx*ddb);
      } else {
	 dx = z - x[n-1];
	 *val = y[n-1] + dx*(k[n-1] + 0.5*dx*dde);
      }
      return(-1);
   }
   dx = z - x[m-1];
   
   h = x[m] - x[m-1];
   d = (y[m] - y[m-1])/h;
   t = dx/h;
   a = (k[m-1] - d)*(1 - t);
   b = (k[m]-d) *t ;
   *val = t*y[m] + (1 - t)*y[m-1] + h*t*(1 - t)*(a - b);
   return(0);
}
