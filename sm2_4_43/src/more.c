#include "copyright.h"
/************************************************************/
/*
 * This routine takes a line and writes it to stdout.
 * every NLIN newlines, it stops and waits for:
 *   A carriage return or linefeed      (for one more line),
 *   /string<CR>                        (to search for string),
 *   ^C                                 (as an sm_interrupt),
 *   ^D                                 (for another half page),
 *   h                                  (for help),
 *   n                                  (for repeat previous search),
 *   q                                  (for quit),
 * Anything else gives another page.
 *
 * If line is NULL, then initialise the the number of lines printed to 0.
 * Note that q ("Quit") works by cleaning up and returning -1.
 * It is YOUR responsibility to ensure that interrupts are acted upon.
 */
#include <stdio.h>
#include "options.h"
#include "charset.h"
#include "sm_declare.h"

#define LSIZE 80                /* maximum number of characters in a line */
#define NOTHING 50258 	        /* more has nothing interesting to report */
#define PSIZE 40                /* longest pattern to look for */

extern int sm_interrupt,	        /* handle ^C interrupts */
	   nlines;		/* number of lines on screen */
static int num_printed = 0,     /* # of lines in page already printed */
           search = 0,          /* looking for a pattern */
	   wait_for_a_character();

int
more(line)
char *line;                     	/* line to print */
{
   char *lptr;				/* pointer to line */
   static char pattern[PSIZE] = "",     /* pattern to look for */
               *pptr;                   /* pointer to pattern */

   if(line == NULL) {        		/* reset line counter */
      num_printed = 0;
      search = 0;                       /* no searching */
      return(0);
   }

   if(sm_interrupt) return(-1);

   if(search) {                         /* we are looking for a pattern */
      pptr = pattern;
      for(lptr = line;*lptr != '\0';lptr++) {
	 if(*lptr == *pptr) {
            if(*(pptr + 1) == '\0') {      /* we've matched the pattern */
               search = 0;
               num_printed = 1;
	       break;
	    } else {
	       pptr++;
	    }
	 } else {                          /* no, this isn't it */
	    pptr = pattern;
	 }
      }
   }

   if(!search) {
      (void)fputs(line,stdout);
      for(lptr = line;*lptr != '\0';) {
	 if(*lptr++ == '\n') {
	    if(++num_printed >= nlines - 3) {
	       num_printed = 0;
	       return(wait_for_a_character(pattern));	/* what it says */
	    }
	 }
      }
   }
   
   return(0);
}

/************************************************************/
/*
 * wait for a character, and act upon it
 */
static int
wait_for_a_character(pattern)
char *pattern;                         /* string to search for */
{
   int i,j;

   if(sm_interrupt) return(-1);
   
   (void)get1char(CTRL_A);			/* set get1char */

   (void)write(1,"...",3);
   i = get1char(0);
   printf("\r   \b\b\b");

   if(i == '\n' || i == '\r') {         /* one more line */
      num_printed = nlines;
   } else if(i == CTRL_D) {             /* half a page */
      num_printed = nlines/2;
   } else if(i == 'h' || i == '?') {    /* help */
      (void)get1char(EOF);
      printf("%s\n%s\n%s\n",
	  "Use q to quit, <CR> for another line, ^D for half a page,",
	  "h or ? for this message, /string<CR> to search (n or /<CR> repeats)",
	  "Anything else gives a full page.");
      return(wait_for_a_character(pattern));
   } else if(i == 'n') {                        /* repeat search */
      (void)putchar('\r');
      if(pattern[0] == '\0') {			/* no pattern specified */
	 (void)putchar(CTRL_G);
	 return(wait_for_a_character(pattern));
      } else {
	 search = 1;                               /* start searching */
      }
   } else if(i == 'q') {                        /* quit */
      get1char(EOF);
      return(-1);
   } else if(i == '/') {                        /* look for a pattern */
      (void)putchar('/');
      (void)fflush(stdout);
      for(i = 0;i < PSIZE;i++) {
         if((j = get1char(0)) == '\n' || j == '\r') {
            break;
         } else if(j == DEL) {                  /* delete character */
            if(i == 0) {                        /* abort search */
               printf("\b \b");                 /* remove / */
	       (void)fflush(stdout);
	       (void)get1char(EOF);
	       return(wait_for_a_character(pattern));
            } else {
               i -= 2;                          /* ready to be ++'ed */
               (void)printf("\b \b");
	       (void)fflush(stdout);
            }
         } else {
            (void)putchar(pattern[i] = j);
	    (void)fflush(stdout);
         }
      }
      if(i != 0) {                           /* null terminate a new pattern */
         pattern[i] = '\0';
      }
      (void)putchar('\r');
      search = 1;                               /* start searching */
   }

   (void)get1char(EOF);                      /* no more RAW mode */
   return(0);
}
