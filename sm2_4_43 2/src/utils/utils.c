#include "copyright.h"
/*
 * Here are some routines that do things that vms requires
 *	alloca		: DUMMY - used by bison
 *	memcpy
 * and general utilities
 *	next_word	: parse a string into words (v. similar to strtok)
 *	sm_suspend	: suspend the process, and return to its parent
 *	version		: what version are we running?
 *	atof2		: like atof() but understands 1.23d-10
 *	atn2		: identical to atan2(), but it may not be supplied
 *	check_noclobber : complain if a file would be overwritten && $noclobber
 *      make_temp	: like mktemp but with no limits on the number of names
 */
#include "options.h"
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "sm_declare.h"

#ifdef NO_ALLOCA
Void *
alloca(i)
int i;
{
   msg("Alloca is not supported\n");
   exit(EXIT_BAD);
}
#endif
#ifdef FreeBSD
/*
 *      ecvt converts to decimal
 *      the number of digits is specified by ndigit
 *      decpt is set to the position of the decimal point
 *      sign is set to 0 for positive, 1 for negative
 */

#define NDIG    80

static char*
cvt(double arg, int ndigits, int *decpt, int *sign, int eflag)
{
        register int r2;
        double fi, fj;
        register char *p, *p1;
        static char buf[NDIG];

        if (ndigits>=NDIG-1)
                ndigits = NDIG-2;
        r2 = 0;
        *sign = 0;
        p = &buf[0];
        if (arg<0) {
                *sign = 1;
                arg = -arg;
        }
        arg = modf(arg, &fi);
        p1 = &buf[NDIG];
        /*
         * Do integer part
         */
        if (fi != 0) {
                p1 = &buf[NDIG];
                while (fi != 0) {
                        fj = modf(fi/10, &fi);
                        *--p1 = (int)((fj+.03)*10) + '0';
                        r2++;
                }
                while (p1 < &buf[NDIG])
                        *p++ = *p1++;
        } else if (arg > 0) {
                while ((fj = arg*10) < 1) {
                        arg = fj;
                        r2--;
                }
        }
        p1 = &buf[ndigits];
        if (eflag==0)
                p1 += r2;
        *decpt = r2;
        if (p1 < &buf[0]) {
                buf[0] = '\0';
                return(buf);
        }
        while (p<=p1 && p<&buf[NDIG]) {
                arg *= 10;
                arg = modf(arg, &fj);
                *p++ = (int)fj + '0';
        }
        if (p1 >= &buf[NDIG]) {
                buf[NDIG-1] = '\0';
                return(buf);
        }
        p = p1;
        *p1 += 5;
        while (*p1 > '9') {
                *p1 = '0';
                if (p1>buf)
                        ++*--p1;
                else {
                        *p1 = '1';
                        (*decpt)++;
                        if (eflag==0) {
                                if (p>buf)
                                        *p = '0';
                                p++;
                        }
                }
        }
        *p = '\0';
        return(buf);
}

char*
ecvt(double arg, int ndigits, int *decpt, int *sign)
{
        return(cvt(arg, ndigits, decpt, sign, 1));
}

char*
fcvt(double arg, int ndigits, int *decpt, int *sign)
{
        return(cvt(arg, ndigits, decpt, sign, 0));
}


#endif

#if defined(VMS)
unlink(file)
Const char file[];
{
   return(delete(file));
}
#endif
 
#ifdef NO_MEMCPY
/*
 * If you have bcopy use this:
 */
#if 1
Void *
memcpy(p1,p2,count)
register Void *p1;
register Const Void *p2;
register int count;
{
   Void *bcopy();
   
   return(bcopy(p2,p1,count));
}
#else					/* but here's some real code */
Void *
memcpy(p1,p2,count)
register Void *p1;
register Const Void *p2;
register int count;
{
   register int i;
   Void *sp1;

   sp1 = p1;
   
   for(i = (count % 8) + 1;--i > 0;) {
      *p1++ = *p2++;
   }
   
   for(i = (count>>3) + 1;--i > 0;) {
      *p1++ = *p2++;
      *p1++ = *p2++;
      *p1++ = *p2++;
      *p1++ = *p2++;
      *p1++ = *p2++;
      *p1++ = *p2++;
      *p1++ = *p2++;
      *p1++ = *p2++;
   }
   return(p1);
}
#endif
#endif
   
