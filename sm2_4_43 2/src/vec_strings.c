#include "copyright.h"
/*
 * This file contains a number of functions to do string operations on vectors.
 *
 *    function			finds
 *
 * vec_index(v1,v2,ans)		ans = index of v2 in v1
 * vec_length(v1,ans)		ans = length_if_plotted(v1)
 * vec_sprintf(fmt,v1,ans)	sprintf(ans,fmt,v1)
 * vec_string(v1,ans)		ans = string(v1)
 * vec_strlen(v1,ans)		ans = strlen(v1)
 * vec_substr(str,i,n,ans)	ans = str[i] ... str[i + n - 1]
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "vectors.h"
#include "sm_declare.h"

extern int sm_interrupt,			/* Have we seen a ^C? */
  	   sm_verbose;			/* shall we be chatty? */

/***********************************************************/
/*
 * Return the starting index of string V2 in V1
 */
void
vec_index(v1,v2,ans)
VECTOR *v1,*v2,
       *ans;			/* the answer */
{
   int dimen;				/* size of ans */
   int i;
   int i1,i2;				/* indices for v1 and v2 */
   char *ptr1;				/* pointer to value of v1 */
   char *ptr2,*start2;			/* pointer and start of v2 */
   
   ans->type = VEC_NULL;

   if(v1->type != VEC_STRING || v2->type != VEC_STRING) {
      msg_1s("Vector %s is not string valued",
	     (v1->type != VEC_STRING ? v1->name : v2->name));
      yyerror("show where");
      vec_free(v1);
      vec_free(v2);
      return;
   }

   if(v1->dimen == 1) dimen = v2->dimen;
   else if(v2->dimen == 1) dimen = v1->dimen;
   else if(v1->dimen == v2->dimen) dimen = v1->dimen;
   else {
      msg_2s("Vectors %s and %s have different lengths",v1->name,v2->name);
      yyerror("show where");
      vec_free(v1);
      vec_free(v2);
      return;
   }

   ans->type = VEC_LONG;
   if(vec_malloc(ans,dimen) < 0) {
      msg_2s("Can't allocate space for index(%s,%s)\n",v1->name,v2->name);
      vec_free(v1);
      vec_free(v2);
      return;
   }
   ans->name = "(index)";

   for(i = i1 = i2 = 0;i < dimen;i++) {
      ptr1 = v1->vec.s.s[i1];
      ptr2 = start2 = v2->vec.s.s[i2];
      if(*ptr1 == '\0') {		/* null string; no match */
	 ans->vec.l[i] = -1;
      } else {
	 ans->vec.l[i] = -1;
	 while(*ptr1 != '\0') {
	    if(*ptr1++ == *ptr2++) {
	       if(*ptr2 == '\0') {	/* found a match */
		  ans->vec.l[i] = (ptr1 - v1->vec.s.s[i1]) - (ptr2 - start2);
		  break;
	       }
	    } else {
	       ptr1 -= (ptr2 - start2 - 1);
	       ptr2 = start2;
	    }
	 }
      }
      if(v1->dimen > 1) i1++;
      if(v2->dimen > 1) i2++;
   }

   vec_free(v1);
   vec_free(v2);
}

/***********************************************************/
/*
 * Return the lengths of a vector's elements
 */
void
vec_length(v1,ans)
VECTOR *v1,			/* vector to be measured */
       *ans;			/* the answer */
{
   int i;
   float fval;

   ans->type = VEC_NULL;

   if(v1->type != VEC_STRING) {
      msg_1s("Vector %s is not string valued",v1->name);
      yyerror("show where");
      vec_free(v1);
      vec_free(ans);
      ans->name = "(empty)";
      return;
   }

   ans->type = VEC_LONG;
   if(vec_malloc(ans,v1->dimen) < 0) {
      msg_1s("Can't allocate space for length(%s)\n",v1->name);
      vec_free(v1);
      return;
   }
   ans->name = "(length)";

   for(i = 0;i < v1->dimen;i++) {
       string_size(v1->vec.s.s[i],&fval,(float *)NULL,(float *)NULL);
       ans->vec.l[i] = fval;
   }

   vec_free(v1);
   return;
}

