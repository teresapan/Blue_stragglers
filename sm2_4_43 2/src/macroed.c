#include "copyright.h"
/*
 * Edit a macro, which may span several lines. The first line of the macro
 * takes the form "line_1", which includes the number of arguments. This line
 * may be edited to change this number.
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "charset.h"
#include "edit.h"
#include "sm_declare.h"

#define PSIZE 40			/* Maximum size of saved prompt */

static EDIT *first = NULL;		/* first (i.e. zeroth) line of macro */
static EDIT *free_list = NULL;		/* deleted history nodes */
static EDIT *last = NULL;		/* last line of macro */
static EDIT *new_line();
extern char prompt[];			/* current system prompt */
extern int sm_interrupt;			/* did we see a ^C? */
extern int plength;			/* length of prompt */
static void delete_lines();		/* delete a range of lines */
static int what_line();			/* what is my line number? */

int
macro_edit(name)
char *name;				/* name of macro to edit */
{
   char *macro,				/* initial text of macro */
	*mptr,				/* pointer to macro */
	nargs_str[20],			/* number of arguments */
   	*ptr,				/* pointer to current place in line */
	save_prompt[PSIZE + 1];		/* save prompt for SM proper */
   static char line_1[] = "Macro %s : Number of arguments: %s";
   					/* line one to be edited */
   static char search_string[40] = "";	/* the search string for ^S/^R */
   int i,j,
       line_num,			/* current line in macro */
       nargs_min,
       nargs_max,
       arg_eol,
       save_plength;			/* saved value of plength */
   EDIT *lptr;				/* pointer to current line */
   EDIT *temp;
   unsigned nchar;			/* number of characters in macro */

   delete_lines(first,last);
   if((first = new_line()) == NULL) {
      return(-1);
   }
   first->prev = NULL;
   lptr = first;
/*
 * Copy macro into arrays pointed to by lines
 */
   mptr = get_macro(name,&nargs_min,&nargs_max,&arg_eol);
   sprintf(nargs_str,"%d%d%d",arg_eol,nargs_min,nargs_max);
   (void)sprintf(first->line,line_1,name,nargs_str);
   for(;;) {
      if((lptr->next = new_line()) == NULL) {
	 return(-1);
      }
      lptr->next->prev = lptr;
      lptr = lptr->next;
      if(mptr == NULL) break;
      
      for(i = 0;i < HSIZE;i++) {
	 if((lptr->line[i] = *mptr++) == '\n' || lptr->line[i] == '\0') {
	    lptr->line[i] = '\0';
	    break;
	 }
      }
      if(*(mptr - 1) == '\0') {	/* the end of the macro */
	 break;
      }
   }
   last = lptr;
   last->next = NULL;
/*
 * Now start editing the macro
 */
   (void)strncpy(save_prompt,prompt,PSIZE);	/* save prompt */
   save_plength = plength;			/* and its length */
   lptr = first->next;
   line_num = 1;
   for(;;) {
      (void)sprintf(prompt,"%3d>",line_num); /* use line number as prompt*/
      plength = strlen(prompt) + 1;
      switch(i = edit_line(lptr->line,&ptr)) {
       case CTRL_C:
	 (void)strcpy(prompt,save_prompt);
	 plength = save_plength;
	 (void)unput('\n');
	 return(-1);
       case CTRL_D | '\200':		/* forget this command */
	 if((temp = lptr->next) == NULL) {
	    if((temp = lptr->prev) != NULL) {
	       line_num--;
	    }
	 }
	 if(temp == NULL) {
	    erase_str(lptr->line,HSIZE);
	    break;
	 }
	 
	 if(lptr->next != NULL) {	/* cut node out of list */
	    lptr->next->prev = lptr->prev;
	 }
	 if(lptr->prev != NULL) {
	    lptr->prev->next = lptr->next;
	 }
	 if(lptr == first) first = first->next;
	 if(lptr == last) last = last->prev;
	 
	 lptr->prev = free_list;	/* delete this node */
	 free_list = lptr;
	 lptr = temp;
	 break;
       case ('g' | '\200') :		/* go to line n */
	 if(*(ptr = get_edit_str("which line?  ")) == '\0') {
	    break;
	 }
	 j = atoi(ptr);
	 for(i = 0,temp = first;temp != NULL;i++,temp = temp->next) {
	    if(i == j) {
	       break;
	    }
	 }
	 if(temp == NULL) {
	    msg_1d("Macro has only %d lines\n",i);
	    lptr = last;
	    putchar(CTRL_G);
	 } else {
	    lptr = temp;
	 }
	 line_num = i;
	 break;
       case CTRL_M :			/* next line */
	 if((temp = new_line()) == NULL) {
	    putchar(CTRL_G);
	    putchar('\n');
	    (void)strcpy(prompt,save_prompt);	/* restore prompt */
	    plength = save_plength;
	    return(-1);
	 }
	 temp->next = lptr->next;
	 temp->prev = lptr;
	 if(lptr->next != NULL) lptr->next->prev = temp;
	 lptr->next = temp;
	 if(lptr == last) last = lptr->next;
	 lptr = temp;
	 line_num++;
	 if(*ptr != '\0') {		/* break line */
	    (void)strcpy(lptr->line,ptr);
	    erase_str(ptr,lptr->prev->line + HSIZE - ptr);
	 }
	 break;
       case CTRL_N :			/* next command */
	 if(lptr->next != NULL) {
	    lptr = lptr->next;
	    line_num++;
	 }
	 break;
       case CTRL_P:			/* previous command */
	 if(lptr->prev == NULL) {
	    putchar(CTRL_G);
	 } else {
	    line_num--;
	    lptr = lptr->prev;
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

	    if((temp = find_str(ptr,repeat,(i == CTRL_R) ? 1 : -1,
				first,last)) == NULL) {
	       putchar(CTRL_G);
	    } else {
	       lptr = temp;
	       line_num = what_line(lptr);
	    }
	 }
	 break;
       case CTRL_V :			/* forward N_SCROLL lines */
	 for(i = 0;i < N_SCROLL;i++) {
	    if(lptr->next == NULL ||
	       (lptr->next == last && *lptr->next->line == '\0')) {
	       putchar(CTRL_G);
	       break;
	    } else {
	       line_num++;
	       lptr = lptr->next;
	    }
	 }
	 break;
      case ('v' | '\200') :		/* scroll back N_SCROLL lines */
	 for(i = 0;i < N_SCROLL;i++) {
	    if(lptr->prev != NULL) {
	       line_num--;
	       lptr = lptr->prev;
	    } else {
	       putchar(CTRL_G);
	       break;
	    }
	 }
	 break;
      case CTRL_X:			/* exit editor */
	 (void)strcpy(prompt,save_prompt);
	 plength = save_plength;
	 putchar('\n');

	 if(str_scanf(first->line,line_1,name,nargs_str) != 2) {
	    msg("Argument definition line corrupted, no changes made\n");
	    return(-1);
	 }
	 
	 if(*nargs_str == '-') {
	    (void)define(name,"delete",0,0,0);
	    return(0);
	 }
	 
	 for(lptr = first->next;lptr != NULL && *lptr->line == '\0';
	     lptr = lptr->next) continue;
	 if(lptr == NULL) {		/* macro is empty */
	    (void)define(name,"delete",0,0,0);
	    return(0);
	 }
/*
 * We must convert lines back into a linear array
 */
         for(nchar = 0,lptr = first->next;lptr != NULL;lptr = lptr->next) {
	    nchar += strlen(lptr->line) + 1; /* count num. of characters */
	 }
	 if((macro = malloc(nchar)) == NULL) {
	    msg("Can't malloc space to return in edit_macro\n");
	    return(-1);
	 }
/*
 * We have the space, so copy lines onto mptr
 */
	 mptr = macro;
	 for(lptr = first->next;lptr != NULL;lptr = lptr->next) {
	    (void)strcpy(mptr,lptr->line);
	    j = strlen(lptr->line);
	    mptr += j;
	    if(j > 0) {
	       *mptr++ = '\n';
	    }
	 }
	 if(mptr > macro) *(mptr - 1) = '\0';

	 (void)define_s(name,macro,nargs_str);

	 return(0);
       case '<' | '\200':		/* first command */
	 lptr = first;
	 line_num = 0;
	 if(lptr->next != NULL) {
	    lptr = lptr->next;
	    line_num++;
	 }
	 break;
       case '>' | '\200':		/* last command */
	 if(last != NULL) {
	    lptr = last;
	    if(*lptr->line == '\0' && lptr->prev != NULL) {
	       lptr = lptr->prev;
	    }
	    line_num = what_line(lptr);
	 }
	 break;
       default:
	 putchar(CTRL_G);
	 break;
      }
      (void)fflush(stdout);
   }
}

