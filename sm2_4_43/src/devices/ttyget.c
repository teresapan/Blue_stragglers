#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "tty.h"
#include "sm_declare.h"

/*
 * ttygetb -- Determine whether or not a capability exists for a device.
 * If there is any entry at all, the capability exists.
 */
int
ttygetb (tty, cap)
TTY *tty;			/* tty descriptor */
char cap[];			/* two character capability name */
{
   return (tty_find_capability (tty, cap) != NULL);
}

/*************************************************************/
/*
 * TTYGETI -- Get an integer valued capability.  If the capability is not
 * found for the device, or cannot be interpreted as an integer, zero is
 * returned.  Integer capabilities have the format ":xx#dd:".
 */
int
ttygeti (tty, cap)
TTY *tty;			/* tty descriptor */
char cap[];		/* two character capability name */
{
   char *ip;
   
   if ((ip = tty_find_capability(tty, cap)) == NULL)
     return (0);
   else if(*ip != '#') {
      return (0);
   } else {
      return(atoi(++ip));		/* skip the '#' */
   }
}

/**************************************************/
/*
 * TTYGETR -- Get a real valued capability.  If the capability is not
 * found for the device, or cannot be interpreted as a number, zero is
 * returned.  Real valued capabilities have the format ":xx#num:".
 */
float
ttygetr (tty, cap)
TTY *tty;			/* tty descriptor */
char	cap[];		/* two character capability name */
{
   char *ip;			/* pointer to graphcap string */
   
   if ((ip = tty_find_capability(tty, cap)) == NULL)
     return(0.0);
   else if(*ip != '#') 
     return(0.0);
   else {
      return(atof2(++ip));
   }
}

/***********************************************************/
/*
 * TTYGETS -- Get the string value of a capability. Process all termcap escapes
 * These are:
 * 
 * 	\E		ascii esc (escape)
 * 	^X		control-X (i.e., ^C=03B, ^Z=032B, etc.)
 * 	\[nrtbf]	newline, return, tab, backspace, formfeed
 * 	\ddd		octal value of character
 * 	\^		the character ^
 * 	\\		the character \
 * 
 * The character ':' may not be placed directly in a capability string; it
 * should be given as \072 instead.  The null character is represented as \200;
 * all characters are masked to 7 bits upon output by TTYPUTS, hence \200
 * is sent to the terminal as NUL.
 */
int
ttygets(tty, cap, outstr, maxch)
TTY *tty;			/* tty descriptor */
char cap[],			/* two character capability name */
     outstr[];			/* receives cap string */
int maxch;			/* size of outstr */
{
   char	ch,
	*ip,
	*op;

   op = outstr;
   *op = '\0';			/* empty string if not present */

   if((ip = tty_find_capability (tty, cap)) != NULL) {
/* Skip the '=' which follows the two character capability name. */
      if (*ip == '=') ip++;
/* Extract the string, processing all escapes. */
      for(ch = *ip;ch != ':';ch = *ip) {
	 if(ch == '^') {
	    if(islower(ch)) ch = toupper(ch);
	    ch = *++ip - 'A' + 1;
	 } else if (ch == '\\') {
	    switch (*++ip) {
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7':
	        ch = *ip - '0';
	        if(isdigit(*(ip+1))) ch = 8*ch + (*++ip - '0');
		if(isdigit(*(ip+1))) ch = 8*ch + (*++ip - '0');
	        break;
	    case 'E':
	        ch = '\033';		/* Escape */
		break;
	    case 'b':
	        ch = '\b';
		break;
	    case 'f':
	        ch = '\f';
		break;
	    case 'n':
	        ch = '\n';
		break;
	    case 'r':
	        ch = '\r';
		break;
	    case 't':
	        ch = '\t';
		break;
	    case '^':
	    case ':':
	    case '\\':
	    default:
		ch = *ip;
		break;
	    }
	 }

	*op++ = ch;
	ip++;
	if (op - outstr >= maxch - 1)		/* keep space for a NUL */
	    break;
      }
      *op = '\0';
   }
   return(op-outstr);
}