/*********************************************************/
/*
 * Suspend a process, and return control to its parent
 */
#ifdef vms
#  include <jpidef.h>
#  include <ssdef.h>
#endif
#include <signal.h>
int
sm_suspend()
{
#ifdef SIGSTOP
   return(kill(getpid(),SIGSTOP));	/* suspend process */
#else
#  if defined(__BORLANDC__)
   system("");
   return(0);				/* success */
#  else
#     if defined(vms)
   int stat;				/* return code from system calls */
   long iosb[2],			/* IO-status block */
	ppid,				/* parent's PID */
	ret_len1;			/* length returned by SYS$GETJPI */
   struct item_descrip {		/* item descriptor for SYS$GETJPI */
      short buf_len,			/* length of buffer to be filled */
	    item_code;			/* code to request information */
      long *buf_address,		/* address of return buffer */
	   *ret_len_address;		/* address of word to return length */
   } itmlist[2];

   itmlist[0].buf_len = sizeof(ppid);
   itmlist[0].item_code = JPI$_OWNER;
   itmlist[0].buf_address = &ppid;
   itmlist[0].ret_len_address = &ret_len1;

   itmlist[1].buf_len = 0;			/* zero to mark end of list */
   itmlist[1].item_code = 0;

   (void)SYS$GETJPIW(0,0,0,itmlist,iosb,0,0);
   if((stat = iosb[0]) != SS$_NORMAL) {
      msg_1d("SYS$GETJPIW fails, returning %d\n",stat);
      (void)fflush(stderr);
      return(-1);
   }
   if(ppid == 0) {			/* There is no parent */
      if((stat = LIB$PAUSE()) != SS$_NORMAL) {
         msg_1d("LIB$PAUSE fails, returning %d ",stat);
         msg_1d("( = 0x%x)\n",stat);
         (void)fflush(stderr);
         return(-1);
      }
      return(0);
   }
   if((stat = LIB$ATTACH(&ppid)) != SS$_NORMAL) {
      msg_1d("LIB$ATTACH fails, returning %d ",stat);
      msg_1d("( = 0x%x)\n",stat);
      (void)fflush(stderr);
      return(-1);
   }
   return(0);
#     else
   msg("\rI don't know how to suspend a process\r");
   sleep(1);
   return(-1);
#     endif
#  endif
#endif
}

/****************************************************************/
/*
 * If called with a string argument, return the first non-blank character.
 * Future calls to next_char with a NULL string will return pointers
 * to succesive words, all nul terminated. No extra storage is allocated,
 * and the nuls are inserted into the original string.
 * (This is very similar to the ANSI function strtok)
 */
char *
next_word(ptr)
char *ptr;
{
   char *word;
   static char *next = NULL;

   if(ptr != NULL) {
      while(isspace(*ptr)) ptr++;
      next = ptr;
   }
   if(next == NULL) {
      msg("You must set next_word with a string!\n");
      return("");
   }
   word = ptr = next;

   while(*ptr != '\0') {
      if(isspace(*ptr)) {
	 *ptr++ = '\0';
	 break;
      }
      ptr++;
   }
   
   while(isspace(*ptr)) ptr++;

   next = ptr;
   return(word);
}

/************************************************************************/
/*
 * Convert a string into a set of words (like argc/argv)
 * Initially argc is the dimension of argv
 */
int
str_to_argv(str,argv,argc)
char *str;				/* string to be split up */
char *argv[];				/* pointers to words */
int *argc;				/* number of words */
{
   char *ptr;
   int size_argv;

   size_argv = *argc;

   if(str == NULL) {
      *argc = 0;
      return(0);
   }

   ptr = str;
   for(*argc = 0;*argc < size_argv;(*argc)++) {
      while(isspace(*ptr)) ptr++;
      if(*ptr == '\0') {		/* nothing more to read */
	 return(0);
      } else if(*ptr != '"') {		/* not a quoted string */
	 argv[*argc] = ptr;
	 for(;;) {
	    if(*ptr == '\0') {
	       (*argc)++;
	       return(0);
	    } else if(isspace(*ptr)) {
	       break;
	    } else {
	       ptr++;
	    }
	 }
      } else {
	 argv[*argc] = ++ptr;
	 for(;*ptr != '\0';ptr++) {
	    if(*ptr == '\\') {
	       if(*(ptr + 1) != '\0') ptr++;
	    } else if(*ptr == '"') {
	       break;
	    }
	 }
      }
      if(*ptr != '\0') *ptr++ = '\0';
   }
   return(-1);
}

