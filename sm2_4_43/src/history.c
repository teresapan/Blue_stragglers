#include "copyright.h"
/*
 * these routines handle the history system, which allows any of the
 * previous commands to be repeated, by specifying ^number, ^string
 * or ^^ by analogy to the Unix csh history. The number of commands
 * remembered is given by the .sm history variable -- if this is zero
 * there is no limit to the number of commands, and if it is absent 80
 * commands are remembered. When reading macros onto the history list
 * there is no limit to the history size.
 *
 * The history list is maintained by a doubly-linked list, each of whose
 * nodes represents a line of history. The two ends of the list are
 * "first" and "last", and a free list of currently unused nodes is
 * maintained as "free_list".
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "charset.h"
#include "edit.h"
#include "sm_declare.h"

#define FPUTCHAR(C)			/* putchar + fflush */		\
  putchar((C) & '\177');						\
  (void)fflush(stdout);

static char buffer[HSIZE + 1];		/* buffer for unread()'ing */
static char *bptr = buffer;		/* pointer to buffer */
static EDIT *first = NULL;		/* oldest history node */
static EDIT *free_list = NULL;		/* deleted history nodes */
static EDIT *find_lev();		/* return a node given its number */
static EDIT *hptr = NULL;		/* look for a command */
static EDIT *last = NULL;		/* youngest history node */
static char last_line[HSIZE + 1] = "";	/* save the last line typed */
extern FILE *logfil;			/* where to log all terminal input */
static int new_hist();			/* get a fresh history node */
static int new_last_line = 0;		/* there's a new last line to delete */
static int num_hist = 0;		/* number of nodes allocated */
static int hlev = 1;			/* current history level */
extern int sm_interrupt;			/* respond to a ^C */
extern TERMINAL term;			/* describe my terminal */
extern int sm_verbose;			/* respond to a request for chatter */