/***********************************************************/
/*
 * A restricted version of sprintf, allowing just one argument to be formatted
 */
void
vec_sprintf(fmt,v1,ans)
VECTOR *fmt,*v1,
       *ans;				/* the answer */
{
   char *aptr;				/* pointer to the answer string */
   char *fptr;				/* pointer to fmt string */
   char *fstart;			/* start of % part of string */
   int dimen;				/* size of the answer */
   int i;
   int fi,vi;				/* indices for fmt and v1 */

   ans->type = VEC_NULL;
   vec_free(ans);
   ans->name = "(empty)";
   
   if(fmt->type != VEC_STRING) {
      msg_1s("Vector %s is not string-valued",fmt->name);
      yyerror("show where");
      vec_free(fmt);
      vec_free(v1);
      return;
   }
   if(fmt->dimen == 1) dimen = v1->dimen;
   else if(v1->dimen == 1) dimen = fmt->dimen;
   else if(fmt->dimen == v1->dimen) dimen = v1->dimen;
   else {
      msg_2s("Vectors %s and %s have different lengths",fmt->name,v1->name);
      yyerror("show where");
      vec_free(fmt);
      vec_free(v1);
      return;
   }

   ans->type = VEC_STRING;
   if(vec_malloc(ans,dimen) < 0) {
      msg_2s("Can't allocate space for sprintf(%s,%s)\n",fmt->name,v1->name);
      vec_free(fmt);
      vec_free(v1);
      return;
   }
   ans->name = "(sprintf)";

   for(i = fi = vi = 0;i < dimen;i++) {
      aptr = ans->vec.s.s[i];
      fptr = fmt->vec.s.s[fi];
      while(*fptr != '\0') {
	 if(*fptr == '%') {
	    if(*(fptr + 1) == '%') {	/* a %% */
	       fptr++;
	    } else {
	       break;
	    }
	 }
	 *aptr++ = *fptr++;
      }
      if(*fptr == '%') {
	 *aptr = '\0';			/* terminate answer */
	 fstart = fptr;			/* start of format string */
	 fptr++;			/* skip the % */
	 if(*fptr == '-' || *fptr == '+' || *fptr == ' ' || *fptr == '#') {
	    fptr++;
	 }
	 while(isdigit(*fptr)) fptr++;
	 if(*fptr == '.') fptr++;
	 while(isdigit(*fptr)) fptr++;
	 if(*fptr == 'l') fptr++;
	 switch (*fptr) {
	  case '\0':
	    break;
	  case 'c': case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
	    if(v1->type != VEC_LONG) {
	       if(vi == 0) {
		  msg_1s("Vector %s is not an int", v1->name);
		  yyerror("show where");
	       }
	    } else {
	       sprintf(aptr,fstart, v1->vec.l[vi]);
	    }
	    break;
	  case 'e': case 'E': case 'f': case 'g': case 'G':
	    if(v1->type == VEC_LONG) {
	       if(vi == 0 && sm_verbose > 0) {
		  msg_1s("Vector %s is an int; converting to float", v1->name);
		  yyerror("show where");
	       }
	       sprintf(aptr,fstart,(float)v1->vec.l[vi]);
	    } else if(v1->type != VEC_FLOAT) {
	       if(vi == 0) {
		  msg_1s("Vector %s is not an int", v1->name);
		  yyerror("show where");
	       }
	    } else {
	       sprintf(aptr,fstart,v1->vec.f[vi]);
	    }
	    break;
	  case 's':
	    if(v1->type != VEC_STRING) {
	       if(vi == 0) {
		  msg_1s("Vector %s is not a string", v1->name);
		  yyerror("show where");
	       }
	    } else {
	       sprintf(aptr,fstart,v1->vec.s.s[vi]);
	    }
	    break;
	  case 't': case 'T':
	    if(v1->type != VEC_FLOAT && v1->type != VEC_LONG) {
	       if(fmt->dimen > 1 || i == 0) {
		  msg_1s("Vector %s is not a number", v1->name);
		  yyerror("show where");
	       }
	    } else {
	       char buff[40],*bptr = buff;
	       char c = *fptr;

	       *fptr = 'e';
	       if(v1->type == VEC_LONG) {
		  if(vi == 0 && sm_verbose > 0) {
		     msg_1s("Vector %s is an int; converting to float",
			    v1->name);
		     yyerror("show where");
		  }
		  sprintf(buff,fstart,(float)v1->vec.l[vi]);
	       } else {
		  sprintf(buff,fstart,v1->vec.f[vi]);
	       }
	       *fptr = c;
	       
	       while(*bptr != 'e') *aptr++ = *bptr++;
	       bptr++;			/* skip the 'e' */
	       
	       strcpy(aptr,"\\times 10^{");
	       while(*aptr != '\0') aptr++;

	       if(*bptr == '-') {
		  *aptr++ = '-'; bptr++;
	       } else if(*bptr == '+') {
		  bptr++;
	       }
	       while(*bptr == '0') bptr++;
	       if(!isdigit(*bptr)) bptr--; /* allow a single 0 to survive */

	       while(isdigit(*bptr)) *aptr++ = *bptr++;
	       *aptr++ = '}';

	       fptr++;
	       while(*fptr != '\0') *aptr++ = *fptr++;
	       *aptr = '\0';
	    }
	    break;
	  default:
	    if(fmt->dimen > 1 || i == 0) {
	       msg_2s("Unknown format: %s (%s)",fmt->vec.s.s[fi],fmt->name);
	       yyerror("show where");
	    }
	    break;
	 }
      }
      if(fmt->dimen > 1) fi++;
      if(v1->dimen > 1) vi++;
   }

   vec_free(fmt);
   vec_free(v1);
   return;
}