/*****************************************************************************/
/*
 * This function converts a string to a float:
 *	10	-> 10.0
 *	-1.23	-> -1.23
 * 	1E3	-> 1000	
 * 	1.23E-3 -> 0.00123
 * Where E is d, D, e, or E (i.e. it's atof() allowing fortran's `D' format)
 *
 * In addition, if the string starts "0x" and contains only hexdigits, or
 * "0" and only [0-7] it'll be interpreted as a hex or octal integer
 */
REAL
atof2(str)
char *str;
{
   REAL f;
   int negative;			/* is number negative? */
   REAL x;

   if(*str == '-') {
      str++;
      negative = 1;
   } else if(*str == '+') {
      str++;
      negative = 0;
   } else {
      negative = 0;
   }
   
   if(str[0] == '0') {			/* may be hex or octal integer */
      char *sstart = str;		/* initial value of str */
      str++;
      x = 0.0;
      if(*str == 'x') {
	 str++;
	 for(;;) {
	    if(isdigit(*str)) {
	       x = 16*x + *str++ - '0';
	    } else if(*str >= 'a' && *str <= 'f') {
	       x = 16*x + *str++ - 'a' + 10;
	    } else {
	       break;
	    }
	 }
      } else {
#if OCTAL_NUMBERS
	 while(isdigit(*str)) {
	    if(*str == '8' || *str == '9') {
	       break;
	    }
	    x = 8*x + *str++ - '0';
	 }
#endif
      }
      if(*str == '\0') {
	 return(negative ? -x : x);	/* yup; an octal or hex integer */
      }
      str = sstart;
   }
   
   x = 0.0;				/* try again; it's decimal */
   while(isdigit(*str)) {
      x = 10*x + *str++ - '0';
   }
   if(*str == '\0') {
      return(negative ? -x : x);
   } else if(*str == '.') {
      str++;
      for(f = 0.1;isdigit(*str);f /= 10) {
	 x += (*str++ - '0')*f;
      }
   }
   if(*str == 'd' || *str == 'D' || *str == 'e' || *str == 'E') {
      str++;
      x *= pow(10.,(double)atoi(str));
   }
   
   return(negative ? -x : x);
}

/*****************************************************************************/
/*
 * This function converts a string to an int:
 *	10	-> 10.0
 *	-1	-> -1
 *      0xa     -> 10
 *
 * In addition, if the string starts "0x" and contains only hexdigits, or
 * "0" and only [0-7] it'll be interpreted as a hex or octal integer
 *
 * (i.e. it's atoi() allowing 0x to signal hex, and maybe 0 to signal octal )
 *
 * If the base is specified it'll be taken as the initial guess, and
 * may be used to e.g. read hex strings like DEAD1234 without the leading 0x
 */
LONG
atoi2(str, base, end)
char *str;
LONG base;
char **end;				/* final value of str; or NULL */
{
   int negative;			/* is number negative? */
   LONG x;

   if(base == 0) {
       base = 10;
   }

   if(*str == '-') {
      str++;
      negative = 1;
   } else if(*str == '+') {
      str++;
      negative = 0;
   } else {
      negative = 0;
   }
   
   if(str[0] == '0') {			/* may be hex or octal integer */
      str++;
      x = 0;
      if(base == 16 || *str == 'x') {
	 if (*str == 'x') {
	    str++;
	 }
	 for(;;) {
	    if(isdigit(*str)) {
	       x = 16*x + *str++ - '0';
	    } else if(*str >= 'a' && *str <= 'f') {
	       x = 16*x + *str++ - 'a' + 10;
	    } else if(*str >= 'A' && *str <= 'F') {
	       x = 16*x + *str++ - 'A' + 10;
	    } else {
	       break;
	    }
	 }
      } else if(base == 8 || OCTAL_NUMBERS) {
	 while(isdigit(*str)) {
	    if(*str == '8' || *str == '9') {
	       break;
	    }
	    x = 8*x + *str++ - '0';
	 }
      } else {
	  while(*str == '0') {
	      str++;
	  }
	  while(isdigit(*str)) {
	      x = base*x + *str++ - '0';
	  }
      }

      if(end != NULL) {
	 *end = str;
      }
   } else {
      x = strtol(str, end, (base == 0 ? 10 : base));
   }
   
   return(negative ? -x : x);
}

