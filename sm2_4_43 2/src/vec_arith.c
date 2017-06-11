#include "copyright.h"
/*
 * This file contains a number of functions to do arithmetic and string
 * operations on vectors.
 *
 *    function			finds
 *
 * vec_abs(v1,ans)		ans = abs(v1)
 * vec_acos(v1,ans)		ans = acos(v1)
 * vec_add(v1,v2,ans)		ans = v1 + v2
 * vec_asin(v1,ans)		ans = asin(v1)
 * vec_atan(v1,ans)		ans = atan(v1)
 * vec_concat(v1,v2,ans)	ans = concatenation of v1 and v2
 * vec_cos(v1,ans)		ans = cos(v1)
 * vec_divide(v1,v2,ans)	ans = v1 / v2
 * vec_mod(v1,v2,ans)		ans = v1 % v2
 * vec_bitand(v1,v2,ans)	ans = v1 & v2
 * vec_bitor(v1,v2,ans)		ans = v1 | v2
 * vec_gamma(v1,v2,ans)		ans = gamma(v1,v2)
 * vec_hist(v1,v2,ans)		ans = histogram of v1 at v2
 * vec_interp(v1,v2,ans)	ans = IMAGE(v1, v2)
 * vec_int(v1,ans)		ans = (int)v1
 * vec_lg(v1,ans)		ans = lg(v1)
 * vec_ln(v1,ans)		ans = ln(v1)
 * vec_multiply(v1,v2,ans)	ans = v1 * v2
 * vec_power(v1,v2,ans)		ans = v1^v2
 * vec_random(v1,ans)		ans = random number with same dimension as v1
 * vec_set_image(v1,v2,vals)    IMAGE(v1,v2) = vals
 * vec_sin(v1,ans)		ans = sin(v1)
 * vec_sqrt(v1,ans)		ans = sqrt(v1)
 * vec_subtract(v1,v2,ans)	ans = v1 - v2
 * vec_tan(v1,ans)		ans = tan(v1)
 */
#include <stdio.h>
#include <math.h>
#include "options.h"
#include "vectors.h"
#include "sm_declare.h"

static int check_vec_long P_ARGS(( VECTOR *, VECTOR *, VECTOR * ));

extern int sm_interrupt,		/* Have we seen a ^C? */
  	   sm_verbose;			/* shall we be chatty? */

/***********************************************************/
/*
 * Take the absolute value of a vector
 */
