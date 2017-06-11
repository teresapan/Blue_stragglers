#include "copyright.h"
/*
 * These routines handle the system i/o
 *
 * input returns the next character to be processed. In doing so it
 * deals with expansions of variables and recognises comments, quotes,
 * and a variety of escape sequences. All this is handled by a stack
 * of character buffers, and a pointer to the current place. When this
 * stack is empty, input reads from stdin. This stack is also used to
 * handle macros, do loops, foreach loops, and if clauses; by pushing
 * a pointer to the desired text onto the stack, that text will become the
 * next text read. When the stack must be popped (because input finds
 * a '\0'), various magic things happen depending on the flag set. These
 * magic things handle looping for loops, freeing space, popping the
 * macro call stack, etc.
 *
 * sm_peek() looks at the next character to be processed
 * unput() pushes characters back onto the stack. At least one character
 *   may always be pushed, and usually many more if they came from the stack
 *   in the first place.
 * get_input_line() reads another line from the user if we run out of input.
 * push() pushes a string onto the stack.
 * lexflush() empties the stack, and resets various other stacks.
 * buff_is_empty() checks whether only whitespace remains to be read.
 * prompt_user() writes a string if the buffer is empty.
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "charset.h"
#include "edit.h"
#include "stack.h"
#include "sm_declare.h"

#define BSIZE 3				/* size of zero-level buffer */
#define CC '#'				/* comment character */
#define NSTACK 20			/* initial size of stack */
#define POP_STACK			/* pop stack, looking at flag */\
   sm_interrupt -= 2;			/* stop hand_C fiddling with stack */\
   switch(stack[sindex].flag) {						\
    case S_DO:				/* end of a do loop */		\
      in_comment = 0;							\
      if(next_do() < 0) {		/* the do loop is complete */	\
         free(stack[sindex].base);					\
         ptr = stack[--sindex].ptr;					\
      } else {								\
         ptr = stack[sindex].base;					\
      }									\
      break;								\
    case S_FOREACH:			/* end of a foreach loop */	\
      in_comment = 0;							\
      if(next_foreach() < 0) {		/* no more elements on list */	\
         free(stack[sindex].base);					\
	 ptr = stack[--sindex].ptr;					\
      } else {								\
         ptr = stack[sindex].base;					\
      }									\
      break;								\
    case S_MACRO:			/* end of a macro */		\
      in_comment = 0;							\
      pop_mstack();			/* pop arguments off stack */	\
      ptr = stack[--sindex].ptr;					\
      break;								\
    case S_NORMAL:			/* probably a variable */	\
      ptr = stack[--sindex].ptr;					\
      break;								\
    case S_RESET_PROMPT:		/* reset prompt on popping */	\
      plength = stack[sindex].base[HSIZE];				\
      strcpy(prompt,stack[sindex].base + HSIZE + 1);			\
      ptr = stack[--sindex].ptr;					\
      break;								\
    case S_TEMP:			/* temporary storage */		\
      free(stack[sindex].base);						\
      ptr = stack[--sindex].ptr;					\
      break;								\
    case S_WHILE:			/* a WHILE loop */		\
      in_comment = 0;							\
      ptr = stack[sindex].base;						\
      break;								\
    default:								\
      msg_1d("Illegal flag on I/O stack: %d\n",stack[sindex].flag);	\
      ptr = stack[--sindex].ptr;					\
      break;								\
   }   									\
   if(sm_interrupt == -3) {						\
      sm_interrupt = 0;							\
      hand_C(0);							\
   } else {								\
      sm_interrupt += 2;						\
   }

typedef struct {
   char *base,				/* start of string */
	*ptr,				/* pointer to string */
   	flag;				/* type of object being pushed */
} STACK;

char cchar = CC;			/* tell macros about CC */
extern char prompt[];			/* the input prompt */
static char buffer[BSIZE],		/* buffer for bottom of stack */
	    *eval_expr(),
  	    *ptr;			/* pointer to current stack */
extern int expand_vars,			/* expand variables everywhere */
  	   sm_interrupt,                   /* respond to ^C */
	   in_quote,			/* am I in a quoted string? */
	   noexpand,			/* shall I expand $ and keywords? */
	   plength,			/* length of prompt[] */
	   recurse_lvl,			/* how deeply nested are we? */
           sm_verbose;                     /* shall I be chatty ? */