char *
edit_hist()
{
   char *current,			/* line currently being edited */
   	*ptr,
        saved_new_line[HSIZE];		/* the line being composed */
   static char search_string[40] = "";	/* the search string for ^S/^R */
   int i;
   EDIT *hptr0;				/* the original position of hptr */
   EDIT *temp;

   if(new_hist(0) < 0) return(NULL);

   current = last->line;
   erase_str(current,HSIZE);
   *saved_new_line = '\0';

   if(*print_var("remember_history_line") == '\0') {
      hptr = last;
   } else {
      for(hptr = last;hptr != NULL;hptr = hptr->prev) {
	 if(hptr->mark & HISTORY_MARK) {
	    hptr->mark &= ~HISTORY_MARK;
	    break;
	 }
      }
      if(hptr == NULL) hptr = last;
   }
   hptr0 = hptr;

   for(;;) {
      switch(i = edit_line(current,(char **)NULL)) {
       case CTRL_C:
	 sm_interrupt = 0;
	 break;
       case CTRL_D | '\200':		/* forget this command */
	 erase_str(current,HSIZE);
	 if(hptr != last) {
	    if(hptr->next != NULL) {	/* cut node out of list */
	       hptr->next->prev = hptr->prev;
	    }
	    if(hptr->prev != NULL) {
	       hptr->prev->next = hptr->next;
	    }
	    if(hptr == first) first = first->next;

	    temp = hptr->next;
	    hptr->prev = free_list;	/* delete this node */
	    free_list = hptr;
	    hptr = temp;
	    (void)strcpy(current,hptr->line);
	 }
	 break;
       case ('g' | '\200') :		/* go to line n */
	 if(*(ptr = get_edit_str("which command?  ")) == '\0') {
	    break;
	 }
	 if((temp = find_lev(atoi(ptr))) == NULL) {
	    putchar(CTRL_G);		/* invalid history, e.g. deleted */
	 } else {
	    hptr = temp;
	    erase_str(current,HSIZE);
	    (void)strcpy(current,hptr->line);
	 }
	 break;
       case CTRL_M :			/* accept current command */
	 for(i = strlen(current) - 1;i >= 0 && current[i] != '\0';i--) {
	    if(!isspace(current[i])) break;
	    current[i] = '\0';
	 }
	 if(i >= 0) {			/* command is not empty */
	    if(last->prev != NULL && strcmp(last->prev->line,current) == 0) {
	       delete_last_history(1);	/* same as last command */
	    } else {
	       strcpy(last_line,current);
	       last->num = hlev++;
	       new_last_line = 1;	/* there is a new last command */
	    }
	    
	    if(hptr != hptr0) {
	       hptr->mark |= HISTORY_MARK;
	    }
	    
	    if(logfil != NULL) {
	       fprintf(logfil,"%s\n",current);
	       (void)fflush(logfil);
	    }
         }

	 return(current);
       case CTRL_N :			/* next command */
	 if(hptr->next == NULL) {
	    putchar(CTRL_G);
	 } else {
	    if(strcmp(hptr->line, current) != 0) { /* modified but not saved */
	       (void)strcpy(saved_new_line,current);
	    }
 	    
	    erase_str(current,HSIZE);
	    hptr = hptr->next;
 	    if(hptr->next == NULL) {	/* a new command, not on history yet */
	       putchar(CTRL_G);
	       (void)strcpy(current,saved_new_line); /* restore saved_... */
	    } else {
	       (void)strcpy(current,hptr->line);
	    }
 	 }
	 break;
       case CTRL_P:			/* previous command */
	 if(hptr->next == NULL ||	/* a new command, not on history yet */
	    (strcmp(hptr->line, current) != 0)) { /* modified but not saved */
	    (void)strcpy(saved_new_line,current);
	 }
	 if(hptr->prev == NULL) {
	    putchar(CTRL_G);
	 } else {
	    erase_str(current,HSIZE);
	    hptr = hptr->prev;
	    (void)strcpy(current,hptr->line);
	 }
	 break;
       case CTRL_R:			/* reverse search */
       case CTRL_S:			/* forward search */
	 {
	    char buff[HSIZE];
	    int repeat;

	    sprintf(buff,"string [%s]: ",search_string);
	    if(*(ptr = get_edit_str(buff)) == '\0') {
	       if(*search_string == '\0') {
		  msg("There is no search string\n");
		  break;
	       }
	       repeat = 1;
	       ptr = search_string;	/* repeat search */
	    } else {
	       repeat = 0;
	       strcpy(search_string,ptr);
	    }

	    erase_str(current,HSIZE);
	    if((temp = find_str(ptr,repeat,
				(i == CTRL_R) ? 1 : -1,first,last)) == NULL) {
	       putchar(CTRL_G);
	    } else {
	       hptr = temp;
	       (void)strcpy(current,hptr->line);
	    }
	 }
	 break;
       case CTRL_V :			/* forward N_SCROLL lines */
	 for(i = 0;i < N_SCROLL;i++) {
	    if(hptr->next != NULL) {
	       erase_str(current,HSIZE);
	       hptr = hptr->next;
	       (void)strcpy(current,hptr->line);
	    } else {
	       putchar(CTRL_G);
	       break;
	    }
	 }
	 break;
      case ('v' | '\200') :		/* scroll back N_SCROLL lines */
	 for(i = 0;i < N_SCROLL;i++) {
	    if(hptr->prev != NULL) {
	       erase_str(current,HSIZE);
	       hptr = hptr->prev;
	       (void)strcpy(current,hptr->line);
	    } else {
	       putchar(CTRL_G);
	       break;
	    }
	 }
	 break;
       case '<' | '\200':		/* first command */
	 erase_str(current,HSIZE);
	 hptr = first;
	 (void)strcpy(current,hptr->line);
	 break;
       case '>' | '\200':		/* last command */
	 erase_str(current,HSIZE);
	 if(last != NULL && last->prev != NULL) {
	    hptr = last->prev;
	 }
	 (void)strcpy(current,hptr->line);
	 break;
       default:
	 putchar(CTRL_G);
	 break;
      }
      (void)fflush(stdout);
   }
}

/******************************************************************/
/*
 * read a character from stdin, and store it in history buffer 
 * use get1char, so character retrieval is immediate. (Inverse is unreadc())
 *
 * The default history character is ^, and it is used in the comments
 */
#define HISTORY_CHAR ('^' | '\200')	/* character to retrieve history */

