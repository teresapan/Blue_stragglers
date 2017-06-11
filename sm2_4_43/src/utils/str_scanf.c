#include "copyright.h"
#include "options.h"
/*
 * This function is identical to sscanf, but it works with read-only strings
 * It doesn't support floating point (%e, %f, %g) or pointer (%p) formats
 */
#if !defined(str_scanf)
#  if !defined(STDARG)
       #error Sorry, str_scanf uses <stdarg.h>
#  endif
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include "sm_declare.h"

#define IS_DIGIT(C)			/* is C an allowable digit? */	\
  ((base == 8 && ((C) >= '0') && ((C) < '8')) ||			\
   (base == 10 && isdigit(C)) ||					\
   (base == 16 && isxdigit(C)))
#define CHAR_AS_DIGIT(C)		/* convert C to a digit */	\
    (((C) <= '9') ? (C) - '0' : 10 + (C) - 'a')
#define LARGE 10000			/* maximum field width */

int
str_scanf(char *str, char *fmt, ...)
/*
 * str: string to scan
 * fmt: format to use
 * ...: items to return
 */
{
   char c,				/* character in format */
        in_range[128],			/* characters to allow */
        *s,				/* char * being scanned */
	width_modifier;			/* modifier for target width */
   double dval;
   int base,				/* base of integer */
       i,
       except = 0,			/* [^...] */
       negative,			/* is number negative? */
       nmatch = 0,			/* number of matches */
       suppress,			/* suppress assigning this field */
       width;				/* max width of this field */
   long lval;				/* integers being scanned */
   va_list ap;				/* argument pointer */

   va_start(ap,fmt);

   for(;;) {
      if(*str == '\0' || (c = *fmt++) == '\0') {
	 if(*str == '\0') {		/* check for trailing %n */
	    if(*fmt == '%' && *(fmt + 1) == 'n') {
	       *(va_arg(ap,int *)) = nmatch;
	    }
	 }
	 va_end(ap);
	 return(nmatch);
      } else if(c == '%') {
	 if((c = *fmt++) == '\0') {
	    msg("Illegal format in str_scanf: %% at end of format\n");
	    return(EOF);
	 } else if(c == '%') {		/* %% */
	    if(c != *str++) {
	       va_end(ap);
	       return(nmatch);
	    }
	 }
	 if(c == '*') {			/* %*... */
	    if((c = *fmt++) == '\0') {
	       msg("Illegal format in str_scanf: %%* at end of format\n");
	       return(EOF);
	    }
	    suppress = 1;
	 } else {
	    suppress = 0;
	 }
	 if(isdigit(c)) {			/* %{number}... */
	     for(width = CHAR_AS_DIGIT(c);;) {
	       if((c = *fmt++) == '\0') {
		  msg("Illegal format in str_scanf: %%# at end of format\n");
		  return(EOF);
	       }
	       if(!isdigit(c)) {
		  break;
	       }
	       width = 10*width + CHAR_AS_DIGIT(c);
	    }
	 } else {
	    width = LARGE;		/* should be enough */
	 }
	 if(c == 'h' || c == 'l' || c == 'L') {	/* %[hlL]... */
	    if((c = *fmt++) == '\0') {
	       msg_1d("Illegal format in str_scanf: %%%c at end of format\n",
		      (int)c);
	       return(EOF);
	    }
	    width_modifier = c;
	 } else {
	    width_modifier = '\0';
	 }

	 if(c != 'c') {
	    while(isspace(*str)) str++;
	 }

	 switch (c) {			/* finally deal with conversion char */
	  case 'd':			/* integral types */
	  case 'i':
	  case 'n':
	  case 'u':
	  case 'o':
	  case 'x':
	    if(c == 'n') {
	       lval = nmatch;
	    } else {
	       if(c == 'i') {
		  if(*str == '0') {
		     str++;
		     if(*str == 'x') {
			str++;
			base = 16;
		     } else {
			base = 8;
		     }
		  } else {
		     base = 10;
		  }
	       } else if(c == 'o') {
		  base = 8;
		  if(*str == '0') str++;
	       } else if(c == 'x') {
		  base = 16;
		  if(*str == '0' && *(str + 1) == 'x') {
		     str += 2;
		  }
	       } else {
		  base = 10;
	       }
	       if(base == 10 && *str == '-') {
		  if(c == 'u') {	/* unsigned */
		     va_end(ap);
		     return(nmatch);
		  }
		  negative = 1;
		  str++;
	       } else {
		  negative = 0;
	       }
	       if(!IS_DIGIT(*str)) {	/* failed */
		  va_end(ap);
		  return(nmatch);
	       }
	       for(i = 0,lval = 0;i < width;str++, i++) {
		  if(!IS_DIGIT(*str)) {
		     break;
		  }

		  lval = base*lval + CHAR_AS_DIGIT(*str);
	       }

	       if(negative) {
		   lval = -lval;
	       }
	       if(!suppress) {
		   nmatch++;
	       }
	    }
	    if(!suppress) {
	       if(width_modifier == 'h') {
		  *va_arg(ap,short *) = lval;
	       } else if(width_modifier == 'l') {
		  *va_arg(ap,long *) = lval;
	       } else if(width_modifier == '\0') {
		  *va_arg(ap,int *) = lval;
	       } else {
		  msg_1d("unknown modifier %c for ",(int)width_modifier);
		  msg_1d("%c format\n",(int)c);
		  va_end(ap);
		  return(EOF);
	       }
	    }
	    break;
	  case 'c':
	  case 's':
	  case '[':
	    if(suppress) {
	       s = NULL;
	    } else {
	       s = va_arg(ap,char *);
	    }

	    if(c == 'c') {
	       if(width == LARGE) {
		  width = 1;
	       }
	       for(i = 0;i < width && *str != '\0';i++,str++) {
		  if(s != NULL) {
		     *s++ = *str;
		  }
	       }
	    } else if(c == 's') {
	       for(i = 0;i < width && *str != '\0';str++,i++) {
		  if(isspace(*str)) {
		     break;
		  }
		  if(s != NULL) {
		     *s++ = *str;
		  }
	       }
	       if(s != NULL) {
		  *s = '\0';
	       }
	    } else {			/* %[...] */

	       if(*fmt == '^') {
		  except = 1;
		  fmt++;
		  for(i = 0;i < 128;i++) {
		     in_range[i] = 1;
		  }
	       } else {
		  for(i = 0;i < 128;i++) {
		     in_range[i] = 0;
		  }
	       }
	       if(*fmt == ']') {
		  in_range[']'] = (except == 0);
		  fmt++;
	       }
	       for(;;fmt++) {
		  if(*fmt == '\0') {
		     msg("Illegal format in str_scanf: unclosed %%[...]\n");
		     return(EOF);
		  } else if(*fmt == ']') {
		     fmt++;
		     break;
		  } else {
		     in_range[(int)(*fmt)] = (except == 0);
		  }
	       }
	       for(i = 0;i < width && *str != '\0';str++,i++) {
		  if(!in_range[(int)(*str)]) {
		     break;
		  }
		  if(s != NULL) {
		     *s++ = *str;
		  }
	       }
	       if(s != NULL) {
		  *s = '\0';
	       }
	    }
	    if(!suppress && i > 0) nmatch++;
	    break;
	  case 'e':
	  case 'f':
	  case 'g':
	    msg_1d("format %%%c is not supported by str_scanf\n",(int)c);
	    suppress = 1;
	    if(!suppress) {
	       if(width_modifier == 'l' || width_modifier == 'l') {
		  *va_arg(ap,double *) = dval;
	       } else if(width_modifier == '\0') {
		  *va_arg(ap,float *) = dval;
	       } else {
		  msg_1d("unknown modifier %c for ",(int)width_modifier);
		  msg_1d("%c format\n",(int)c);
		  va_end(ap);
		  return(EOF);
	       }
	    }
	    break;
	  default:
	    msg_1d("format %%%c is unknown to str_scanf\n",(int)c);
	    va_end(ap);
	    return(EOF);
	 }
      } else if(c == ' ' || c == '\t') { /* skip white space in fmt */
	 while(isspace(*str)) str++;	/* and in string too */
	 continue;
      } else {
	 if(c != *str++) {
	    va_end(ap);
	    return(nmatch);
	 }
      }
   }
}
#else
/*
 * make some linkers happy
 */
static void linker_func() { linker_func(); }
#endif
