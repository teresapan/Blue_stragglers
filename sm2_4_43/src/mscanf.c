#include "copyright.h"
/*
 * Mscanstr is a bit like scanf("%s", except that it takes its input from
 * the function call input() and pushes output with unput, instead of using
 * getchar and unputc. It also checks that the string is long enough
 *
 * There is also a version of gets called mgets, which is similar to fgets
 * except that it uses input() not getc(), and does not require a FILE pointer.
 * A closing newline is returned to the input stream.
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "sm_declare.h"

extern int expand_vars,			/* turn on $ expansion within {}, "" */
	   in_quote,			/* are we in a quoted string? */
  	   noexpand;			/* expand and recognise things? */


/*****************************************************************************/
/*
 * Read to the end of the line using input()
 */
char *
mgets(s, n)
char *s;
register int n;
{
   register int c;
   register char *cs;

   if(n <= 1) {
      return(NULL);
   }
   cs = s;
   for(;;){
      if((c = input()) < 0) {
         return(NULL);
      }
      *cs++ = c;
      if(c == '\n') {
         (void)unput(c);
         cs--;
         break;
      } else if(--n <= 0) {
         break;
      }
   }
   *cs = '\0';
   return(s);
}

/*****************************************************************************/
/*
 * Read a string using input(). If noexpand is > 0 while we are doing
 * this (e.g. reading a macro argument), then quotes are not recognised.
 * We therefore deal with them explicitly here. If to_eol read up to the
 * end of the line, otherwise just read a white-space delimited word,
 * skipping any leading whitespace.
 */
static int
read_string(arg,size,to_eol)
char *arg;
int size;
int to_eol;				/* read to the eol? */
{
   char *aptr;				/* pointer to string arg */
   int allow_space,			/* allow blanks in the string? */
       c,
       in_q = 0,			/* we're in '' */
       in_qq = 0,			/* we're in "" */
       save_expand_vars = expand_vars;	/* saved value */

   if(!in_quote && !to_eol) {
      while((c = input()) == ' ' || c == '\t') continue;
      (void)unput(c);
      if(c == EOF) return(EOF);
   }

   for(aptr = arg;aptr < arg + size;aptr++) {
      allow_space = to_eol || in_q || in_qq;
      expand_vars = (in_qq ? 0 : save_expand_vars);
      if((*aptr = input()) == '$' && sm_peek() == '!') {
	 expand_vars = 1;
	 (void)unput(*aptr);
	 *aptr = input();
      }
      if(*aptr == ' ' || *aptr == '\t') {
	 if(!allow_space) {
	    break;
	 }
      } else if(*aptr == '\n') {
	 break;
      } else if(*aptr == '\'') {
	 in_q = in_q ? 0 : 1;
      } else if(*aptr == '"') {
	 in_qq = in_qq ? 0 : 1;
      } else if(*aptr == '\\') {	/* escaped ", ', $, \, or \n */
	 if((c = sm_peek()) == '"' || c == '\'' || c == '$' || c == '\\') {
	    if(aptr < arg + size - 2) {
	       *++aptr = input();
	    }
	 } else if(c == 'n' && !(in_quote || in_q || in_qq)) {
	    break;
	 }
      }
   }
   (void)unput(*aptr);

   if(aptr == arg + size) {
      msg_1s("Insufficient room in read_string() for string:\n\t\"%s\"\n",arg);
      aptr--;
   }
   *aptr = '\0';

   expand_vars = save_expand_vars;
   return(1);
}

/*****************************************************************************/
/*
 * Here are the functions that SM actually uses. History and all that.
 */
int
mscanstr(arg,size)
char *arg;
int size;
{
   return(read_string(arg,size,0));
}

int
mscanline(arg,size)
char *arg;
int size;
{
   return(read_string(arg,size,1));
}