#if !defined(M_PI)
#  include "sm.h"
#  define M_PI PI
#endif

REAL
atn2(y,x)
double y,x;
{
   double ans;				/* the answer */
   int quadrant;			/* which quadrant are we in? */

   if(x == 0.0) {
      return(y >= 0.0 ? M_PI/2 : -M_PI/2);
   }

   if(x > 0) {
      if(y > 0) {
	 quadrant = 1;
      } else {
	 quadrant = 4;
	 y = -y;
      }
   } else {
      x = -x;
      if(y > 0) {
	 quadrant = 2;
      } else {
	 quadrant = 3;
	 y = -y;
      }
   }
   
   ans = atan(y/x);
   switch (quadrant) {
    case 1:
      break;
    case 2:
      ans = M_PI - ans;
      break;
    case 3:
      ans = ans - M_PI;
      break;
    case 4:
      ans = -ans;
      break;
   }
   return(ans);
}

/*****************************************************************************/
/*
 * If a file exists and $noclobber is set, return true. Otherwise return false.
 */
int
would_clobber(file)
char *file;
{
   FILE *fil;

   if(*print_var("noclobber") == '\0' || (fil = fopen(file,"r")) == NULL) {
      return(0);
   } else {
      fclose(fil);
      msg_1s("You attempted to overwrite %s with $noclobber defined\n",file);
      return(1);
   }
}

/*****************************************************************************/
/*
 * This is like mktemp, but some versions of mktemp only return 26 unique
 * names. tmpnam() is no better. This code tries up to 26**3 unique names
 * for a given PID
 */
#define NCHAR 62			/* number of chars used in names */
#define NTMP (26*26*26)			/* number of temp names tried */

static char
get09azAZ(c)
int c;
{
   c %= NCHAR;
   if(c < 10) {
      return(c + '0');
   }
   c -= 10;
   if(c < 26) {
      return(c + 'a');
   }
   c -= 26;
   return(c + 'A');
}

char *
make_temp(path)
char *path;
{
   static char name[101];
   static int id = 0;
   int i;
   int id0;
   int len = strlen(path);
   FILE *fil;
   int pid = getpid();
   int tmp;

   strncpy(name,path,100);
   if(len < 6 || strncmp(&name[len - 6],"XXXXXX",6) != 0) {
      return(name);
   }
/*
 * convert the pid to a 3-character string
 */
   for(i = 4;i <= 6;i++) {
      name[len - i] = get09azAZ(pid%NCHAR); pid /= NCHAR;
   }

   for(id0 = (id + NTMP - 1)%NTMP;id != id0;id = (id + 1)%NTMP) {
      tmp = id;
      for(i = 1;i <= 3;i++) {
	 name[len - i] = get09azAZ(tmp%NCHAR); tmp /= NCHAR;
      }
      if((fil = fopen(name,"r")) == NULL) { /* doesn't exist. Good */
	 return(name);
      } else {
	 fclose(fil);
      }
   }

   msg_1s("Failed to find unique name of the form %s\n",path);
   return(NULL);
}

/*****************************************************************************/
/*
 * Compare two strings, ignoring case
 */
int
IC_strncmp(str1, str2, n)
Const char *str1;
Const char *str2;
int n;
{
   int i;
   int c1, c2;

   for(i = 0; i < n; i++) {
      c1 = toupper(str1[i]); c2 = toupper(str2[i]);
      if(c1 == c2) {
	 if(c1 == '\0') {
	    return(0);
	 }
      } else {
	 return(c1 - c2);
      }
   }

   return(0);
}