void
vec_abs(v1,ans)
VECTOR *v1,			/* vector to be absed */
       *ans;			/* the answer */
{
   int i;
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */
   
   for(i = 0;i < dimen;i++) {
      f[i] = fabs(f[i]);
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Take the acos of a vector
 */
void
vec_acos(v1,ans)
VECTOR *v1,			/* vector to be acosed */
       *ans;			/* the answer */
{
   int i,
       first_err = 1;			/* First arithmetic error? */
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */
   
   for(i = 0;i < dimen;i++) {
      if(fabs(f[i]) <= 1) {
	 f[i] = acos(f[i]);
      } else {
	 if(sm_verbose) {
	    if(sm_verbose > 1 || first_err) {
	       first_err = 0;
	       msg_1f("Attempt to take acos of %g",f[i]);
	       yyerror("show where");
	    }
	 }
	 if(sm_interrupt) break;
	 f[i] = 0;
      }
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/********************************************************/
/*
 * Add two vectors
 */
void
vec_add(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be added */
       *ans;			/* the answer */
{
   char buff[2*VEC_STR_SIZE];
   int i,
       exchange = 0,			/* have vectors been exchanged ? */
       first_err = 1;			/* First error? */
   VECTOR *temp;			/* used in switching v1 and v2 */

   if((v1->type == VEC_FLOAT && v2->type == VEC_LONG) ||
      (v1->type == VEC_LONG  && v2->type == VEC_FLOAT)) {
      vec_convert_float(v1);
      vec_convert_float(v2);
   }

   if(v1->type != v2->type) {
      msg_2s("Vectors %s and %s have different types",v1->name,v2->name);
      yyerror("show where");
      vec_free(v1);
      vec_free(v2);
      if(ans != NULL) {
	 ans->type = VEC_NULL;
	 vec_free(ans);
	 ans->name = "(empty)";
      }
      return;
   }
   
   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = !exchange;
   } else if(v2->dimen == 1) {		/* add scalar and vector */
      if(v1->type == VEC_FLOAT) {
	 int dimen = v1->dimen;		/* unalias */
	 REAL *f = v1->vec.f;
	 REAL val = v2->vec.f[0];
	 
	 for(i = 0;i < dimen;i++) {
	    f[i] += val;
	 }
      } else if(v1->type == VEC_LONG) {
	 int dimen = v1->dimen;		/* unalias */
	 LONG *l = v1->vec.l;
	 LONG val = v2->vec.l[0];
	 
	 for(i = 0;i < dimen;i++) {
	    l[i] += val;
	 }
      } else {
	 if(exchange) {
	    for(i = 0;i < v1->dimen;i++) {
	       (void)sprintf(buff,"%s%s",v2->vec.s.s[0],v1->vec.s.s[i]);
	       (void)strncpy(v1->vec.s.s[i],buff,VEC_STR_SIZE-1);
	       if((int)strlen(buff) >= VEC_STR_SIZE) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg_1d("Truncating %dth element of string vector",i);
		     msg_2s(" \"%s + %s\"",v2->name,v1->name);
		     yyerror("show where");
		  }
		  v1->vec.s.s[i][VEC_STR_SIZE-1] = '\0';
	       }
	    }
	 } else {
	    for(i = 0;i < v1->dimen;i++) {
	       (void)sprintf(buff,"%s%s",v1->vec.s.s[i],v2->vec.s.s[0]);
	       (void)strncpy(v1->vec.s.s[i],buff,VEC_STR_SIZE-1);
	       if((int)strlen(buff) >= VEC_STR_SIZE) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg_1d("Truncating %dth element of string vector",i);
		     msg_2s(" \"%s + %s\"",v1->name,v2->name);
		     yyerror("show where");
		  }
		  v1->vec.s.s[i][VEC_STR_SIZE-1] = '\0';
	       }
	    }
	 }
      }
   } else {			/* both vectors */
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s + %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      if(v1->type == VEC_FLOAT) {
	 int dimen = v2->dimen;		/* unalias */
	 REAL *f1 = v1->vec.f, *f2 = v2->vec.f;
	 
	 for(i = 0;i < dimen;i++) {
	    f1[i] += f2[i];
	 }
      } else if(v1->type == VEC_LONG) {
	 int dimen = v2->dimen;		/* unalias */
	 LONG *l1 = v1->vec.l, *l2 = v2->vec.l;
	 
	 for(i = 0;i < dimen;i++) {
	    l1[i] += l2[i];
	 }
      } else {
	 if(exchange) {
	    for(i = 0;i < v2->dimen;i++) {
	       (void)sprintf(buff,"%s%s",v2->vec.s.s[i],v1->vec.s.s[i]);
	       (void)strncpy(v1->vec.s.s[i],buff,VEC_STR_SIZE-1);
	       if((int)strlen(buff) >= VEC_STR_SIZE) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg_1d("Truncating %dth element of string vector",i);
		     msg_2s(" \"%s + %s\"",v2->name,v1->name);
		     yyerror("show where");
		  }
		  v1->vec.s.s[i][VEC_STR_SIZE-1] = '\0';
	       }
	    }
	 } else {
	    for(i = 0;i < v2->dimen;i++) {
	       (void)sprintf(buff,"%s%s",v1->vec.s.s[i],v2->vec.s.s[i]);
	       (void)strncpy(v1->vec.s.s[i],buff,VEC_STR_SIZE-1);
	       if((int)strlen(buff) >= VEC_STR_SIZE) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg_1d("Truncating %dth element of string vector",i);
		     msg_2s(" \"%s + %s\"",v1->name,v2->name);
		     yyerror("show where");
		  }
		  v1->vec.s.s[i][VEC_STR_SIZE-1] = '\0';
	       }
	    }
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = v1->type;
   ans->name = v1->name;
   ans->vec = v1->vec;

   return;
}

