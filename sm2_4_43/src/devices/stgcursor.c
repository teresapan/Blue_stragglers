#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "charset.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm.h"
#include "sm_declare.h"

#define	MAX_LENCUR	15

static int pat_match();

/*
 * STG_READCURSOR -- read the cursor, returning the cursor position
 * in SCREEN coordinates and the keystroke value as output arguments.  The
 * cursor value string is read in raw mode with interrupts disabled.  Receipt
 * of ^C causes EOF to be returned as the key value.
 *
 * The cursor is described by 2 parameters, a pattern string (RC) and a string
 * length parameter (CN).
 *
 *	RC	A pattern specifying either the delimiter sequence if
 *		len_curval > 0, or the entire cursor value string if
 *		len_curval <= 0.
 *
 *	CN	If no pattern is given, CN is the number of characters to be
 *		read and automatic error detection is not possible.  If a
 *		pattern is given, a negative CN specifies the minimum number
 * 		of characters in the cursor value string, with the pattern
 *		being used to match the characters in the actual cursor value
 *		string.  A positive CN specifies a fixed length cursor value
 *		string, which which case the pattern is used only to determine
 *		when a valid cursor value string has been received.
 *
 * The cursor read algorithm tries to ignore unsolicited input and recover from
 * bad cursor reads, or loss of hardware cursor mode during a cursor read, e.g.,
 * if the screen is accidentally cleared or the terminal otherwise initialized.
 */
#define SIZE 30

int
stg_cursor(get_pos,x,y)
int get_pos;				/* simply return the current position */
int *x,*y;			/* position of cursor */
{
   char buff[SIZE],
	*pattern,
	*decodecur;
   int len_pattern, len_curval, ip = 0, op, i1,
       key;

   key = EOF;
/*
 * Make sure there is a cursor before going any further
 */
   if(ttygets(g_tty,"RC",buff,SIZE) == 0) {
      msg("No cursor is defined for this device\n");
      return(EOF);
   } else if(!strcmp(buff,"prompt")) {
      stg_ctrl("GD");	/* deactivate graphics */
      printf("k,x,y: ");
      scanf("%d %d %d",x,y,&key);
      return(key);
   }
   
   decodecur   = g_sg->sg_decodecur;
   pattern     = g_sg->sg_delimcur;
   len_curval  = g_sg->sg_lencur;
   len_pattern = strlen(pattern);
/*
 * Either len_curval or pattern must be given, preferably both
 */
   if (len_curval == 0 && len_pattern == 0) return(EOF);
/*
 * Encode the cursor value pattern, which may be either a pattern
 * matching the entire cursor value, or just the delimiter.  The value
 * of len_curval may be negative if a pattern is given, but must be
 * positive otherwise
 */
   if(len_pattern <= 0 && len_curval < 0)
     len_curval = -len_curval;
/*
 * Set raw mode on the input file (the graphics terminal)
 */
   (void)get1char(CTRL_A);		/* initialise raw mode */
/*
 * Turn the device cursor on.  We assume that the cursor is turned
 * off when read
 */
   do {
      stg_ctrl1("RC",1);		/* write "RC" string, cursor 1 for
					   compatibility with iraf's GRAPHCAP*/
      ttflush(g_out);
/*
 * Read the cursor value string.  If a pattern is given accumulate
 * at least abs(len_curval) characters and stop when the pattern
 * is matched, returning the last len_curval characters if
 * len_curval > 0, else the matched substring.  If no pattern is
 * given simply accumulate len_curval characters.  The number of
 * characters we will accumulate in one iteration is limited to
 * MAX_LENCUR to permit retransmission of the RC control sequence
 * in the event that hardware cursor mode is accidentally cleared.
 */
      for(op=0;op < MAX_LENCUR;) {
	 key = g_mem[op++] = get1char(0);	/* read character */
	 g_mem[op] = '\0';

	 if(key == CTRL_C) {
	    (void)get1char(EOF);
	    return(EOF);
	 } else if (len_pattern > 0) {
/*
 * A pattern string was given.  Once the minimum number of
 * chars have been accumulated, try to match the pattern,
 * which may match either the cursor string delimiter (in
 * the case of a fixed length cursor value), or the entire
 * cursor string (which may then be variable length).
 */
	    if(op < abs(len_curval)) {
	       continue;
	    } else if((i1 = pat_match(g_mem,pattern)) >= 0) {
	       if(len_curval > 0)
		 ip = op - len_curval; 	/* fixed length cur */
	       else
		 ip = i1;		/* variable length cur */
	       break;
	    }
	 } else {
/*
 * No pattern was given.  Terminate the cursor read once the len_curval
 * characters have been accumulated.
 */
	    if(op >= len_curval) {
	       ip = 0;
	       break;
	    }
	 }
      }
   } while (op >= MAX_LENCUR);	/* exit if broke out of inner loop */
/*
 * Turn off raw input mode.
 */
   (void)get1char(EOF);
/*
 * Decode the cursor value string and return the position and key
 * as output values.  Return the cursor position in SCREEN coordinates.
 * If extra characters were typed, e.g., before the cursor was turned
 * on, and the cursor has a delimiter string, the extra characters will
 * have been read into low memory and we should be able to ignore them
 * and still get a valid read.
 */
   g_reg[E_IOP].i = ip;
   stg_encode (decodecur, g_mem, g_reg);
   
   *x = g_reg[1].i/xscreen_to_pix + 0.5;
   *y = g_reg[2].i/yscreen_to_pix + 0.5;
   key = g_reg[3].i;
   return(key);
}