static int get_input_line(),		/* return first char of a new line */
	   in_comment = 0,		/* am I in a comment? */
  	   sindex;			/* index to stack */
static STACK *stack;			/* i/o stack */
static unsigned nstack;			/* current size of stack */

int
input()
{
   char *buff,
        name[200];			/* name of variable */
   int c,
       value;			/* do we want the value of a variable? */

   if(*ptr == '\0') {
      if(sindex > 0) {
	 POP_STACK;
	 return(input());
      } else {
	 c = get_input_line();
      }
   } else {
      c = *ptr++;
   }
/*
 * deal with broken lines first
 */
   if(c == '\\') {
      if(sm_peek() == '\n') {		/* escaped newline */
	 ptr++;				/* `read' the \n */
	 return(input());
      }
   }
/*
 * If in a comment, simply look for the end of the line.
 * Comments are also ended by the end of macros, or DO or FOREACH loops
 */
   if(c == '\n') {			/* zero-length comment */
      in_comment = 0;
   }
   while(in_comment) {
      if(*ptr == '\0') {		/* get next character */
	 if(sindex > 0) {
	    POP_STACK;
	    return(input());
	 } else {
	    c = get_input_line();
	 }
      } else {
	 c = *ptr++;
      }
      if(c == '\n') {
	 in_comment = 0;
	 break;
      }
   }

   if(in_quote) {			/* in a quoted string */
      if(c == '\\') {			/* maybe an escaped " */
	 c = sm_peek();			/* look at next char */
	 if(c == '"') {			/* an escaped '"' */
	    ptr++;			/* 'read' that " */
	    return('"');
	 } else {
	    return('\\');
	 }
      } else if(c == '"') {
	 in_quote = 0;
	 noexpand--;
	 return(input());			/* get next character */
      }
   }

   if(noexpand > 0) {		/* probably don't do any expansions of \,$ */
      if(c == '$') {
	 if(expand_vars) {		/* always expand variables */
	    ;
	 } else if(sm_peek() == '!') {	/* We may have found $!var */
	    ptr++;			/* 'read' the ! */
	    if(in_quote) {		/* inside "" */ 
	       if(sm_peek() == '!') ptr++; /* $!!var */
	    } else {			/* in { } -- look for $!!var */
	       if(sm_peek() == '!') {
		  ptr++;			/* 'read' the '!' */
	       } else {
		  (void)unput('!');		/* the first one */
		  return('$');
	       }
	    }
	 } else {
	    return('$');
	 }
      } else {
	 return(c);
      }
   }

   switch(c) {
    case '\\':				/* maybe an escape for next char */
      c = sm_peek();
      if(c == '$' || c == '"' || c == cchar) {
	 ptr++;				/* `read' the $ or whatever */
	 return(c);
      } else if(c == 'n') {
	 ptr++;				/* `read' the n */
	 push("\n",S_NORMAL);		/* if unput('\n') we'll be reentrant */
	 return(input());
      } else if(c == '\\') {		/* might be \\n */
	 ptr++;				/* `read' the second \ */
	 if(sm_peek() == 'n') {		/* yes, \\n */
	    return('\\');
	 } else {
	    (void)unput('\\');		/* that's the second one */
	 }
	 return('\\');
      } else {				/* just return the backslash */
	 return('\\');
      }
    case '"':				/* begin a quoted string */
      noexpand++;
      in_quote = 1;
      return(input());
    case CC:				/* comment to end of line */
      in_comment = 1;
      return(input());
    case '$':				/* may be variable name */
      if(isspace(c = sm_peek())) {	/* just an isolated '$' */
	 return('$');
      } else {
	 value = 1;
	 buff = name;			/* pointer to that name */
	 while(sm_peek() == '!') ptr++;	/* $!variable */
	 if(sm_peek() == '?') {		/* $?variable */
	    ptr++;			/* eat the ? */
	    value = 0;			/* only want to know if it's defined */
	 }
	 if((c = sm_peek()) == '$') {	/* may be $?$ */
	    c = unput(input());
	 }
	 if(isdigit(c)) {		/* macro argument */
	    ptr++;			/* `read' digit */
	    buff = print_arg(c - '0',value);
	    name[0] = c;		/* needed for printing if verbose */
	    name[1] = '\0';
	 } else if(c == '(') {
	    int paren_level = 0;
	    int warned = 0;		/* have we warned of a truncation? */

	    ptr++;			/* `read' the '(' */
	    while((c = input()) != '\0') {
	       if(c == ')') {
		  if(paren_level-- <= 0) {
		     break;
		  }
	       } else if(c == '(') {
		  paren_level++;
	       } else if(c == '\n') {
		  (void)unput('\n');
		  *buff = '\0';
		  msg_1s("Missing closing ) in $(%s\n",name);
		  break;
	       }
	       if(buff - name >= sizeof(name) - 1) {
		  if(!warned) {
		     warned = 1;
		     *buff = '\0';
		     msg_1s("Variable %s is too long; truncating\n",name);
		  }
	       } else {
		  *buff++ = c;
	       }
	    }
	    *buff = '\0';
	    buff = eval_expr(name);
	 } else {
	    int warned = 0;		/* have we warned of a truncation? */
	    for(;;c = sm_peek()) {
	       if(c == '_' || isalnum(c)) {
		  *buff = *ptr++;
	       } else if(c == '$') {	/* allow for $$name */
		  *buff = input();
	       } else {
		  break;
	       }
	       
	       if(buff - name >= sizeof(name) - 1 && !warned) {
		  warned = 1;
		  *buff = '\0';
		  msg_1s("Variable %s is too long; truncating\n",name);
	       } else {
		  buff++;
	       }
	    }
	    *buff = '\0';
	    
	    buff = print_var(name);
	 }
      }
      
      if(sm_verbose > 3) {
	 msg_2s("$%s --> %s\n",name,buff);
	 (void)fflush(stdout);
      }
      
      if(value) {
	 if(buff[0] == '\0') {
	    msg_1s("variable %s is not defined\n",name);
	    buff = " ";
	 }
	 push(buff,S_NORMAL);
	 return(input());
      } else {
	 if(buff[0] == '\0') {
	    return('0');			/* not defined */
	 } else {
	    return('1');
	 }
      }
      /*NOTREACHED*/
    default:
      return(c);
   }
   /*NOTREACHED*/
}