int
readc()
{
   char word[40],                       /* as in ^word for history request */
	*wptr;				/* pointer to word */
   EDIT *command = NULL;		/* the command of interest */
   int c,                               /* next character */
       i,
       len;                             /* length of word */

   if(bptr > buffer) return((int)*--bptr); /* first try the buffer */
/*
 * there are no characters waiting on the buffer, so read one from stdin.
 * look for a string ^c
 */
   if((c = map_key()) != HISTORY_CHAR) { /* not history */
      return(c);
   } else {						 /* probably history */
/*
 * we now know that we are dealing with a history symbol one of
 * ^^, ^$, ^nnn, or ^string so look at second character
 */
      FPUTCHAR(last_char);
      if((c = map_key()) == HISTORY_CHAR || c == '$') {
	 char *lptr;
	 printf("\b%s",term.del_char); (void)fflush(stdout); /* so delete ^ */
	 lptr = last_line;		/* the last one typed, which may not
					   be the last one saved */
	 len = strlen(lptr);
	 while(isspace(lptr[--len])) continue;
	 len++;

	 if(term.del_char[0] == '\0')	/* terminal is dumb, so redraw line */
	   (void)unreadc(CTRL_L);

	 if(c == HISTORY_CHAR) {
	    while(--len >= 0) {
	       (void)unreadc(lptr[len]);
	    }
	 } else {
	    while(--len >= 0 && !isspace(i = lptr[len])) {
	       (void)unreadc(i);
	    }
	    printf("%s",term.del_char); (void)fflush(stdout); /* so delete ^ */
	 }
	 return(readc());
      }
/*
 * Not one of special cases about last line, so find which line is wanted
 */
      wptr = word;
      *wptr++ = '^';			/* match at start of line */
      (void)unreadc(c);			/* push first character */
      do {
	 if((c = readc()) == DEL || c == CTRL_H) {
	    printf("\b%s",term.del_char); /* delete previous character */
	    (void)fflush(stdout);
	    if(wptr > word) {
	       wptr--;			/* delete previous */
	    } else {
	       return(readc());
	    }
	 } else {
	    if(c != '\0' && !isspace(c)) { FPUTCHAR(c); }
	    *wptr++ = c;
	    }
      } while(c != '\0' && !isspace(c)) ;
      c = *--wptr;			/* save last character*/
      *wptr = '\0';
/*
 * we have the string from ^string in word[], but it may be an integer
 */
      len = strlen(word);
      for(i = 1;i < len;i++) {		/* skip leading `^' */
	 if(i == 1 && word[i] == '-') continue;
	 if(!isdigit(word[i])) break;	/* string not just digits */
      }

      if(i == len) {			/* string was an integer */
	 command = find_lev(atoi(&word[1]));
      } else {				/* a string */
	 command = find_str(word,0,1,first,last);
      }
/*
 * we now know that the required command was "command" (NULL if not found)
 * delete characters of ^string
 */
      for(i = strlen(word);i > 0;i--) printf("\b%s",term.del_char);

      if(term.del_char[0] == '\0')	/* terminal is dumb, so redraw line */
	 (void)unreadc(CTRL_L);

      if(command == NULL) {
	 putchar(CTRL_G);				/* ring bell */

	 (void)unreadc(CTRL_H);
	 (void)unreadc(c);		/* something for readc() to read */
      } else {
	 (void)unreadc(c);	     /* char which terminated history symbol */
         len = strlen(command->line);
         while(--len >= 0) (void)unreadc(command->line[len]);
      }
      return(readc());
   }
}

/***************************************************/
/*
 * Return a character to the history buffer
 */
void
unreadc(c)
int c;
{
   if(bptr - buffer >= HSIZE) {
      fprintf(stderr,"Attempt to push too many characters in unreadc\n");
      return;
   } else {
      *bptr++ = c;
   }
}

/***************************************************/
/*
 * Define a macro from the history buffer
 */
int
define_history_macro(name,l1,l2)
char name[];				/* name of macro */
int l1,l2;				/* use history commands l1-l2 */
{
   char *macro,				/* text of macro */
	*mptr;				/* pointer to macro */
   EDIT *hl1,*hl2;			/* the two history nodes */
   EDIT *histptr;				/* node that traverse history */
   int i,
       len,				/* length of macro */
       nchar;				/* number of characters in macro */

   if(first == NULL) {
      if(sm_verbose > 1) {
	 msg("There are no lines on the history list\n");
      }
      return(-1);
   }

   if(l2 < l1) {			/* switch l2 and l1 */
      i = l1;
      l1 = l2;
      l2 = i;
   }
/*
 * Check that lines are on history list
 */
   if((hl1 = find_lev(l1)) == NULL) {
      if(sm_verbose > 1) {
	 fprintf(stderr,"Line %d is not on history list\n",l1);
      }
      hl1 = first;
   }
   if((hl2 = find_lev(l2)) == NULL) {
      if(sm_verbose > 1) {
	 fprintf(stderr,"Line %d is not on history list\n",l2);
      }
      hl2 = last;
   }
/*
 * We must convert lines into a linear array
 */
   nchar = 0;
   for(histptr = hl1;;histptr = histptr->next) { /* count num. of characters */
      nchar += strlen(histptr->line) + 1;	/* allow for \n or \0 */
      if(histptr == hl2) break;
   }
   if(nchar <= 0) {
      (void)define(name,"",0,0,0);
      return(0);
   }
   if((macro = malloc((unsigned)nchar)) == NULL) {
      fprintf(stderr,"Can't malloc space in define_history_macro\n");
      return(-1);
   }
/*
 * We have the space, so copy lines onto mptr
 */
   mptr = macro;
   for(histptr = hl1;;histptr = histptr->next) {
      (void)strcpy(mptr,histptr->line);
      len = strlen(histptr->line);
      mptr += len;
      if(len > 0) {
	 *mptr++ = '\n';
      }
      if(histptr == hl2) break;
   }
   if(mptr > macro) {
      *(mptr - 1) = '\0';
   }
   (void)define(name,macro,0,0,0);
   free(macro);
   return(0);
}

