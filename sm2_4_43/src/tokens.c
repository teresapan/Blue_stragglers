#include "copyright.h"
/*
 * Save the last token parsed to help with error diagnostics.
 *
 * save_float(), save_int(), or save_str() is called by the parser whenever
 * it parses a token,
 * get_line() will return the text of the last line parsed.
 * start_line() starts saving a new line
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "vectors.h"
#include "yaccun.h"
#include "control.h"		/* definitions of DOUBLE and INTEGER */
#include "sm_declare.h"

#define SIZE 200		/* official size of line buffer */

static char c_line[SIZE+CHARMAX] = "",
				/* current line (with room for another token)*/
	    *eptr = c_line,	/* pointer to end of current line */
	    *lptr = c_line;	/* pointer in line to start of current token */

void
save_float(x)
double x;
{
   if(eptr != c_line) {
      *eptr++ = ' ';			/* add a space */
      *eptr = '\0';
   }
   lptr = eptr;				/* move token pointer to end of line */

   (void)sprintf(lptr,"%g",x);

   while(*eptr != '\0') eptr++;		/* move eptr to end of line */
   if(eptr > c_line + SIZE) {		/* no room for next token */
      eptr = c_line;
   }
}

/*********************************************************/

void
save_int(i)
int i;
{
   if(eptr != c_line) {
      *eptr++ = ' ';			/* add a space */
      *eptr = '\0';
   }
   lptr = eptr;				/* move token pointer to end of line */

   (void)sprintf(lptr,"%d",i);

   while(*eptr != '\0') eptr++;		/* move eptr to end of line */
   if(eptr > c_line + SIZE) {		/* no room for next token */
      eptr = c_line;
   }
}

/***********************************************************/

void
save_str(str)
char *str;
{
   if(eptr != c_line) {
      *eptr++ = ' ';			/* add a space */
      *eptr = '\0';
   }
   lptr = eptr;				/* move token pointer to end of line */

   if(str[0] == '\n') {
      (void)sprintf(lptr,"\\n");	/* make carriage return visible */
   } else {
      (void)strcpy(lptr,str);
   }

   while(*eptr != '\0') eptr++;		/* move eptr to end of line */
   if(eptr > c_line + SIZE) {		/* no room for next token */
      eptr = c_line;
   }
}


/***********************************************************/

void
unsave_str()				/* unsave a string */
{
   if(eptr > c_line) eptr--;		/* back from '\0' */
   while(eptr >= c_line && !isspace(*eptr)) *eptr-- = '\0';
   if(eptr >= c_line) *eptr = '\0';	/* deal with space */
}

/***********************************************************/
/*
 * Return the text of the last line parsed, and the offset of the last
 * token parsed
 */
char *
get_line(offset)
int *offset;			/* offset of last token */
{
   *offset = lptr - c_line;	/* offset of last token */
   return(c_line);
}

/**************************************************************/

void
start_line()
{
   eptr = c_line;
}