/**********************************************************************/
/*
 * Pattern matching routine. Given a string and a pattern, if the pattern
 * occurs in the string, return the offset of the start of the pattern,
 * or -1 if not found.
 *
 * The following characters are `magic':
 *	#	matches any digit
 *	?	matches any character
 *	*	means `0 or more of the preceding character'
 *	^J,^M	match \n or \r	(beat terminal emulators...)
 *	\	escapes the next character - use \\ to match a single \
 *		( \\* won't work)
 *
 * Note that a#*?a will match aaaaa1111b at the first a.
 */
static int
pat_match(str,pattern)
char *str,			/* string to be searched for pattern */
     *pattern;			/* pattern to be found */
{
   char c,			/* == *pptr */
	*pptr,			/* pointer to pattern */
	*sptr,			/* pointer to str */
	*start_match;		/* start of matched string */
   int magic,			/* is next character magic? */
       match;			/* Does the current character match? */

   pptr = pattern;
   start_match = str;

   for(sptr = str;*sptr != '\0';) {
      if(*pptr == '\\') {		/* escape magic meaning of next char */
	 pptr++;
	 magic = 0;
      } else {
         magic = 1;
      }
      if(!magic) {
         match = (*pptr++ == *sptr);
      } else {
         if((c = *pptr++) == '?') {	/* match anything */
	    match = 1;
         } else if(c == '#') {		/* match any digit */
	    match = isdigit(*sptr);
	 } else if(c == '\n' || c == '\r') { /* match \n or \r */
	    match = (*sptr == '\n' || *sptr == '\r');
	 } else {
            match = (c == *sptr);
	 }
	 if(*pptr == '*') {		/* match any number of preceding char*/
	    if(match) {			/* we got one, look for another */
	       pptr--;
	    } else {
	       pptr++;			/* try the next character in pattern */
	       if(*pptr == '\0') {		/* pattern is matched */
	          return(start_match - str);	/* offset of start */
	       }
	       continue;
	    }
	 }
      }
      if(match) {
	 if(*pptr == '\0') {		/* pattern is matched */
	    return(start_match - str);	/* offset of start */
	 }
	 sptr++;
      } else {
	 pptr = pattern;
	 sptr = ++start_match;
      }
   }
   return(-1);			/* no match */
}