/***********************************************************/
/*
 * Convert a vector to a string
 */
void
vec_string(v1,ans)
VECTOR *v1,			/* vector to be converted */
       *ans;			/* the answer */
{
   int i;

   if(v1->type == VEC_STRING) {		/* already a string */
      ans->dimen = v1->dimen;
      ans->type = v1->type;
      ans->name = v1->name;
      ans->vec.s = v1->vec.s;
      return;
   }

   ans->type = VEC_STRING;
   if(vec_malloc(ans,v1->dimen) < 0) {
      msg_1s("Can't allocate space to convert vector %s to string\n",v1->name);
      ans->dimen = 0;
      ans->vec.s.s = NULL;
      ans->vec.s.s_s = NULL;
      ans->name = "(empty)";
      vec_free(v1);
      return;
   }

   if(v1->type == VEC_FLOAT) {
      for(i = 0;i < v1->dimen;i++) {
	 num_to_ascii(v1->vec.f[i],0,ans->vec.s.s[i]);
      }
   } else {
      for(i = 0;i < v1->dimen;i++) {
	 num_to_ascii(v1->vec.l[i],0,ans->vec.s.s[i]);
      }
   }

   ans->name = "(string)";
   vec_free(v1);
   return;
}

/***********************************************************/
/*
 * Return the number of characters in a vector's elements
 */
void
vec_strlen(v1,ans)
VECTOR *v1,			/* vector to be measured */
       *ans;			/* the answer */
{
   int i;

   ans->type = VEC_NULL;

   if(v1->type != VEC_STRING) {
      msg_1s("Vector %s is not string valued",v1->name);
      yyerror("show where");
      vec_free(ans);
      return;
   }

   ans->type = VEC_LONG;
   if(vec_malloc(ans,v1->dimen) < 0) {
      msg_1s("Can't allocate space for strlen(%s)\n",v1->name);
      vec_free(v1);
      return;
   }
   ans->name = "(strlen)";

   for(i = 0;i < v1->dimen;i++) {
      ans->vec.l[i] = strlen(v1->vec.s.s[i]);
   }

   vec_free(v1);
   return;
}

/***********************************************************/
/*
 * Return the returns the V2-character substring of STR starting at V1.
 * If V2 is 0, the rest of STR is returned.
 */