/***********************************************************/
/*
 * Take the asin of a vector
 */
void
vec_asin(v1,ans)
VECTOR *v1,			/* vector to be asined */
       *ans;			/* the answer */
{
   int i,
       first_err = 1;			/* First arithmetic error? */
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      if(fabs(f[i]) <= 1) {
	 f[i] = asin(f[i]);
      } else {
	 if(sm_verbose) {
	    if(sm_verbose > 1 || first_err) {
	       first_err = 0;
	       msg_1f("Attempt to take asin of %g",f[i]);
	       yyerror("show where");
	    }
	 }
	 if(sm_interrupt) break;
	 f[i] = 0;
      }
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Take the atan of a vector
 */
void
vec_atan(v1,ans)
VECTOR *v1,			/* vector to be ataned */
       *ans;			/* the answer */
{
   int i;
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      f[i] = atan(f[i]);
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Take the atan of a pair of vectors, x and y
 */
void
vec_atan2(v1,v2,ans)
VECTOR *v1,*v2,				/* vectors to be atan2ed */
       *ans;				/* the answer */
{
   int i,
       exchange = 0;			/* have vectors been exchanged ? */
   VECTOR *temp;			/* used in exchangeing v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(!exchange) {
	 for(i = 0;i < dimen;i++) {
	    f[i] = atn2(f[i],val);
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f[i] = atn2(val,f[i]);
	 }
      }
   } else {				/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      REAL *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg_2s("atan2(%s,%s): vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 if(!exchange) {
	    f1[i] = atn2(f1[i],f2[i]);
	 } else {
	    f1[i] = atn2(f2[i],f1[i]);
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/********************************************************/
/*
 * bitwise and of vectors
 */
void
vec_bitand(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be anded */
       *ans;			/* the answer */
{
   int i;
   VECTOR *temp;			/* used in switching v1 and v2 */

   if(check_vec_long(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      LONG *l = v1->vec.l;
      LONG l2 = v2->vec.l[0];
      for(i = 0;i < dimen;i++) {
	 l[i] &= l2;
      }
   } else {				/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      LONG *l1 = v1->vec.l;
      LONG *l2 = v2->vec.l;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s & %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 l1[i] &= l2[i];
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_LONG;
   ans->name = v1->name;
   ans->vec.l = v1->vec.l;

#if 0
   vec_convert_float(ans);		/* XXX */
#endif
     
   return;
}

/********************************************************/
/*
 * bitwise or of vectors
 */
void
vec_bitor(v1,v2,ans)
VECTOR *v1,*v2,				/* vectors to be ored */
       *ans;				/* the answer */
{
   int i;
   VECTOR *temp;			/* used in switching v1 and v2 */

   if(check_vec_long(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      LONG *l = v1->vec.l;
      LONG l2 = v2->vec.l[0];
      for(i = 0;i < dimen;i++) {
	 l[i] |= l2;
      }
   } else {				/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      LONG *l1 = v1->vec.l;
      LONG *l2 = v2->vec.l;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s | %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 l1[i] |= l2[i];
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_LONG;
   ans->name = v1->name;
   ans->vec.l = v1->vec.l;

#if 0
   vec_convert_float(ans);		/* XXX */
#endif

   return;
}

/********************************************************/
/*
 * bit shift of vectors
 */
void
vec_bitshift(v1, v2, ans, left)
VECTOR *v1,*v2,				/* shift v1 by v2 */
       *ans;				/* the answer */
int left;				/* shift left? */
{
   int i,
       exchange = 0;			/* have vectors been exchanged ? */
   VECTOR *temp;			/* used in switching v1 and v2 */

   if(check_vec_long(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {		/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      LONG *l = v1->vec.l;
      LONG l2 = v2->vec.l[0];
      if(!exchange) {
	 if(left) {
	    for(i = 0;i < dimen;i++) {
	       l[i] <<= l2;
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       l[i] >>= l2;
	    }
	 }
      } else {
	 if(left) {
	    for(i = 0;i < dimen;i++) {
	       l[i] = l2 << l[i];
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       l[i] = l2 >> l[i];
	    }
	 }
      }
   } else {				/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      LONG *l1 = v1->vec.l;
      LONG *l2 = v2->vec.l;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s & %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      if(!exchange) {
	 if(left) {
	    for(i = 0;i < dimen;i++) {
	       l1[i] <<= l2[i];
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       l1[i] >>= l2[i];
	    }
	 }
      } else {
	 if(left) {
	    for(i = 0;i < dimen;i++) {
	       l1[i] = l2[i] << l1[i];
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       l1[i] = l2[i] >> l1[i];
	    }
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_LONG;
   ans->name = v1->name;
   ans->vec.l = v1->vec.l;

   return;
}

/********************************************************/
/*
 * Concatenate two vectors
 */
void
vec_concat(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be added */
       *ans;			/* the answer */
{
   int i,
       old_v1_dimen;			/* dimension of v1 before reallocing */

   if((v1->type == VEC_FLOAT && v2->type == VEC_LONG) ||
      (v1->type == VEC_LONG  && v2->type == VEC_FLOAT)) {
      vec_convert_float(v1);
      vec_convert_float(v2);
   }

   if(v1->type != v2->type) {
      msg_2s("Vectors %s and %s have different types in CONCAT",
	     v1->name,v2->name);
      yyerror("show where");
      vec_free(v1);
      vec_free(v2);
      (void)vec_value(ans,0.0);
      return;
   }

   old_v1_dimen = v1->dimen;
   if(vec_realloc(v1,v1->dimen + v2->dimen) < 0) {
      msg("realloc returns NULL in vec_concat()\n");
      ans->dimen = 0;
      ans->type = VEC_NULL;
      ans->name = "(failed concat)";
      return;
   }
   ans->vec = v1->vec;
   ans->dimen = v1->dimen;
   ans->type = v1->type;
   ans->name = "(concated)";
   if(ans->type == VEC_FLOAT) {
      int dimen = v2->dimen;		/* unalias */
      REAL *fa = ans->vec.f;
      REAL *f2 = v2->vec.f;
      for(i = 0;i < dimen;i++) {
	 fa[i + old_v1_dimen] = f2[i];
      }
   } else if(ans->type == VEC_LONG) {
      int dimen = v2->dimen;		/* unalias */
      LONG *la = ans->vec.l;
      LONG *l2 = v2->vec.l;
      for(i = 0;i < dimen;i++) {
	 la[i + old_v1_dimen] = l2[i];
      }
   } else if(ans->type == VEC_STRING) {
      for(i = 0;i < v2->dimen;i++) {
	 (void)strncpy(ans->vec.s.s[i + old_v1_dimen],v2->vec.s.s[i],
		       VEC_STR_SIZE);
      }
   } else {
      msg_2s("Vectors %s and %s have ",v1->name,v2->name);
      msg_1d("unknown type %d in CONCAT\n",ans->type);
      vec_free(v1);
      (void)vec_value(ans,0.0);
   }
   vec_free(v2);
   return;
}

/***********************************************************/
/*
 * Take the cosine of a vector
 */ 
void
vec_cos(v1,ans)
VECTOR *v1,			/* vector to be cosined */
       *ans;			/* the answer */
{
   int i;
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      f[i] = cos(f[i]);
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/********************************************************/
/*
 * divide two vectors
 */
void
vec_divide(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be divided */
       *ans;			/* the answer */
{
   int i,
       exchange = 0,		/* have vectors been exchanged ? */
       first_err = 1;			/* First arithmetic error? */
   VECTOR *temp;		/* used in exchangeing v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    if(f[i] == 0) {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to divide by zero");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f[i] = 0;
	    } else {
	       f[i] = val/f[i];
	    }
	 }
      } else {
	 if(val == 0) {
	    if(sm_verbose) {
	       if(sm_verbose > 1 || first_err) {
		  first_err = 0;
		  msg("Attempt to divide by zero");
		  yyerror("show where");
	       }
	    }
	    for(i = 0;i < dimen;i++) {
	       f[i] = 0;
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       f[i] /= val;
	    }
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      REAL *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s / %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 if(!exchange) {
	    if(f2[i] != 0) {
	       f1[i] /= f2[i];
	    } else {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to divide by zero");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f1[i] = 0;
	    }
	 } else {
	    if(f1[i] != 0) {
	       f1[i] = f2[i]/f1[i];
	    } else {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to divide by zero");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f1[i] = 0;
	    }
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/********************************************************/
/*
 * Return vec1 % vec2
 */
void
vec_mod(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be processed */
       *ans;			/* the answer */
{
   int i,
       i1, i2,				/* v1 and v2 converted to int */
       exchange = 0,			/* have vectors been exchanged ? */
       first_err = 1;			/* First arithmetic error? */
   VECTOR *temp;			/* used in exchangeing v1 and v2 */

   if(check_vec_long(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      LONG *f = v1->vec.l;
      i2 = v2->vec.l[0] < 0 ? -(-v2->vec.l[0] + 0.5) : v2->vec.l[0] + 0.5;
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    i1 = f[i] < 0 ? -(-f[i] + 0.5) : f[i] + 0.5;
	    if(i1 == 0) {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to reduce modulo zero");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f[i] = 0;
	    } else {
	       v1->vec.l[i] = i2%i1;
	    }
	 }
      } else {
	 if(i2 == 0) {
	    if(sm_verbose) {
	       if(sm_verbose > 1 || first_err) {
		  first_err = 0;
		  msg("Attempt to reduce modulo zero");
		  yyerror("show where");
	       }
	    }
	    for(i = 0;i < dimen;i++) {
	       f[i] = 0;
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       i1 = f[i] < 0 ? -(-f[i] + 0.5) : f[i] + 0.5;
	       f[i] = i1%i2;
	    }
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      LONG *f1 = v1->vec.l;
      LONG *f2 = v2->vec.l;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s % %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 i1 = f1[i] < 0 ? -(-f1[i] + 0.5) : f1[i] + 0.5;
	 i2 = f2[i] < 0 ? -(-f2[i] + 0.5) : f2[i] + 0.5;
	 if(!exchange) {
	    if(i2 != 0) {
	       f1[i] = i1%i2;
	    } else {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to reduce modulo zero");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f1[i] = 0;
	    }
	 } else {
	    if(i1 != 0) {
	       f1[i] = i2%i1;
	    } else {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to reduce modulo zero");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f1[i] = 0;
	    }
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_LONG;
   ans->name = v1->name;
   ans->vec.l = v1->vec.l;
   return;
}

/***********************************************************/
/*
 * Take the exponential of a vector
 */
void
vec_exp(v1,ans)
VECTOR *v1,			/* vector to be exponentiated */
       *ans;			/* the answer */
{
   int i;
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      f[i] = exp(f[i]);
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/*****************************************************************************/
/*
 * return the incomplete gamma function gamma(v1,v2)
 */
static int
gamma_error(first_err, a, x)
int first_err;
double a, x;
{
   if(a <= 0 || x <= 0) {
      if(sm_verbose) {
	 if(sm_verbose > 1 || first_err) {
	    first_err = 0;
	    msg("attempt to call gamma(a,x) with ");
	    if(a <= 0) {
	       if(x <= 0) {
		  msg("a and x <= 0");
	       } else {
		  msg("a <= 0");
	       }
	    } else {
	       msg("x <= 0");
	    }
	    yyerror("show where");
	 }
      }
      return(1);
   } else {
      return(0);
   }
}


void
vec_gamma(v1,v2,ans)
VECTOR *v1,*v2,				/* we want gamma(v1,v2) */
       *ans;				/* the answer */
{
   int i,
       exchange = 0,			/* have vectors been exchanged ? */
       first_err = 1;			/* First arithmetic error? */
   VECTOR *temp;			/* used in exchangeing v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {		/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    if(gamma_error(first_err,val,f[i])) {
	       first_err = 0;

	       if(sm_interrupt) break;
	       f[i] = 0;
	    } else {
	       f[i] = sm_gamma(val,f[i]);
	    }
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    if(gamma_error(first_err,f[i],val)) {
	       first_err = 0;
	       
	       if(sm_interrupt) break;
	       f[i] = 0;
	    } else {
	       f[i] = sm_gamma(f[i],val);
	    }
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      REAL *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg_2s("gamma(%s,%s): vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      if(!exchange) {
	 for(i = 0;i < dimen;i++) {
	    if(gamma_error(first_err,f1[i],f2[i])) {
	       first_err = 0;

	       if(sm_interrupt) break;
	       f1[i] = 0;
	    } else {
	       f1[i] = sm_gamma(f1[i],f2[i]);
	    }
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    if(gamma_error(first_err,f2[i],f1[i])) {
	       first_err = 0;

	       if(sm_interrupt) break;
	       f1[i] = 0;
	    } else {
	       f1[i] = sm_gamma(f2[i],f1[i]);
	    }
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Convert v1 into a histogram at points v2. That is, return a histogram
 * where ans[i] is the number of elements in v1 lying in
 * [ (v2[i-1]+v2[i])/2 , (v2[i]+v2[i+1])/2 ]
 */
void
vec_hist(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors giving coordinates */
       *ans;			/* the answer */
{
   REAL mid_val;			/* value at middle of a bin */
   int i,j,k;
   int dimen1, dimen2;			/* unaliased from v[12]->dimen */
   REAL *f1, *f2;			/* unaliased from v[12]->vec.f */

   if(check_vec(v1,v2,ans) < 0) return;

   dimen1 = v1->dimen; dimen2 = v2->dimen;
   f1 = v1->vec.f; f2 = v2->vec.f;

   for(i = 1;i < dimen2;i++) {
      if(f2[i] < f2[i-1]) {
         msg("Second vector is not sorted in vec_hist\n");
	 ans = v1;
	 vec_free(v2);
	 return;
      }
   }

   sort_flt(f1,dimen1);

   i = k = 0;
   for(j = 0;j < dimen2 - 1;j++) {
      mid_val = (f2[j] + f2[j+1])/2;	/* midpoint of bin */
      for(;i < dimen1;i++) {
         if(f1[i] > mid_val) {
            f2[j] = i - k;
	    k = i;
	    break;
	 }
      }
      if(i == dimen1) break;
   }
   f2[j++] = dimen1 - k;
   for(;j < dimen2;j++) {
      f2[j] = 0;
   }
	    
   vec_free(v1);
   ans->dimen = v2->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v2->vec.f;
   return;
}

/***********************************************************/
/*
 * Interpolate into the current IMAGE at points (v1,v2)
 */
void
vec_interp(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors giving coordinates */
       *ans;			/* the answer */
{
   int exchange = 0,		/* did I switch v1 and v2 ? */
       i;
   VECTOR *temp;		/* used in switching v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(!exchange) {
         for(i = 0;i < dimen;i++) {
	    f[i] = image_val(f[i],val);
         }
      } else {
         for(i = 0;i < dimen;i++) {
	    f[i] = image_val(val,f[i]);
         }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      REAL *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg("X and Y vectors are different lengths");
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 f1[i] = image_val(f1[i],f2[i]);
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Take the integral part of a vector
 */
void
vec_int(v1,ans)
VECTOR *v1,			/* vector to be cast to int */
       *ans;			/* the answer */
{
   vec_cast_long(v1, ans);
}

/***********************************************************/
/*
 * Take the lg of a vector
 */
void
vec_lg(v1,ans)
VECTOR *v1,			/* vector to be logged (base 10) */
       *ans;			/* the answer */
{
   int i,
       first_err = 1;			/* First arithmetic error? */
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      if(f[i] > 0) {
	 f[i] = log10(f[i]);
      } else {
	 if(sm_verbose) {
	    if(sm_verbose > 1 || first_err) {
	       first_err = 0;
	       msg("Attempt to take lg of non-positive number");
	       yyerror("show where");
	    }
	 }
	 f[i] = -1000;
	 if(sm_interrupt) break;
      }
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Take the lg of a vector
 */
void
vec_ln(v1,ans)
VECTOR *v1,			/* vector to be natural logged */
       *ans;			/* the answer */
{
   int i,
       first_err = 1;			/* First arithmetic error? */
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      if(f[i] > 0) {
	 f[i] = log(f[i]);
      } else {
	 if(sm_verbose) {
	    if(sm_verbose > 1 || first_err) {
	       first_err = 0;
	       msg("Attempt to take ln of non-positive number");
	       yyerror("show where");
	    }
	 }
	 f[i] = -1000;
	 if(sm_interrupt) break;
      }
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Multiply two vectors
 */
void
vec_multiply(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be added */
       *ans;			/* the answer */
{
   int i;
   VECTOR *temp;		/* used in switching v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      for(i = 0;i < dimen;i++) {
	 f[i] *= val;
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      REAL *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s * %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 f1[i] *= f2[i];
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Calculate a power of a vector
 */
void
vec_power(v1,v2,ans)
VECTOR *v1,			/* vector to be exponentiated */
       *v2,			/* indexes to be used */
       *ans;			/* the answer */
{
   REAL base,			/* we want base^index */
	 index;
   int dimen,
       exchange = 0,		/* have vectors been exchanged ? */
       first_err = 1,		/* First arithmetic error? */
       i,
       integral;		/* is index integral ? */
   VECTOR *temp;		/* used in exchanging v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {		/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      REAL *f = v1->vec.f;
      dimen = v1->dimen;		/* unalias */
      if(exchange) {
	 base = v2->vec.f[0];
	 for(i = 0;i < dimen;i++) {
	    index = f[i];
	    if(base < 0 && fabs(index) - (int)fabs(index) != 0) {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to take fractional power of -ve number");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f[i] = 0;
	    } else {
	       f[i] = pow(base,index);
	    }
	 }
      } else {
	 index = v2->vec.f[0];
	 integral = (fabs(index) - (int)fabs(index) == 0) ? 1 : 0;
	 for(i = 0;i < dimen;i++) {
	    base = f[i];
	    if(!integral && base < 0) {
	       if(sm_verbose) {
		  if(sm_verbose > 1 || first_err) {
		     first_err = 0;
		     msg("Attempt to take fractional power of -ve number");
		     yyerror("show where");
		  }
	       }
	       if(sm_interrupt) break;
	       f[i] = 0;
	    } else {
	       f[i] = pow(base,index);
	    }
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      REAL *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s ** %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 if(exchange) {
	    base = f2[i];
	    index = f1[i];
	 } else {
	    index = f2[i];
	    base = f1[i];
	 }
	 if(base < 0 && fabs(index) - (int)fabs(index) != 0) {
	    if(sm_verbose) {
	       if(sm_verbose > 1 || first_err) {
		  first_err = 0;
		  msg("Attempt to take fractional power of -ve number");
		  yyerror("show where");
	       }
	    }
	    if(sm_interrupt) break;
	    f1[i] = 0;
	 } else {
	    f1[i] = pow(base,index);
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Return a vector of random numbers in [0,1]
 */
static int init_random = 0;		/* is the random number generator
					   initialised? */

void
vec_random(dimen,v1)
int dimen;
VECTOR *v1;				/* the values */
{
   int i;
   REAL *f;				/* unalias v1->vec.f */
   
   if(make_anon_vector(v1,dimen,VEC_FLOAT) == -1) {
      return;
   }

   if(!init_random) {
      sm_srand(time((TIME_T *)NULL));
      init_random = 1;
   }

   f = v1->vec.f;
   for(i = 0;i < dimen;i++) {
      f[i] = sm_rand();
   }
}

void
set_random(seed)
long seed;
{
   sm_srand(seed);
   init_random = 1;
}

/***********************************************************/
/*
 * Take the sine of a vector
 */
void
vec_sin(v1,ans)
VECTOR *v1,			/* vector to be sined */
       *ans;			/* the answer */
{
   int i;
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      f[i] = sin(f[i]);
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Take the sqrt of a vector
 */
void
vec_sqrt(v1,ans)
VECTOR *v1,			/* vector to be sqrted */
       *ans;			/* the answer */
{
   int i,
       first_err = 1;			/* First arithmetic error? */
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */

   for(i = 0;i < dimen;i++) {
      if(f[i] >= 0) {
	 f[i] = sqrt(f[i]);
      } else {
	 if(sm_verbose) {
	    if(sm_verbose > 1 || first_err) {
	       first_err = 0;
	       msg("Attempt to take sqrt of negative number");
	       yyerror("show where");
	    }
	 }
         if(sm_interrupt) break;
	 f[i] = 0;
      }
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/********************************************************/
/*
 * subtract two vectors
 */
void
vec_subtract(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be subtracted */
       *ans;			/* the answer */
{
   int i,
       exchange = 0;		/* have vectors been exchanged ? */
   VECTOR *temp;		/* used in exchangeing v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 0) {			/* swap them back, to free longer */
      temp = v1;
      v1 = v2;
      v2 = temp;
   } else if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    f[i] = val - f[i];
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f[i] -= val;
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      REAL *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s - %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    f1[i] = f2[i] - f1[i];
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f1[i] -= f2[i];
	 }
      }
   }
   vec_free(v2);
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/***********************************************************/
/*
 * Take the tangent of a vector
 */
void
vec_tan(v1,ans)
VECTOR *v1,			/* vector to be tangented */
       *ans;			/* the answer */
{
   int i;
   int dimen = v1->dimen;		/* unalias */
   REAL *f;

   if(check_vec(v1,(VECTOR *)NULL,ans) < 0) return;
   f = v1->vec.f;			/* unalias */
   
   for(i = 0;i < dimen;i++) {
      f[i] = tan(f[i]);
   }
   ans->dimen = v1->dimen;
   ans->type = VEC_FLOAT;
   ans->name = v1->name;
   ans->vec.f = v1->vec.f;
   return;
}

/**********************************************************/
/*
 * Check if v1 (and optionally v2) are arithmetic
 */
int
check_vec(v1,v2,ans)
VECTOR *v1,*v2,*ans;
{
   if(v1->type == VEC_LONG) {
      vec_convert_float(v1);
   }
   if(v2 != NULL && v2->type == VEC_LONG) {
      vec_convert_float(v2);
   }

   if(v1->type != VEC_FLOAT || (v2 != NULL && v2->type != VEC_FLOAT)) {
      if(v1->type != VEC_FLOAT) {
	 msg_1s("Vector %s is not arithmetic",v1->name);
	 yyerror("show where");
      }
      if(v2 != NULL && v2->type != VEC_FLOAT) {
	 msg_1s("Vector %s is not arithmetic",v2->name);
	 yyerror("show where");
      }
      vec_free(v1);
      if(v2 != NULL) vec_free(v2);
      if(ans != NULL) {
	 ans->type = VEC_NULL;
	 vec_free(ans);
	 ans->name = "(empty)";
      }
      return(-1);
   }
   return(0);
}

/*****************************************************************************/
/*
 * Check if v1 (and optionally v2) are LONG
 */
static int
check_vec_long(v1,v2,ans)
VECTOR *v1,*v2,*ans;
{
   if(v1->type == VEC_FLOAT) {
      vec_convert_long(v1);
   }
   if(v2 != NULL && v2->type == VEC_FLOAT) {
      vec_convert_long(v2);
   }

   if(v1->type != VEC_LONG || (v2 != NULL && v2->type != VEC_LONG)) {
      if(v1->type != VEC_LONG) {
	 msg_1s("Vector %s is not arithmetic",v1->name);
	 yyerror("show where");
      }
      if(v2 != NULL && v2->type != VEC_LONG) {
	 msg_1s("Vector %s is not arithmetic",v2->name);
	 yyerror("show where");
      }
      vec_free(v1);
      if(v2 != NULL) vec_free(v2);
      if(ans != NULL) {
	 ans->type = VEC_NULL;
	 vec_free(ans);
	 ans->name = "(empty)";
      }
      return(-1);
   }
   return(0);
}