/**************************************************/

int
unput(c)
int c;
{
   static char temp[2] = " ";

   if(ptr <= stack[sindex].base) {
      if(sm_verbose > 1) {
         fprintf(stderr,"Attempt to unput off bottom of buffer\n");
      }
      temp[0] = c;
      push(temp,S_NORMAL);
   } else {
      if(*--ptr != c) {
	 if(sindex == 0) {		/* there is a buffer for doing this */
	    *ptr = c;
	 } else {
	    if(sm_verbose > 1) {
	       fprintf(stderr,"Attempt to overwrite string in unput\n");
	    }
	    ptr++;				/* don't allow decrement */
	    temp[0] = c;
	    push(temp,S_NORMAL);
	 }
      }
   }
   return(c);
}

/*****************************************************************************/
/*
 * Get a new line from the keyboard, and return the first character.
 * Only one call to this routine can be active at a time as the stack is fully
 * popped before it is called, so static storage for command is safe
 */
static int
get_input_line()
{
   static char command[2*HSIZE];
   char *pptr;

   command[HSIZE] = plength;
   strncpy(&command[HSIZE + 1],prompt,HSIZE - 1);
   if(*(pptr = print_var("prompt2")) != '\0') {
      strcpy(prompt,pptr);
   } else {
      strcpy(prompt,">>");
   }
   plength = strlen(prompt) + 1;
   
   (void)sprintf(command,"%s\n",edit_hist());
   if(sm_interrupt) {			/* push will fail, so reset prompt */
      plength = command[HSIZE];
      strcpy(prompt,command + HSIZE + 1);
   } else {
      push(command,S_RESET_PROMPT);
   }
   return(input());
}

/**************************************************/
/*
 * evaluate a string
 */
static char *
eval_expr(str)
char *str;
{
   char name[40];			/* name of temp variable */
   char *sptr;
   int save_in_quote,save_noexpand;

   if((sptr = malloc(60 + strlen(str))) == NULL) {
      msg_1s("Can't allocate storage for %s\n",str);
      return("");
   }
   recurse_lvl++;
   sprintf(name,"_temp_expr%d",recurse_lvl);
   sprintf(sptr,"DEFINE %s (%s) SNARK ",name,str);
   push(sptr,S_TEMP);
   save_in_quote = in_quote; in_quote = 0;
   save_noexpand = noexpand; noexpand = 0;
   yyparse();
   in_quote = save_in_quote;
   noexpand = save_noexpand;
   return(print_var(name));
}