/***************************************************/

void
history_list(dir)
int dir;				/* direction to list in */
{
   char buff[HSIZE + 1];
   EDIT *histptr;
   int lev;

   for(lev = 1,histptr = first;histptr != NULL;histptr = histptr->next,lev++) {
      histptr->num = lev;
   }
   lev--;
	 
   (void)more((char *)NULL);			/* initialise more */
   if(dir >= 0) {
      for(histptr = last;histptr != NULL;histptr = histptr->prev,lev--) {
	 (void)sprintf(buff,"%3d  %s\n",histptr->num,histptr->line);
	 if(more(buff) < 0) break;
      }
   } else {
      for(lev = 1,histptr = first;histptr != NULL;
	  				       histptr = histptr->next,lev++) {
	 (void)sprintf(buff,"%3d  %s\n",histptr->num,histptr->line);
	 if(more(buff) < 0) break;
      }
   }
}

/***********************************************************/
/*
 * write history to a file
 */
void
write_hist()
{
   char *outfile;			/* file for saving history */
   FILE *fil;				/* stream for write */
   EDIT *histptr;

   if(*(outfile = print_var("history_file")) == '\0') {
      return;
   }

#ifdef vms			/* must delete old version of saved history */
   delete(outfile);
#endif
   if((fil = fopen(outfile,"w")) == NULL) {
      fprintf(stderr,"Can't open file %s for saving history\n",outfile);
      return;
   }

   for(histptr = first;histptr != NULL;histptr = histptr->next) {
      fprintf(fil,"%s\n",histptr->line);
   }

   (void)fclose(fil);
   return;
}

/***********************************************************/
/*
 * read history from a file
 */
void
read_hist(fil)
FILE *fil;				/* stream for read */
{
   char *outfile;			/* file for saving history */
   int opened_fil = 0;			/* did we open fil ourselves? */

   if(fil == NULL) {
      if((outfile = get_val("history_file")) == NULL) {
	 return;
      }
      if((fil = fopen(outfile,"r")) == NULL) {
	 return;
      }
      opened_fil = 1;
   }

   while(new_hist(1) == 0 && fgets(last->line,HSIZE,fil) != NULL) {
      if(last->line[strlen(last->line) - 1] == '\n')
	last->line[strlen(last->line) - 1] = '\0';
      last->num = hlev++;
   }
   delete_last_history(1);		/* we created one too many */

   if(opened_fil) {
      (void)fclose(fil);
   }
   
   return;
}

/****************************************************/
/*
 * Delete an entry from the history list
 */
void
delete_history(l1,l2,this_too)
int l1,l2;				/* range of lines to delete */
int this_too;				/* forget this command too */
{
   EDIT *hl1,*hl2;			/* the two history nodes */
   int i;

   if(first == NULL) {
      if(sm_verbose > 1) {
	 msg("There are no lines on the history list\n");
      }
      hlev = 1;
      return;
   }

   if(l2 < l1) {			/* switch l2 and l1 */
      i = l1;
      l1 = l2;
      l2 = i;
   }
/*
 * Check that lines are on history list
 */
   if((hl1 = find_lev(l1)) == NULL) {
      if(sm_verbose > 1) {
	 fprintf(stderr,"Line %d is not on history list\n",l1);
      }
      hl1 = first;
   }
   if((hl2 = find_lev(l2)) == NULL) {
      if(sm_verbose > 1) {
	 fprintf(stderr,"Line %d is not on history list\n",l2);
      }
      hl2 = last;
   }

   if(hl1 == first) {			/* fixup first and last */
      first = hl2->next;
   }
   if(hl2 == last) {
      last = hl1->prev;
      this_too = 0;			/* it'll go automatically */
   }

   if(hl1->prev != NULL) {		/* cut section from history */
      hl1->prev->next = hl2->next;
   }
   if(hl2->next != NULL) {
      hl2->next->prev = hl1->prev;
   }

   hl1->prev = free_list;		/* and put it onto free list */
   free_list = hl2;

   if(first == NULL) {			/* reset history counter */
      hlev = 1;
   }

   if(this_too) {
      delete_last_history(1);
   }
}