/***************************************************************/
/*
 * get a fresh node for the next line
 */
static EDIT *
new_line()
{
   EDIT *new;				/* the new line */

   if(free_list != NULL) {		/* get one from free list */
      new = free_list;
      free_list = free_list->prev;
   } else {				/* need a new node */
      if((new = (EDIT *)malloc(sizeof(EDIT))) == NULL) {
	 fprintf(stderr,"Can't allocate storage for new editor node\n");
	 return(NULL);
      }
   }

   erase_str(new->line,HSIZE);
   new->mark = '\0';
   return(new);
}

/**************************************************************/
/*
 * Delete a range of lines (given as nodes)
 */
static void
delete_lines(l1,l2)
EDIT *l1,*l2;				/* the two lines */
{
   if(l1 == NULL && l2 == NULL) return;

   if(l1 == NULL) {
      l1 = first;
   }
   if(l2 == NULL) {
      l2 = last;
   }
   
   if(l1 == first) {			/* fixup first and last */
      first = l2->next;
   }
   if(l2 == last) {
      last = l1->prev;
   }

   if(l1->prev != NULL) {		/* cut section from macro */
      l1->prev->next = l2->next;
   }
   if(l2->next != NULL) {
      l2->next->prev = l1->prev;
   }

   l1->prev = free_list;		/* and put it onto free list */
   free_list = l2;
}

/**************************************************************/
/*
 * Return line number of a node
 */
static int
what_line(line)
EDIT *line;
{
   int i;
   EDIT *ptr;

   for(ptr = first,i = 0;ptr != NULL;ptr = ptr->next,i++) {
      if(ptr == line) {
	 return(i);
      }
   }
   msg_1s("Line \"%s\" doesn't exist\n",line == NULL ? "" : line->line);
   return(10000);
}