/**************************************************/
/*
 * allocate and initialise the stack
 */
void
init_stack()
{
   nstack = NSTACK;
   if((stack = (STACK *)malloc(nstack*sizeof(STACK))) == NULL) {
      msg("Can't alloc i/o stack - panic exit\n");
      (void)fflush(stdout); (void)fflush(stderr);
      abort();
   }

   sindex = 0;
   stack[0].base = buffer;
   stack[0].flag = S_ILLEGAL;
   ptr = buffer + sizeof(buffer) - 1;
   *ptr = '\0';
}

/**************************************************/
/*
 * push a string onto the stack
 */
void
push(str,flag)
char *str;
int flag;
{
   if(sm_interrupt) {
      return;
   }
/*
 * If *ptr == '\0', then the stack may be ready to be popped so let's do it
 * if we can. Of course, we can't pop the stack if macros or loops
 * are being processed, as they may not be finished with yet. If we don't try
 * to pop, the stack can fill with dead pointers, just waiting
 * to be discarded
 */
   if(*ptr == '\0') {
      switch(stack[sindex].flag) {
       case S_NORMAL:
	 ptr = stack[--sindex].ptr;
	 break;
       case S_TEMP:
	 free(stack[sindex].base);
	 ptr = stack[--sindex].ptr;
	 break;
      }   
   }

   if(sindex == nstack - 1) {
      nstack *= 2;
      if(sm_verbose > 1 || nstack > NSTACK) {
	 msg("Extending i/o stack\n");
      }
      if((stack = (STACK *)realloc((char *)stack,nstack*sizeof(STACK)))
	 							== NULL) {
	 msg("Can't realloc i/o stack - panic exit\n");
	 (void)fflush(stdout); (void)fflush(stderr);
	 abort();
      }
   }
   stack[sindex++].ptr = ptr;		/* save old pointer */
   stack[sindex].base = ptr = str;
   stack[sindex].flag = flag;
}

/**************************************************/
/*
 * Just look at the next character. Note that it is always safe to ptr++
 * after a call to sm_peek().
 */
int
sm_peek()
{
   if(*ptr == '\0') {
      if(sindex > 0) {
	 POP_STACK;
	 return(sm_peek());
      } else {
	 return(unput(get_input_line()));
      }
   } else {
      return(*ptr);
   }
}

/***********************************************/
/*
 * This function forces a return from the current macro,
 * first popping other stuff (loops, variables, ...), so it looks
 * as if the macro came to its end naturally.
 */
void
mac_return()
{
   while(sindex > 0 && stack[sindex].flag != S_MACRO) {
      switch(stack[sindex].flag) {
       case S_DO:			/* end of a do loop */
	 pop_dstack();
	 free(stack[sindex--].base);
	 break;
       case S_FOREACH:			/* end of a foreach loop */
	 pop_fstack();
	 free(stack[sindex--].base);
	 break;
       case S_WHILE:			/* a while loop */
       case S_TEMP:			/* temporary storage */
	 free(stack[sindex--].base);
	 break;
       case S_NORMAL:			/* probably a variable */
	 sindex--;
	 break;
       default:
	 msg_1d("Illegal flag on I/O stack: %d\n",stack[sindex].flag);
	 sindex--;
	 break;
      }
   }

   if(sindex == 0) {
      lexflush();			/* force return to prompt */
      push("\n",S_NORMAL);
      if(sm_verbose > 1) {
	 msg("RETURN: Returning to prompt\n");
      }
   } else {				/* end of a macro */
      in_comment = 0;
      if(sm_verbose > 1) {
	 msg_1s("RETURN: returning from %s\n",print_arg(0,1));
      }
      pop_mstack();			/* pop arguments off stack */
      ptr = stack[--sindex].ptr;
   }
}

/***********************************************/

void
lexflush()
{
   for(;sindex > 0;sindex--) {
      switch(stack[sindex].flag) {
       case S_DO:
       case S_FOREACH:
       case S_TEMP:
       case S_WHILE:
         free(stack[sindex].base);
	 break;
       case S_RESET_PROMPT:
	 plength = stack[sindex].base[HSIZE];
	 strcpy(prompt,stack[sindex].base + HSIZE + 1);
	 break;
       default:
	 break;
      }
   }
   in_comment = 0;
   flush_dstack();			/* clean out DO stack */
   flush_fstack();			/* clean out FOREACH stack */
   flush_mstack();			/* clean out macro call stack */
   ptr = buffer + sizeof(buffer) - 1;
   *ptr = '\0';
}

