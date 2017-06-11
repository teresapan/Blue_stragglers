#include "copyright.h"
/*
 * This does logical operations on (pointers to) vectors.
 *
 * vec_and(v1,v2,ans)	ans = v1 && v2
 * vec_or(v1,v2,ans)	ans = v1 || v2
 * vec_eq(v1,v2,ans)	ans = v1 == v2
 * vec_ne(v1,v2,ans)	ans = v1 != v2
 * vec_ge(v1,v2,ans)	ans = v1 >= v2
 * vec_gt(v1,v2,ans)	ans = v1 >  v2
 * vec_ternary(v1,v2,v3,ans)	ans = v1 ? v2 : v3
 */
#include <stdio.h>
#include "options.h"
#include "vectors.h"
#include "sm_declare.h"

extern int sm_verbose;			/* be chatty? */
  
/***********************************************************/

void
vec_and(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be anded */
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

   if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(!val) {
	 for(i = 0;i < dimen;i++) {
	    f[i] = 0;
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f[i] = f[i] && 1;
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f, *f2 = v2->vec.f;
      
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s && %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 f1[i] = f1[i] && f2[i];
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

void
vec_or(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be ored */
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

   if(v2->dimen == 1) {		/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(val) {
	 for(i = 0;i < dimen;i++) {
	    f[i] = 1;
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f[i] = f[i] || 0;
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f, *f2 = v2->vec.f;

      if(v1->dimen != v2->dimen) {
	 msg_2s("%s || %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      for(i = 0;i < dimen;i++) {
	 f1[i] = f1[i] || f2[i];
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

void
vec_eq(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be compared in equality */
       *ans;			/* the answer */
{
   LONG *a;				/* == ans->vec.l */
   int dimen,				/* dimension of longer vector */
       i,
       made_ans_vec;		/* have I made ans->vec.l yet? */
   VECTOR *temp;		/* used in switching v1 and v2 */

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
   }

   if((v1->type == VEC_FLOAT && v2->type == VEC_LONG) ||
      (v1->type == VEC_LONG  && v2->type == VEC_FLOAT)) {
      vec_convert_float(v1);
      vec_convert_float(v2);
   }

   ans->dimen = v1->dimen;
   ans->type = VEC_LONG;
   ans->name = v1->name;

   if(v1->type == VEC_LONG) {
      made_ans_vec = 0;
      ans->vec = v1->vec;
   } else {
      if(vec_malloc(ans,v1->dimen) < 0) {
	 msg_2s("Can't make logical vector comparing %s and %s",
		v1->name,v2->name);
	 vec_free(v2);
	 ans->vec.l = v1->vec.l;	/* best chance */
	 return;
      }
      made_ans_vec = 1;
   }

   a = ans->vec.l;			/* unalias */

   if(v2->dimen == 1) {			/* scalar and vector */
      dimen = v1->dimen;		/* unalias */

      if(v1->type != v2->type) {	/* different types can't be equal */
	 for(i = 0;i < dimen;i++) {
	    a[i] = 0;
	 }
      } else if(v1->type == VEC_FLOAT) {
	 REAL *f = v1->vec.f;
	 REAL val = v2->vec.f[0];
	 for(i = 0;i < dimen;i++) {
	    a[i] = (f[i] == val) ? 1 : 0;
	 }
      } else if(v1->type == VEC_LONG) {
	 LONG *l = v1->vec.l;
	 LONG val = v2->vec.l[0];
	 for(i = 0;i < dimen;i++) {
	    a[i] = (l[i] == val) ? 1 : 0;
	 }
      } else if(v1->type == VEC_STRING) {
	 char **s = v1->vec.s.s;
	 char *val = v2->vec.s.s[0];
	 for(i = 0;i < dimen;i++) {
	    a[i] = (strcmp(s[i],val) == 0) ? 1 : 0;
	 }
      } else {
	 fprintf(stderr, "Impossible vector type in vec_eq\n");
	 abort();
      }
   } else {				/* both vectors */
      dimen = v2->dimen;		/* unalias */
	 
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s == %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      if(v1->type != v2->type) {	/* different types can't be equal */
	 for(i = 0;i < v1->dimen;i++) {
	    a[i] = 0;
	 }
      } else if(v1->type == VEC_FLOAT) {
	 REAL *f1 = v1->vec.f, *f2 = v2->vec.f;
	 for(i = 0;i < dimen;i++) {
	    a[i] = (f1[i] == f2[i]) ? 1 : 0;
	 }
      } else if(v1->type == VEC_LONG) {
	 LONG *l1 = v1->vec.l, *l2 = v2->vec.l;
	 for(i = 0;i < dimen;i++) {
	    a[i] = (l1[i] == l2[i]) ? 1 : 0;
	 }
      } else if(v1->type == VEC_STRING) {
	 char **s1 = v1->vec.s.s, **s2 = v2->vec.s.s;
	 for(i = 0;i < dimen;i++) {
	    a[i] = (strcmp(s1[i],s2[i]) == 0) ? 1 : 0;
	 }
      } else {
	 fprintf(stderr, "Impossible vector type in vec_eq\n");
	 abort();
      }
   }
   if(made_ans_vec) {
      vec_free(v1);
   }
   vec_free(v2);

#if 0
   vec_convert_float(ans);		/* for now XXX */
#endif
     
   return;
}

/***********************************************************/

void
vec_ne(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be compared in inequality */
       *ans;			/* the answer */
{
   int dimen = ans->dimen,
       i;

   vec_eq(v1, v2, ans);

   if(ans->type == VEC_FLOAT) {
      REAL *f = ans->vec.f;
      for(i = 0; i < dimen; i++) {
	 f[i] = f[i] ? 0 : 1;
      }
   } else if(ans->type == VEC_LONG) {
      LONG *l = ans->vec.l;
      for(i = 0; i < dimen; i++) {
	 l[i] = !l[i];
      }
   } else {
      msg("vec_ne: you cannot get here\n");
   }
}

/***********************************************************/

void
vec_ge(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be compared */
       *ans;			/* the answer */
{
   int i,
       exchange = 0;		/* have vectors been exchanged ? */
   VECTOR *temp;		/* used in exchanging v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 1) {	/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    f[i] = val >= f[i];
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f[i] = f[i] >= val;
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f, *f2 = v2->vec.f;
      if(v1->dimen != v2->dimen) {
	 msg_2s("%s >= or <= %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    f1[i] = f2[i] >= f1[i];
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f1[i] = f1[i] >= f2[i];
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

void
vec_gt(v1,v2,ans)
VECTOR *v1,*v2,			/* vectors to be compared */
       *ans;			/* the answer */
{
   int i,
       exchange = 0;		/* have vectors been exchanged ? */
   VECTOR *temp;			/* used in exchanging v1 and v2 */

   if(check_vec(v1,v2,ans) < 0) return;

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v2->dimen == 1) {	/* scalar and vector */
      int dimen = v1->dimen;		/* unalias */
      REAL *f = v1->vec.f;
      REAL val = v2->vec.f[0];
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    f[i] = val > f[i];
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f[i] = f[i] > val;
	 }
      }
   } else {			/* both vectors */
      int dimen = v2->dimen;		/* unalias */
      REAL *f1 = v1->vec.f, *f2 = v2->vec.f;

      if(v1->dimen != v2->dimen) {
	 msg_2s("%s > or < %s: vectors' dimensions differ",v1->name,v2->name);
	 yyerror("show where");
      }
      if(exchange) {
	 for(i = 0;i < dimen;i++) {
	    f1[i] = f2[i] > f1[i];
	 }
      } else {
	 for(i = 0;i < dimen;i++) {
	    f1[i] = f1[i] > f2[i];
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

void
vec_ternary(log,v1,v2,ans)
VECTOR *log,			/* logical vector */
       *v1,*v2,			/* vectors to be compared */
       *ans;			/* the answer */
{
   int i,
       exchange = 0;		/* have vectors been exchanged ? */
   VECTOR *temp;		/* used in exchanging v1 and v2 */

   if((v1->type == VEC_FLOAT && v2->type == VEC_LONG) ||
      (v1->type == VEC_LONG  && v2->type == VEC_FLOAT)) {
      vec_convert_float(v1);
      vec_convert_float(v2);
   }

   vec_convert_long(log);

   if(v1->type != v2->type || log->type != VEC_LONG) {
      if(v1->type != v2->type) {
	 msg_2s("Vectors %s and %s have different types in ?: statement",
	     v1->name,v2->name);
      } else {
	 msg_1s("Logical vector %s is not LONG",log->name);
      }
      yyerror("show where");
      vec_free(log);
      vec_free(v1);
      vec_free(v2);
      (void)vec_value(ans,0.0);
      return;
   }

   if(v1->dimen < v2->dimen) {	/* make v1 >= v2 */
      temp = v1;
      v1 = v2;
      v2 = temp;
      exchange = 1;
   }

   if(v1->dimen != v2->dimen && v2->dimen != 1) {
      msg_1s("%s ? ",log->name);
      msg_2s("%s : %s: vectors' dimensions differ",v1->name,v2->name);
      yyerror("show where");
      v1->dimen = v2->dimen;
   }
/*
 * Prepare answer vector
 */
   ans->type = v1->type;
   ans->name = v1->name;

   if(log->dimen == 1) {	/* This is a trivial case */
      if((log->vec.l[0] && exchange) || (!log->vec.l[0] && !exchange)) {
	 if(v2->dimen == 1) {
	    if(ans->type == VEC_FLOAT) {
	       int dimen = v1->dimen;		/* unalias */
	       REAL *f = v1->vec.f;
	       REAL val = v2->vec.f[0];
	       for(i = 0;i < dimen;i++) {
		  f[i] = val;
	       }
	    } else if(ans->type == VEC_LONG) {
	       int dimen = v1->dimen;		/* unalias */
	       LONG *l = v1->vec.l;
	       LONG val = v2->vec.l[0];
	       for(i = 0;i < dimen;i++) {
		  l[i] = val;
	       }
	    } else {
	       int dimen = v1->dimen;		/* unalias */
	       char **s = v1->vec.s.s;
	       char *val = v2->vec.s.s[0];
	       for(i = 0;i < dimen;i++) {
		  (void)strcpy(s[i],val);
	       }
	    }
	 } else {
	    temp = v1;		/* v1 will be returned as ans */
	    v1 = v2;
	    v2 = temp;		/* must be able to free v2 */
	 }
      }
      vec_free(log);
      vec_free(v2);
      if(ans->type == VEC_FLOAT) {
	 ans->vec.f = v1->vec.f;
      } else if(ans->type == VEC_LONG) {
	 ans->vec.l = v1->vec.l;
      } else {
	 ans->vec.s = v1->vec.s;
      }
      ans->dimen = v1->dimen;
      return;
   }
/*
 * logical is a vector
 */
   if(log->dimen != v1->dimen) {
      if(v1->dimen == 1) {		/* both v1 and v2 are scalars */
	 ans->dimen = log->dimen;
	 if(ans->type == VEC_FLOAT) {
	    int dimen = log->dimen;	/* unalias */
	    LONG *l = log->vec.l;
	    REAL *f, val1 = v1->vec.f[0], val2 = v2->vec.f[0];

	    if(vec_malloc(ans,log->dimen) < 0) { /* make a vector for answer */
	       msg("Can't get storage in ?: statement\n");
	       ans = v1;
	       return;
	    }
	    f = ans->vec.f;
	    
	    if(exchange) {
	       for(i = 0;i < dimen;i++) {
		  f[i] = l[i] ? val2 : val1;
	       }
	    } else {
	       for(i = 0;i < dimen;i++) {
		  f[i] = l[i] ? val1 : val2;
	       }
	    }
	      
	    vec_free(log);
	 } else if(ans->type == VEC_LONG) {
	    int dimen = log->dimen;	/* unalias */
	    LONG *l, val1 = v1->vec.l[0], val2 = v2->vec.l[0];
	    
	    l = ans->vec.l = log->vec.l; /* put answer in log */
	    if(exchange) {
	       for(i = 0;i < dimen;i++) {
		  l[i] = l[i] ? val2 : val1;
	       }
	    } else {
	       for(i = 0;i < dimen;i++) {
		  l[i] = l[i] ? val1 : val2;
	       }
	    }
	 } else {
	    int dimen = log->dimen;	/* unalias */
	    char **a;
	    LONG *l = log->vec.l;
	    char *val1 = v1->vec.s.s[0], *val2 = v2->vec.s.s[0];

	    if(vec_malloc(ans,log->dimen) < 0) { /* make a vector for answer */
	       msg("Can't get storage in ?: statement\n");
	       ans = v1;
	       return;
	    }
	    a = ans->vec.s.s;
	    if(exchange) {
	       for(i = 0;i < dimen;i++) {
		  (void)strcpy(a[i], l[i] ? val2 : val1);
	       }
	    } else {
	       for(i = 0;i < dimen;i++) {
		  (void)strcpy(a[i],l[i] ? val1 : val2);
	       }
	    }
	      
	    vec_free(log);
	 }
	 vec_free(v1);
	 vec_free(v2);
	 return;
      } else {
	 msg("Logical has wrong dimension in ? : statement");
	 yyerror("show where");
	 if(log->dimen > v1->dimen) {
	    log->dimen = v1->dimen;
	 }
      }
   }
/*
 * We now know that we are going to return the answer in v1
 */
   ans->vec = v1->vec;
   ans->dimen = v1->dimen;
   
   if(v1->type == VEC_FLOAT) {
      int dimen = log->dimen;		/* unalias */
      REAL *f1 = v1->vec.f;
      LONG *l = log->vec.l;
      if(v2->dimen == 1) {
	 REAL val2 = v2->vec.f[0];
	 if(exchange) {
	    for(i = 0;i < dimen;i++) {
	       if(l[i]) {
		  f1[i] = val2;
	       }
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       if(!l[i]) {
		  f1[i] = val2;
	       }
	    }
	 }
      } else {
	 REAL *f2 = v2->vec.f;
	 if(exchange) {
	    for(i = 0;i < dimen;i++) {
	       if(l[i]) {
		  f1[i] = f2[i];
	       }
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       if(!l[i]) {
		  f1[i] = f2[i];
	       }
	    }
	 }
      }
   } else if(v1->type == VEC_LONG) {
      int dimen = log->dimen;		/* unalias */
      LONG *l = log->vec.l, *f1 = v1->vec.l;
      if(v2->dimen == 1) {
	 LONG val2 = v2->vec.l[0];
	 if(exchange) {
	    for(i = 0;i < dimen;i++) {
	       if(l[i]) {
		  f1[i] = val2;
	       }
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       if(!l[i]) {
		  f1[i] = val2;
	       }
	    }
	 }
      } else {
	 LONG *f2 = v2->vec.l;
	 if(exchange) {
	    for(i = 0;i < dimen;i++) {
	       if(l[i]) {
		  f1[i] = f2[i];
	       }
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       if(!l[i]) {
		  f1[i] = f2[i];
	       }
	    }
	 }
      }
   } else {
      int dimen = log->dimen;		/* unalias */
      LONG *l = log->vec.l;
      char **f1 = v1->vec.s.s;
      
      if(v2->dimen == 1) {
	 char *val = v2->vec.s.s[0];
	 if(exchange) {
	    for(i = 0;i < dimen;i++) {
	       if(l[i]) {
		  (void)strcpy(f1[i],val);
	       }
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       if(!l[i]) {
		  (void)strcpy(f1[i],val);
	       }
	    }
	 }
      } else {
	 char **f2 = v2->vec.s.s;
	 if(exchange) {
	    for(i = 0;i < dimen;i++) {
	       if(l[i]) {
		  (void)strcpy(f1[i],f2[i]);
	       }
	    }
	 } else {
	    for(i = 0;i < dimen;i++) {
	       if(!l[i]) {
		  (void)strcpy(f1[i],f2[i]);
	       }
	    }
	 }
      }
   }
   vec_free(log);
   vec_free(v2);
}
