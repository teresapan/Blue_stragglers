#include "copyright.h"
/*
 * This file contains a number of functions to do misc. operations on vectors.
 *
 *    function			finds
 *
 * vec_subscript(vec,ind,v)	vec[ind] = v
 * vec_do(vec, x1, x2, dx)	vec[ind] = x1,x2,dx (implicit do loop)
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "plotsub/image.h"
#include "vectors.h"
#include "sm_declare.h"

extern int sm_interrupt,			/* Have we seen a ^C? */
  	   sm_verbose;			/* shall we be chatty? */

/***********************************************************/
/*
 * Find x[ind] = v (i.e. replace some elements of x with v)
 */
void
vec_subscript(x,ind,v)
VECTOR *x,*ind,*v;
{
   int first_err = 1;			/* the first error that we've found */
   int i;				/* index into i */
   int vi;				/* index into v */
   int xi;				/* index into x */

   if(ind->type == VEC_FLOAT) {
      vec_convert_long(ind);
   }

   if(ind->dimen != v->dimen && v->dimen != 1) {
      msg_2s("Vectors %s and %s have different sizes",
	     ind->name,v->name);
      yyerror("show where");
      vec_free(ind); vec_free(v);
      return;
   }
   if(ind->type != VEC_LONG) {
      msg_1s("Vector %s is not numeric",ind->name);
      yyerror("show where");
      vec_free(ind); vec_free(v);
      return;
   }
   if(x->type == VEC_NULL) {
      msg_1s("vectors %s has type NULL",x->name);
      vec_free(ind); vec_free(v);
      return;
   }
   if((x->type == VEC_STRING) + (v->type == VEC_STRING) == 1) {
      msg_1s("Only one of vectors %s, ",x->name);
      msg_1s("%s are string valued; not setting elements\n",v->name);
      vec_free(ind); vec_free(v);
      return;
   }
/*
 * OK, we can do this.
 */
   for(i = 0;i < ind->dimen;i++) {
      xi = ind->vec.l[i];
      vi = (v->dimen == 1 ? 0 : i);
      if(xi < 0 || xi >= x->dimen) {
	 msg_1d("Index %d ",xi);
	 msg_1s("for vector %s is out of range ",x->name);
	 msg_1d("0 - %d",x->dimen-1);
	 if(first_err) {
	    yyerror("show where");
	    first_err = 0;
	 } else {
	    msg("\n");
	 }
	 vec_free(ind); vec_free(v);
	 return;
      }
      if(x->type == VEC_STRING) {
	 (void)strncpy(x->vec.s.s[xi],v->vec.s.s[vi],VEC_STR_SIZE);
      } else if(x->type == VEC_LONG) {
	 if(v->type == VEC_FLOAT) {
	    x->vec.l[xi] = v->vec.f[vi];
	 } else {
	    x->vec.l[xi] = v->vec.l[vi];
	 }
      } else {
	 if(v->type == VEC_FLOAT) {
	    x->vec.f[xi] = v->vec.f[vi];
	 } else {
	    x->vec.f[xi] = v->vec.l[vi];
	 }
      }
   }
   vec_free(ind);
   vec_free(v);
}

/*****************************************************************************/

int
vec_do(ans, x1, x2, dx)
VECTOR *ans;
double x1, x2, dx;
{
   int dimension,		/* dimension of vector */
       i;
   
   dimension = (x2 - x1)/dx + 1.0001;
   if(dimension <= 0) {
      msg_1s("Vector %s ", ans->name);
      msg_1d("has dimension %d <= 0\n",dimension);
      return(-1);
   } else {
#if defined(__MSDOS__)
      int max_dimen = 16380;
#else
      int max_dimen = 10000000;
#endif
      if(dimension > max_dimen) { /* That must be wrong */
	 msg_1s("Vector %s ", ans->name);
	 msg_1d("has dimension %d ",dimension);
	 msg_1d("> %d\n",max_dimen);
	 return(-1);
      }
   }
   
   if(dx == 1 && x1 == (int)x1 && x2 == (int)x2) {
      ans->type = VEC_LONG;
   } else {
      ans->type = VEC_FLOAT;
   }
   
   if(vec_malloc(ans, dimension) < 0) {
      return(-1);
   }

   if(ans->type == VEC_LONG) {
      int ix1 = x1;
      for(i = 0;i < dimension;i++) {
	 ans->vec.l[i] = ix1 + i;
      }
   } else {
      for(i = 0;i < dimension;i++) {
	 ans->vec.f[i] = x1 + i*dx;
      }
   }

   return(0);
}