/****************************************************/
/*
 * Delete most recent entry from the history list
 */
void
delete_last_history(force)
int force;
{
   EDIT *temp;

   if(new_last_line == 0 && !force) {	/* we've already deleted the last line
					   typed so don't delete another */
      return;
   }
   
   if(last == NULL) {
      hlev = 1;
      return;
   }
   
   temp = last;
   last = last->prev;			/* cut it off active list */
   if(last != NULL) {
      last->next = NULL;
   } else {
      first = NULL;
   }
   
   temp->prev = free_list;		/* and put it on free list instead */
   free_list = temp;
   new_last_line = 0;			/* it's gone now */
}

/*******************************************************/
/*
 * Replace last command on history list by another
 */
void
replace_last_command(string)
char *string;				/* desired new command */
{
   if(last != NULL) {
      (void)strncpy(last->line,string,HSIZE);
   }
}

/*****************************************************************/
/*
 * Read a macro onto the history buffer
 */
int
read_history_macro(name)
char *name;				/* name of macro */
{
   char c,				/* a temporary character */
	*macro;				/* pointer to text */
   int i;
   
   if((macro = get_macro(name,(int *)NULL,(int *)NULL,(int *)NULL)) == NULL) {
      fprintf(stderr,"Macro %s is not defined\n",name);
      return(-1);
   }

   while(new_hist(1) == 0) {		/* for each line of macro */
      for(i = 0;i <= HSIZE;i++) {	/* read macro into line buffer */
         if((c = last->line[i] = *macro++) == '\0') {
	    last->num = hlev++;
	    return(0);
	 } else if(c == '\n') {
	    last->line[i] = '\0';	/* replace carriage return */
	    last->num = hlev++;
	    break;
	 } else if(i == HSIZE - 1) {
	    last->line[i] = '\0';	/* truncate line */
	    last->num = hlev++;
	    return(0);
	 }
      }
   }
   return(-1);
}

/***************************************************************/
/*
 * find_lev returns a history node, given a history number
 */
static EDIT *
find_lev(hnum)
int hnum;				/* the history number */
{
   EDIT *histptr;

   if(hnum < 0) {			/* number relative to end of list */
      for(histptr = last;histptr != NULL && hnum < 0;
	  				      histptr = histptr->prev,hnum++) {
	 ;
      }
   } else {
      for(histptr = last;histptr != NULL;histptr = histptr->prev) {
	 if(histptr->num == hnum) {
	    break;
	 }
      }
   }
   return(histptr);
}

/***************************************************************/
/*
 * get a fresh node for the next history command
 */
static int
new_hist(alloc)
int alloc;			/* if true, always make a node if needed */
{
   char *ptr;
   static int hsize = -1;		/* size of history list */

   if(hsize < 0) {
      if((ptr = get_val("history")) == NULL) {
	 hsize = 80;
      } else {
	 hsize = atoi(ptr);
      }
   }

   if(last == NULL) {
      if(free_list == NULL) {		/* no deleted nodes */
	 if((last = (EDIT *)malloc(sizeof(EDIT))) == NULL) {
	    fprintf(stderr,"Can't allocate storage for new history node\n");
	    return(-1);
	 }
	 num_hist++;
      } else {
	 last = free_list;
	 free_list = free_list->prev;
      }
      last->next = last->prev = NULL;
      first = last;
   } else if(last->line[0] == '\0') {	/* don't save blank lines */
      ;
   } else {
      if(free_list != NULL) {		/* get one from free list */
	 last->next = free_list;
	 free_list = free_list->prev;
      } else {				/* need a new node... */
	 if(alloc || hsize <= 0 || num_hist < hsize) {
	    if((last->next = (EDIT *)malloc(sizeof(EDIT))) == NULL) {
	       fprintf(stderr,"Can't allocate storage for new history node\n");
	       return(-1);
	    }
	    num_hist++;
	 } else {			/* re-use oldest command */
	    last->next = first;
	    if(first->next != NULL) first->next->prev = NULL;
	    first = first->next;
	 }
      }
      last->next->prev = last;
      last->next->next = NULL;
      last = last->next;
   }
   last->mark = '\0';
   return(0);
}