/************************************************/

int
buff_is_empty()
{
   for(;;ptr++) {
      if(!isspace(*ptr)) break;
   }
      
   if(*ptr == '\0') {
      if(sindex > 0) {
	 POP_STACK;
	 return(buff_is_empty());
      } else {
	 return(1);
      }
   } else {
      return(0);
   }
}

/**************************************************/

void
prompt_user(string)
char string[];
{
   if(buff_is_empty() || sm_verbose > 1) msg_1s("%s\n",string);
}

/**************************************************/
/*
 * print a traceback of the stack
 */
void
dump_stack()
{
   char *cont;				/* to be continued? */
   char *cptr;				/* just a pointer */
   int first = 1;			/* first line of trace */
   char line[50 + 1];			/* current line */
   int line_num;			/* line number */
   char *lptr;				/* pointer to start of current line */
   char *name;				/* name of macro */
   char *point;				/* current point in macro */
   int i,j;
   
   for(i = sindex;i > 0;i--) {
      if(stack[i].flag == S_MACRO) {
	 if(first) {
	    first = 0;
	    msg_1s("Traceback:\n%12s line\n","Macro");
	 }
/*
 * Find the macro's name
 */
	 if((name = macro_name(stack[i].base)) == NULL) {
	    name = "(unknown)";
	 }
	 if((cptr = get_macro(name,(int *)NULL,(int *)NULL,(int *)NULL))
								     == NULL) {
	    line_num = 1;
	 } else {			/* count initial comment lines */
	    for(line_num = 1;*cptr == cchar;line_num++) {
	       while(*cptr != '\0' && *cptr++ != '\n') continue;
	    }
	 }
/*
 * and its line number and current line
 */
	 point = (i == sindex) ? ptr : stack[i].ptr;
	 if(*(point - 1) == '\n') {	/* we want the line that we just read*/
	    point--;
	 }
	 cptr = lptr = stack[i].base;
	 while(cptr < point) {
	    if(*cptr++ == '\n') {
	       line_num++;
	       lptr = cptr;
	    }
	 }

	 for(j = 0;j < 50;j++,lptr++) {
	    if((line[j] = *lptr) == '\0' || *lptr == '\n') {
	       break;
	    }
	 }
	 line[j] = '\0';
	 cont = (j == 50) ? "..." : "";

	 msg_1s("%12s ",name);
	 msg_1d("%3d : ",line_num);
	 msg_2s("%s %s\n",line,cont);
      }
   }
}

/*****************************************************************************/
/*
 * end a while loop
 */
void
end_while()
{
   char *lptr;				/* local copy of global ptr */
   
   while(sindex > 0 && stack[sindex].flag != S_WHILE) { /* pop the BREAK */
      lptr = ptr;
      while(lptr >= stack[sindex].base && isspace(*lptr)) lptr--;
      if(*lptr == '}') {
	 lptr--;
	 while(lptr >= stack[sindex].base && isspace(*lptr)) lptr--;
      }
      
      lptr -= 4;
      if(lptr >= stack[sindex].base &&
	 ((strncmp(lptr,"BREAK",5) == 0) || strncmp(lptr,"break",5) == 0)) {

	 switch(stack[sindex].flag) {
	  case S_TEMP:
	    free(stack[sindex].base);
	    break;
	  case S_NORMAL:		/* nothing to do */
	    break;
	  default:
	    msg_1d("Saw %d while popping BREAKs; please inform RHL\n",
		   stack[sindex].flag);
	    break;
	 }
	 ptr = stack[--sindex].ptr;
      } else {
	 break;				/* not a break */
      }
   }

   if(stack[sindex].flag != S_WHILE) {
      msg("You cannot use BREAK except in WHILE loops\n");
      push("\n",S_NORMAL);
      return;
   } else if(sindex <= 0) {
      msg("I cannot pop a S_WHILE off an empty stack. Please tell RHL\n");
      return;
   }

   free(stack[sindex].base);
   ptr = stack[--sindex].ptr;
}