void
vec_substr(str,v1,v2,ans)
VECTOR *str,*v1,*v2,
       *ans;				/* the answer */
{
   int dimen;				/* dimension of ans */
   int i;
   int is,i1,i2;			/* indices for str, v1, v2 */
   int len;				/* length of element */
   int num;				/* number of chars to copy */
   int start;				/* starting index */

   ans->type = VEC_NULL;

   if(v1->type == VEC_FLOAT) {
      vec_convert_long(v1);
   }
   if(v2->type == VEC_FLOAT) {
      vec_convert_long(v2);
   }

   if(str->type != VEC_STRING ||
      v1->type != VEC_LONG || v2->type != VEC_LONG) {
      if(str->type != VEC_STRING) {
	 msg_1s("Vector %s is not string-valued",str->name);
	 yyerror("show where");
      }
      if(v1->type != VEC_LONG) {
	 msg_1s("Vector %s is not integer",v1->name);
	 yyerror("show where");
      }
      if(v2->type != VEC_LONG) {
	 msg_1s("Vector %s is not integer",v2->name);
	 yyerror("show where");
      }
      vec_free(str);
      vec_free(v1);
      vec_free(v2);
      return;
   }

   if(str->dimen == 1) {
      if(v1->dimen == 1) dimen = v2->dimen;
      else if(v2->dimen == 1) dimen = v1->dimen;
      else if(v1->dimen == v2->dimen) dimen = v1->dimen;
      else {
	 msg_2s("Vectors %s and %s have different lengths",v1->name,v2->name);
	 yyerror("show where");
	 dimen = -1;
      }
   } else {
      if(v1->dimen == str->dimen || v1->dimen == 1) {
	 dimen = str->dimen;
      } else {
	 msg_2s("Vectors %s and %s have different lengths",str->name,v1->name);
	 yyerror("show where");
	 dimen = -1;
      }
      if(v2->dimen == str->dimen || v2->dimen == 1) {
	 dimen = str->dimen;
      } else {
	 msg_2s("Vectors %s and %s have different lengths",str->name,v2->name);
	 yyerror("show where");
	 dimen = -1;
      }
   }
   
   if(dimen < 0) {
      vec_free(str);
      vec_free(v1);
      vec_free(v2);
      return;
   }
   
   ans->type = VEC_STRING;
   if(vec_malloc(ans,dimen) < 0) {
      msg_2s("Can't allocate space for substr(%s,%s",str->name,v1->name);
      msg_1s("%s)\n",v2->name);
      vec_free(str);
      vec_free(v1);
      vec_free(v2);
      return;
   }
   ans->name = "(substr)";

   for(i = is = i1 = i2 = 0;i < dimen;i++) {
      len = strlen(str->vec.s.s[is]);
      if(v1->vec.l[i1] < 0) {
	 start = -v1->vec.l[i1];	/* desired starting index */
	 start = len - start;
      } else {
	 start = v1->vec.l[i1];		/* desired starting index */
      } 
      if(start < 0 || start >= len) {
	 if(sm_verbose && (v1->dimen > 1 || i == 0)) {
	    msg_1s("Starting index %s",v1->name);
	    msg_1d("[%d]",i1);
	    msg_1d(" = %d is invalid",start);
	    yyerror("show where");
	 }
	 ans->vec.s.s[i][0] = '\0';
	 continue;
      }
      if((num = v2->vec.l[i2]) <= 0) {
	 num = (len - start) + num;
      }
      if(num < 0 || start + num > len) {
	 if(sm_verbose && (v1->dimen > 1 || v2->dimen > 1 || i == 0)) {
	    msg_1d("substring[%d]: ",i);
	    msg_1s("%s(",str->name);
	    msg_1d("%d:",start);
	    msg_1d("%d) is invalid",start + num - 1);
	    yyerror("show where");
	 }
	 num = VEC_STR_SIZE - start - 1;
      }
      strncpy(ans->vec.s.s[i],&str->vec.s.s[is][start],num);
      ans->vec.s.s[i][num] = '\0';
      if(str->dimen > 1) is++;
      if(v1->dimen > 1) i1++;
      if(v2->dimen > 1) i2++;
   }

   vec_free(str);
   vec_free(v1);
   vec_free(v2);
   return;
}
