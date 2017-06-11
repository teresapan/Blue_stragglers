#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "charset.h"
#include "stack.h"
#include "tree.h"
#include "sm_declare.h"

typedef struct {
   char *macro,				/* body of macro */
   	*mstart;			/* points after initial comment */
   char nargs_min;			/* minimum number of arguments */
   char nargs_max;			/* maximum number of arguments */
   char arg_eol;			/* last argument extends to EOL */
} MNODE;

extern char cchar;			/* comment character */
static char *mmacro;			/* used by mmake */
extern int expand_vars,			/* always expand variables? */
  	   sm_interrupt,		/* handle ^C interrupts */
	   noexpand,			/* should I expand tokens? */
	   sm_verbose;			/* Should I be chatty? */
static int all,				/* Even write macros starting
					   with 2 comment characters */
	   nnargs_min,			/* used by mmake */
	   nnargs_max,			/*  "   "   " "  */
	   aarg_eol,			/*  "   "   " "  */
  	   stop_list;			/* stop listing macros? */
static void apropos(),
	    mfind(),
  	    mkill(),
  	    mlist(),
	    mwrite();
static Void *mmake();
static TREE mm = { NULL, mkill, mmake };

int
define(name,macro,nargs_min,nargs_max,arg_eol)
char *name,
     *macro;
int nargs_min,
    nargs_max,
    arg_eol;
{
   mmacro = macro;
   
   if(!strcmp(macro,"delete") || !strcmp(macro,"DELETE")) {
      delete_node(name,&mm);
   } else {
      nnargs_min = nargs_min;
      nnargs_max = nargs_max;
      aarg_eol = arg_eol;
      insert_node(name,&mm);
   }
   return(0);
}


/*****************************************************************************/
/*
 * like define, but take a nargs string (e.g. "101")
 */
int
define_s(name,macro,str)
char *name,
     *macro,
     *str;
{
   int nargs_min,
       nargs_max,
       arg_eol;
   int i = 0;

   arg_eol = nargs_max = 0;
   nargs_min = -1;
   
   if(isdigit(str[0])) {		/* there are arguments to the macro */
      if(isdigit(str[1])) {		/* with a minimum set */
	 if(isdigit(str[2])) {		/* last arg goes to eol */
	    arg_eol = str[i++] - '0';
	 }
	 nargs_min = str[i++] - '0';
      }
      nargs_max = str[i++] - '0';

      if(str[i] != '\0' &&              /* this is a borland C++ 3.1 bug... */
         isdigit(str[i])) {
	 msg_1s("Extra digits in nargs for %s\n",name);
      }
   }
   if(nargs_min < 0) nargs_min = nargs_max; /* not otherwise specified */
   
   return(define(name,macro,nargs_min,nargs_max,arg_eol));
}

/***************************************************************/
/*
 * make new node
 */
static Void *
mmake(name,nn)
char *name;
Void *nn;			/* old node */
{
   char *end;				/* end of macro text */
   MNODE *node;

   if(nn == NULL) {
      if((node = (MNODE *)malloc(sizeof(MNODE))) == NULL) {
	 msg_1s("malloc returns NULL in mmake() for %s\n",name);
	 return(NULL);
      }
   } else {
      node = (MNODE *)nn;
      if(node->macro != NULL) free(node->macro);	/* free old macro */
   }
   node->nargs_min = nnargs_min;
   node->nargs_max = nnargs_max;
   node->arg_eol = aarg_eol;

   if((node->macro = malloc((unsigned)strlen(mmacro) + 2)) == NULL) {
      msg_1s("malloc returns NULL in mmake() for %s\n",name);
      free((char *)node);
      return(NULL);
   }
   (void)strcpy(node->macro,mmacro);
/*
 * ensure that macro ends with a space or newline, so that we don't
 * pop the macro stack (and lose local variables/vectors) while parsing
 * it (note that we allocated an extra byte in node->macro to allow for this).
 * We'd like to always use a space, but that won't solve the problem
 * for if statements --- but in that case a \n is OK so use that.
 */
   end = node->macro + strlen(node->macro) - 1;
   while(end > node->macro && (*end == ' ' || *end == '\t')) end--;
   if(end > node->macro) {
      end++;
      if(*(end - 1) == '}') {
	 *end++ = '\n'; *end = '\0';
      } else if(*(end - 1) == '\n') {
	 ;
      } else {
	 *end++ = ' '; *end = '\0';
      }
   }

   if(node->macro[0] == cchar) {		/* skip initial comment */
      for(node->mstart = node->macro;*node->mstart != '\0';node->mstart++) {
          if(*node->mstart == '\n') {
	     node->mstart++;
	     if(*node->mstart != cchar) {
		break;
	     }
	  }
       }
   } else {
      node->mstart = node->macro;
   }

   return((Void *)node);
}

/**********************************************/

static void
mkill(nn)
Void *nn;
{
   MNODE *node;

   if((node = (MNODE *)nn) == NULL) return;
   free(node->macro);
   free((char *)node);
}

/***************************************************************/
/*
 * execute macro name
 */
int
execute(name)
char *name;
{
   MNODE *node;
   
   if((node = (MNODE *)get_rest(name,&mm)) == NULL) {
      return(-1);
   }
/*
 * push_mstack() gets the arguments, and pushes them onto the call stack
 */
   push_mstack(name,node->nargs_min,node->nargs_max,node->arg_eol);
   push(node->mstart,S_MACRO);
   return(0);
}

/***************************************************************/
#define LSIZE 82			/* enough to read a line */

static char line_buff[LSIZE + 1] = "";	/* line buffer */
static int begin,	                /* alphabetical beginning and */
	   endd;		        /* end of range to print */

void
macrolist(b,e)          /* list all the defined macros in left-subtree order */
int b,                  /* but only if name[0] is in range [begin-endd]   */
    e;
{
   begin = b;				/* set up external variables */
   endd = e;
   stop_list = 0;

   (void)more((char *)NULL);			/* initialise more */

   list_nodes(&mm,mlist);
}

static void
mlist(name,nn)                  /* list a macro, only print first line */
char *name;
Void *nn;
{
   char c;
   int  i,j;
   MNODE *node = (MNODE *)nn;

   if(stop_list || name[0] < begin || name[0] > endd) {
      return;
   }
   if(sm_verbose == 0) {			/* may skip some macros */
      if(node->macro[0] == cchar && node->macro[1] == cchar) {
	 return;
      }
   }

   for(i = 0;i < LSIZE && (c = name[i]) != '\0';i++) {
      line_buff[i] = name[i];
   }
   for(;i < 12;i++) {
      line_buff[i] = ' ';
   }
   for(j = 0;i < LSIZE && (c = node->macro[j]) != '\n' && c != '\0';) {
      line_buff[i++] = node->macro[j++];
   }
   line_buff[i++] = '\n';
   line_buff[i] = '\0';			/* we kept a last character for it */
   if(more(line_buff) < 0) stop_list = 1; /* write out that line */
}

/***************************************************************/

void
macro_apropos(pattern)
char *pattern;				/* pattern to match */
{
   if(match_pattern("",pattern,(char **)NULL) == NULL) {/* initialise regexp */
      return;
   }
   (void)more((char *)NULL);			/* initialise more */
   stop_list = 0;

   list_nodes(&mm,apropos);
}

/*
 * list a macro if its help string matches a pattern,
 * (only print first line).
 */
static void
apropos(name,nn)
char *name;
Void *nn;
{
   char c,
   	*end;
   int  i,j;
   MNODE *node = (MNODE *)nn;

   end = node->mstart;
   if(stop_list || (match_pattern(name,(char *)NULL,(char **)NULL) == NULL &&
		    match_pattern(node->macro,(char *)NULL,&end) == NULL)) {
      return;
   }

   for(i = 0;i < LSIZE && (c = name[i]) != '\0';i++) {
      line_buff[i] = name[i];
   }
   for(;i < 12;i++) {
      line_buff[i] = ' ';
   }
   for(j = 0;i < LSIZE && (c = node->macro[j]) != '\n' && c != '\0';) {
      line_buff[i++] = node->macro[j++];
   }
   line_buff[i++] = '\n';
   line_buff[i] = '\0';			/* we kept a last character for it */
   if(more(line_buff) < 0) stop_list = 1; /* write out that line */
}

/***************************************************************/
/*
 * echo definition of macro name, starting at node
 */
int
what_is(name)
char name[];
{
   char c;
   int i,j;
   MNODE *node;

   if((node = (MNODE *)get_rest(name,&mm)) == NULL) { /* unknown macro */
      return(-1);
   }

   (void)more((char *)NULL);			/* Initialise more */
   msg("Macro: ");
   if(node->nargs_max > 0) {
      if(node->nargs_min != node->nargs_max) {
	 msg_1d("variable number of arguments (%d <= narg",node->nargs_min);
	 msg_1d(" <= %d)",node->nargs_max);
      } else if(node->nargs_max == 1) {
	 msg("1 argument");
      } else {
	 msg_1d("%d arguments",node->nargs_max);
      }
      if(node->arg_eol) {
	 msg(" last extends to end of line");
      }
   }
   if(more("\n") < 0) return(0);
   for(i = j = 0;(c = line_buff[i++] = node->macro[j++]) != '\0';) {
      if(i >= LSIZE || c == '\n') {
	 line_buff[i] = '\0';		/* we kept a last character for it */
	 if(more(line_buff) < 0) return(0); /* write out that line */
	 i = 0;
      }
   }
   line_buff[i-1] = '\n';
   line_buff[i] = '\0';			/* we kept a last character for it */
   (void)more(line_buff);		/* write out that line */
   return(0);
}

/**************************************************************/
/*
 * Get the text of a macro
 */
char *
get_macro(name,nargs_min,nargs_max,arg_eol)
char *name;
int *nargs_min;			/* minimim number of arguments */
int *nargs_max;			/* maximum number of arguments */
int *arg_eol;			/* last argument extends to end of line */
{
   MNODE *temp;

   if((temp = (MNODE *)get_rest(name,&mm)) == NULL) {
      if(nargs_min != NULL) *nargs_min = 0;
      if(nargs_max != NULL) *nargs_max = 0;
      if(arg_eol != NULL) *arg_eol = 0;
      return(NULL);
   } else {
      if(nargs_min != NULL) *nargs_min = temp->nargs_min;
      if(nargs_max != NULL) *nargs_max = temp->nargs_max;
      if(arg_eol != NULL) *arg_eol = temp->arg_eol;
      return(temp->macro);
   }
}

/***********************************************************/
/*
 * external variable for macro i/o routines
 */
static FILE *fil;			/* stream for disk i/o */

/***************************************************************/

void
write_macros(ffil,aall)
FILE *ffil;			/* stream of file to contain macros */
int aall;				/* write them all? */
{
   all = aall;
   fil = ffil;
   stop_list = 0;
   list_nodes(&mm,(void (*)())mwrite);	/* write all the defined macros */
}

/***************************************************************/
/*
 * write macro name to disk. If the variable all was not set, don't
 * write macros that start with two comment characters
 */
static void
mwrite(name,nn)
char *name;
Void *nn;
{
   register int i,
                len,
                writing_narg;		/* are we writing the narg string? */
   MNODE *node = (MNODE *)nn;

   if(!all && node->macro[0] == cchar && node->macro[1] == cchar) return;

   fprintf(fil,"%-10s",name);
   if(node->nargs_max != 0) {
      writing_narg = 0;
      putc(' ',fil);
      if(node->arg_eol) {
	 putc('1',fil);
	 writing_narg = 1;
      }
      if(node->nargs_min != node->nargs_max || writing_narg) {
	 putc(node->nargs_min + '0',fil);
	 writing_narg = 1;
      }
      if(node->nargs_max || writing_narg) {
	 putc(node->nargs_max + '0',fil);
      }
   }
   fprintf(fil,"\t");
   len = strlen(node->macro);
   for(i = 0;i < len;i++) {      
      (void)putc(node->macro[i],fil);
      if(node->macro[i] == '\n') fprintf(fil,"\t\t");
   }
   (void)putc('\n',fil);
}

/***************************************************************/

int
write_one_macro(name,file,append)
char *name,                           /* name of macro to write */
     *file;                           /* file to write it to */
int append;				/* should I append to file? */
{
   static char old_file[60] = "";	/* previous macro file */
   int i,len;
   MNODE *node;

   if(append) {			/* force append to bottom of file */
      (void)strncpy(old_file,file,60);
   }

   if(!strcmp(old_file,file)) {
      if((fil = fopen(file,"a")) == NULL) {
	 msg_1s("Can't reopen %s in write_one_macro\n",file);
	 return(-1);
      }
   } else {			/* different file */
      if(would_clobber(file)) {
	 return(-1);
      }
      (void)strncpy(old_file,file,60);
      if((fil = fopen(file,"w")) == NULL) {
	 msg_1s("Can't open %s in write_one_macro\n",file);
	 return(-1);
      }
   }
   
   if((node = (MNODE *)get_rest(name,&mm)) == NULL) {
      msg_1s("%s is not defined\n",name);
      return(-1);
   }

   fprintf(fil,"%-10s",name);
   if(node->nargs_max != 0) {
      int writing_narg = 0;		/* are we writing the narg string? */

      putc(' ',fil);
      if(node->arg_eol) {
	 putc('1',fil);
	 writing_narg = 1;
      }
      if(node->nargs_min != node->nargs_max || writing_narg) {
	 putc(node->nargs_min + '0',fil);
	 writing_narg = 1;
      }
      if(node->nargs_max || writing_narg) {
	 putc(node->nargs_max + '0',fil);
      }
   }
   fprintf(fil,"\t");

   len = strlen(node->macro);
   for(i = 0;i < len;i++) {      
      (void)putc(node->macro[i],fil);
      if(node->macro[i] == '\n') fprintf(fil,"\t\t");
   }
   (void)putc('\n',fil);

   (void)fclose(fil);
   return(0);
}

/***************************************************************/
/*
 * undefine all macros in file
 */
int
undefine_macros(file)
char file[];                    /* name of file containing macros */
{
   char name[NAMESIZE];
         
   if((fil = fopen(file,"r")) == NULL) {
      msg_1s("can't find file %s\n",file);
      return(-1);
   }
   
   while(fgets(line_buff,LSIZE,fil) != NULL) {
      if(line_buff[0] == cchar || isspace(line_buff[0])) {
	 continue;			/* comment or continuation */
      }
      (void)str_scanf(line_buff,"%s",name);
      (void)define(name,"delete",0,0,0); /* delete that macro */
   }

   (void)fclose(fil);
   return(0);
}

/***************************************************************/

void
read_macros(ffil)
FILE *ffil;				/* Stream containing macros */
{
   char line[2*LSIZE + 1],		/* input line */
   	*lptr,				/* pointer to line[] */
        *macro,				/* scratch buffer for macros */
	*mend,				/* end of macro[] */
	*mptr,				/* pointer to macro[] */
	*mstart_of_line,		/* start of current line in macro */
        name[NAMESIZE],
        nargs[10 + 1];			/* number of arguments string */
   int  c,
        len,				/* length of macro */
        tabsize = 0;			/* size of a tab */

   if(*(lptr = print_var("tabsize")) != '\0') {
      tabsize = atoi(lptr);
   }
   if(tabsize == 0) tabsize = 8;

   if((macro = malloc(MACROSIZE + 1 + 9)) == NULL) {
      msg("Panic: cannot allocate macro scratch buffer\n");
      abort();
   }
   mend = macro + MACROSIZE + 9 - 1 - tabsize; /* +9 to make it a pretty size */
   fil = ffil;

   while(fgets(line,2*LSIZE,fil) != NULL) {
#ifdef vms				/* work around a VMS bug */
      len = strlen(line);
      if(line[len-1] != '\n') {
	 (void)getc(fil);		/* eat the \n that VMS missed */
	 line[len] = '\n';		/* and put it in */
	 line[len + 1] = '\0';
      }
#endif
      if(line[0] == cchar) {
	 if(sm_verbose) {
	    if(line[1] != cchar || sm_verbose > 1) {
	       (void)fputs(line,stdout);
	    }
	 }
	 continue;
      }
      *name = *macro = '\0';
      (void)str_scanf(line,"%s",name);
      len = strlen(name) + 1;
      if(len == 1) {			/* a blank line */
	 continue;
      }
      
      while((c = line[len]) == ' ' || c == '\t') { /* discard blanks */
	 len++;
      }

      if(isdigit(c)) {			/* there are arguments to the macro */
	 int i;
	 for(i = 0;i < 10 && isdigit(line[len]);i++) {
	    nargs[i] = line[len++];
	 }
	 nargs[i] = '\0';

	 while((c = line[len]) == ' ' || c == '\t') { /* discard blanks */
	    len++;
	 }
      } else {
	 nargs[0] = '\0';
      }

      mstart_of_line = macro;
      for(lptr = &line[len],mptr = macro;*lptr != '\0';) {
	 if((c = *lptr++) == '\t') {
	    *mptr++ = ' ';
	    while((mptr - mstart_of_line)%tabsize != 0) *mptr++ = ' ';
	 } else {
	    *mptr++ = c;
	 }
      }

      while((c = ungetc(getc(fil),fil)) != EOF) { /* continuation line */
	 if(c == cchar) {
	    (void)fgets(line,2*LSIZE,fil);
	    if(sm_verbose) {
	       if(line[1] != cchar || sm_verbose > 1) {
		  (void)fputs(line,stdout);
	       }
	    }
	    continue;
	 } else if(!isspace(c) && line[strlen(line) - 1] == '\n') {
	    break;
	 }
         lptr = fgets(line,2*LSIZE,fil);
	 if(*lptr == ' ' || *lptr == '\t') lptr++;	/* discard 1 blank */
	 if(*lptr == ' ' || *lptr == '\t') lptr++;	/* and try to lose 2 */
	 if(*lptr == '\0') {		/* end of file with no \n */
	    *mptr = '\0';
	    break;
	 }
	 /*
	  * Do we have room for that line?
	  */
	 if(mptr + strlen(lptr) >= mend) {
	    const int offset = mptr - macro;
	    const int size = mend - macro + MACROSIZE;
	    if(sm_verbose > 1) {
	       msg_1d("Extending MACRO READ scratch buffer to %dbytes\n", size);
	    }
	    
	    if((macro = realloc(macro, size)) == NULL) {
	       msg("Panic: cannot reallocate macro scratch buffer\n");
	       abort();
	    }
	    mptr = macro + offset;
	    mend = macro + size;
	 }

	 mstart_of_line = mptr;
	 while(mptr < mend && *lptr != '\0') {
	    if((c = *lptr++) == '\t') {
	       *mptr++ = ' ';
	       while((mptr - mstart_of_line)%tabsize != 0) *mptr++ = ' ';
	    } else {
	       *mptr++ = c;
	    }
	 }
	 if(*lptr == '\0' && *(lptr - 1) != '\n') { /* no closing \n */
	    c = getc(fil);		/* read next char */
	    if(isspace(c) && mptr < mend) {
	       *mptr++ = c;		/* if it's a space it'll be deleted
					   when re-read, so push it now */
	    }
	    ungetc(c, fil);
	 }
      }
      if(*(mptr - 1) == '\n') *(mptr - 1) = '\0';
      if(*name != '\0') (void)define_s(name,macro,nargs);
   }

   free(macro);
}

/******************************************************/
/*
 * Now the code to handle macro arguments. Each macro pushes its
 * arguments onto a stack of elements MSTACK. Don't push down stack
 * until after processing arguments, though, as arguments from old
 * macro may be passed to new one.
 */
#define ASIZE 80			/* maximum length of an argument */
#define DSTACKSIZE 10			/* amount to increment stack by */
#define NARG 10				/* number of args, name in args[0] */
#define NSIZE 80			/* maximum length of name */

typedef struct mstack {
   struct mstack *prev, *next;		/* links for free/active list */
   int nargs;				/* number of arguments */
   char *args[NARG],			/* arguments */
   	name[NSIZE];			/* storage for args[0] */
   int size_lvar;			/* size of local variable array */
   char **local_var;			/* the local variables */
   int size_lvec;			/* size of local vector array */
   char **local_vec;			/* the local vectors */
} MSTACK;

static int mindex = -1;			/* index into stack */
static MSTACK **mstack;			/* the stack itself */
static MSTACK *freed = NULL, *active = NULL; /* stack frames */
static int n_mstack = -1;		/* max number of elements on stack */

static MSTACK *
new_mframe()
{
   MSTACK *frame;
   
   if(freed == NULL) {
      if((frame = malloc(sizeof(MSTACK))) == NULL) {
	 msg("Panic: cannot allocate macro stack frame\n");
	 abort();
      }
      frame->args[0] = frame->name;
   } else {
      frame = freed; freed = freed->next;
   }

   frame->next = active; frame->prev = NULL;
   if(active != NULL) {
      active->prev = frame;
   }
   active = frame;

   return(frame);
}

static void
del_frame(MSTACK *frame)
{
   if(frame->next != NULL) {
      frame->next->prev = frame->prev;
   }
   if(frame->prev != NULL) {
      frame->prev->next = frame->next;
   }
   if(frame == active) {
      active = frame->next;
   }

   frame->next = freed; freed = frame;
}

void
push_mstack(name,nargs_min,nargs_max,arg_eol)
char *name;
int nargs_min, nargs_max, arg_eol;
{
   char buff[80];			/* buffer to talk to user */
   int c,
       i;
   MSTACK *mnew;			/* new value of mindex */

   mnew = new_mframe();
      
   (void)strcpy(mnew->name,name);
   mnew->nargs = nargs_max;
   mnew->local_var = mnew->local_vec = NULL;
   mnew->size_lvar = mnew->size_lvec = 0;
   if(nargs_max > 0) {
      if((mnew->args[1] = malloc((unsigned)nargs_max*ASIZE)) == NULL) {
	 msg_1s("Can't allocate argument list for %s\n",name);
	 abort();
      }
      for(i = 1;i <= nargs_max;i++) {
	 mnew->args[i] = mnew->args[1] + (i - 1)*ASIZE;
	 if(sm_interrupt) {			/* respond to ^C */
	    mnew->args[i][0] = '\0';
	 } else {
	    if(i <= nargs_min) {
	       (void)sprintf(buff,"argument %d to %s:  ",i,name);
	       prompt_user(buff);
	    }
	    noexpand++;			/* turn off expansions */
	    expand_vars++;		/* still expand variables */
	    if(i == nargs_max && arg_eol) {
	       while((c = input()) == ' ' || c == '\t') continue;
	       if(c == EOF) {
		  *mnew->args[i] = '\0';
	       } else {
		  (void)unput(c);
		  (void)mscanline(mnew->args[i],ASIZE);
	       }
	    } else {
	       (void)mscanstr(mnew->args[i],ASIZE);
	    }
	    noexpand--;			/* reset noexpand */
	    expand_vars--;		/* and expand_vars */
	 }
      }
   }

   if(++mindex >= n_mstack) {
      if(n_mstack < 0) {
	 n_mstack = 0;
      }
      n_mstack += DSTACKSIZE;
      if(mnew == 0) {
	 if((mstack = (MSTACK **)malloc(n_mstack*sizeof(MSTACK *))) == NULL) {
      	    msg("Can't alloc macro stack - panic exit\n");
      	    (void)fflush(stderr); (void)fflush(stdout);
      	    abort();
      	 }
      } else {
	 if(sm_verbose > 1) {
	    msg("Extending macro stack\n");
	 }
	 if((mstack = (MSTACK **)realloc((char *)mstack,
					n_mstack*sizeof(MSTACK *))) == NULL) {
      	    msg("Can't realloc macro stack - panic exit\n");
      	    (void)fflush(stderr); (void)fflush(stdout);
      	    abort();
      	 }
      }
      for(i = 0;i < DSTACKSIZE;i++) {
	 mstack[n_mstack - i - 1] = NULL;
      }
   }

   mstack[mindex] = mnew;
}

/***********************************************************/
/*
 * Add a new macro argument (or replace a pre-existing one)
 */
void
add_marg(n,val)
unsigned int n;				/* Which argument? */
char *val;				/* value of argument */
{
   int i;

   if(mindex < 0) {
      msg_1d("You are not in a macro, so you can't define $%d\n",(int)n);
      return;
   }

   if(!strcmp(val,"delete") || !strcmp(val,"DELETE")) {
      if(n <= mstack[mindex]->nargs) {
	 mstack[mindex]->args[n][0] = '\0';
	 return;
      } else {
	 if(sm_verbose) {
	    msg_1d("Argument %d to ",(int)n);
	    msg_1s("%s is not defined\n",mstack[mindex]->args[0]);
	 }
	 return;
      }
   }
   
   if(n > mstack[mindex]->nargs) {	/* have to realloc arguments */
      if(mstack[mindex]->nargs == 0) {
	 if((mstack[mindex]->args[1] = malloc((unsigned)n*ASIZE)) == NULL) {
	    msg_1s("Can't allocate storage for arguments to %s\n",
						   mstack[mindex]->args[0]);
	    return;
	 }
      } else {
	 if((mstack[mindex]->args[1] = realloc(mstack[mindex]->args[1],
					      (unsigned)n*ASIZE)) == NULL) {
	    msg_1s("Can't reallocate storage for arguments to %s\n",
						   mstack[mindex]->args[0]);
	    mstack[mindex]->nargs = 0;
	    return;
	 }
      }
      for(i = 1;i <= n;i++) {
	 mstack[mindex]->args[i] = mstack[mindex]->args[1] + (i - 1)*ASIZE;
	 if(i > mstack[mindex]->nargs) {
	    mstack[mindex]->args[i][0] = '\0';
	 }
      }
      mstack[mindex]->nargs = n;
   }
   strncpy(mstack[mindex]->args[n],val,ASIZE-1);
}

/*****************************************************************************/
/*
 * create a local variable
 */
void
make_local_variable(name)
char *name;
{
   int i, next;

   if(mindex < 0) {
      msg_1s("You are not in a macro, so you can't make $%s local\n",name);
      return;
   }
/*
 * Allocate the list of local variable names
 */
   if(mstack[mindex]->local_var == NULL) {
      mstack[mindex]->size_lvar = 10;
      if((mstack[mindex]->local_var =
	  (char **)malloc((mstack[mindex]->size_lvar + 1)*sizeof(char *))) ==
									NULL) {
	 msg("Cannot allocate memory for local variable list\n");
	 mstack[mindex]->size_lvar = 0;
	 return;
      }
      next = 0;
	 
      for(i = next;i <= mstack[mindex]->size_lvar;i++) {
	 mstack[mindex]->local_var[i] = NULL;
      }
   } else {
      for(i = 0;mstack[mindex]->local_var[i] != NULL;i++) {
	 if(strcmp(mstack[mindex]->local_var[i],name) == 0) {
	    msg_1s("variable %s is already local\n",name);
	    return;
	 }
      }
      next = i;
      if(next >= mstack[mindex]->size_lvar) {
	 mstack[mindex]->size_lvar *= 2;
	 if((mstack[mindex]->local_var =
	     (char **)realloc(mstack[mindex]->local_var,
		     (mstack[mindex]->size_lvar + 1)*sizeof(char *))) == NULL) {
	    msg("Cannot reallocate memory for local variable list\n");
	    mstack[mindex]->size_lvar = 0;
	    return;
	 }
	 
	 for(i = next;i <= mstack[mindex]->size_lvar;i++) {
	    mstack[mindex]->local_var[i] = NULL;
	 }
      }
   }

   if((mstack[mindex]->local_var[next] = malloc(strlen(name) + 1)) == NULL) {
      msg_1s("cannot allocate memory for local variable %s's name\n",name);
   } else {
     strcpy(mstack[mindex]->local_var[next],name);
     setvar_local(name);
  }
}

/*****************************************************************************/
/*
 * create a local vector
 */
void
make_local_vector(name)
char *name;
{
   int i, next;

   if(mindex < 0) {
      msg_1s("You are not in a macro, so you can't make %s local\n",name);
      return;
   }
/*
 * Allocate the list of local vector names
 */
   if(mstack[mindex]->local_vec == NULL) {
      mstack[mindex]->size_lvec = 10;
      if((mstack[mindex]->local_vec =
	  (char **)malloc((mstack[mindex]->size_lvec + 1)*sizeof(char *))) ==
									NULL) {
	 msg("Cannot allocate memory for local vector list\n");
	 mstack[mindex]->size_lvec = 0;
	 return;
      }
      next = 0;
	 
      for(i = next;i <= mstack[mindex]->size_lvec;i++) {
	 mstack[mindex]->local_vec[i] = NULL;
      }
   } else {
      for(i = 0;mstack[mindex]->local_vec[i] != NULL;i++) {
	 if(strcmp(mstack[mindex]->local_vec[i],name) == 0) {
	    if((name[0] != '_' || name[1] != '_') || sm_verbose > 1) {
	       msg_1s("vector %s is already local\n",name);
	    }
	    return;
	 }
      }
      next = i;
      if(next >= mstack[mindex]->size_lvec) {
	 mstack[mindex]->size_lvec *= 2;
	 if((mstack[mindex]->local_vec =
	     (char **)realloc(mstack[mindex]->local_vec,
		     (mstack[mindex]->size_lvec + 1)*sizeof(char *))) == NULL) {
	    msg("Cannot reallocate memory for local vector list\n");
	    mstack[mindex]->size_lvec = 0;
	    return;
	 }
	 
	 for(i = next;i <= mstack[mindex]->size_lvec;i++) {
	    mstack[mindex]->local_vec[i] = NULL;
	 }
      }
   }

   if((mstack[mindex]->local_vec[next] = malloc(strlen(name) + 1)) == NULL) {
      msg_1s("cannot allocate memory for local vector %s's name\n",name);
   } else {
     strcpy(mstack[mindex]->local_vec[next],name);
     make_vector_local(name);
  }
}

/***********************************************************/
/*
 * Pop the top of the stack, and free appropriate memory
 */
void
pop_mstack()
{
   if(mindex < 0) {			/* can happen following a ^C */
      return;
   }
   if(mstack[mindex]->nargs > 0) {
      free(mstack[mindex]->args[1]);
   }
   if(mstack[mindex]->size_lvar > 0) {	/* delete local variables */
      int i;
      for(i = 0;mstack[mindex]->local_var[i] != NULL;i++) {
	 delvar_local(mstack[mindex]->local_var[i]);
	 free(mstack[mindex]->local_var[i]);
      }
      free((char *)mstack[mindex]->local_var);
   }
   if(mstack[mindex]->size_lvec > 0) {	/* delete local vectors */
      int i;
      for(i = 0;mstack[mindex]->local_vec[i] != NULL;i++) {
	 free_vector_local(mstack[mindex]->local_vec[i]);
	 free(mstack[mindex]->local_vec[i]);
      }
      free((char *)mstack[mindex]->local_vec);
   }

   del_frame(mstack[mindex--]);
}

/***********************************************************/
/*
 * flush out the stack, and free appropriate memory
 */
void
flush_mstack()
{
   while(mindex >= 0) {
      pop_mstack();
   }
}

/**************************************************************/
/*
 * return the value of an argument
 */
char *
print_arg(n,value)
int n;					/* current value of $n */
int value;				/* do we want a value, or existance? */
{
   if(mindex < 0) {
      msg_1d("You are not in a macro, so $%d is undefined\n",n);
      return(" ");
   }
   if(n > mstack[mindex]->nargs) {
      if(!value) return("");
      switch (n) {
       case 1:
	 msg("1st"); break;
       case 2:
	 msg("2nd"); break;
       case 3:
	 msg("3rd"); break;
       default:
	 msg_1d("%dth",n);
	 break;
      }
      msg_1s(" argument to %s is not defined\n",mstack[mindex]->args[0]);
      return(" ");
   } else {
      if(value)
	return(mstack[mindex]->args[n]);
      else
	return(mstack[mindex]->args[n]);
   }
}


/***************************************************/
/*
 * Return name of current macro: Note that if it has already been
 * popped of the stack this will give the wrong answer
 */
char *
current_macro()
{
   if(mindex < 0) {
      return(NULL);
   }
   return(mstack[mindex]->args[0]);	/* points to storage in macro tree */
}


/***************************************************/
/*
 * Print name of macro, given its starting address
 */
char *
macro_name(name)
char *name;				/* name of macro */
{
   mmacro = name;
   
   stop_list = 0;
   list_nodes(&mm,(void (*)())mfind);	/* print name given starting ptr */
   return(mmacro == name ? NULL : mmacro);
}

static void
mfind(name,nn)
char *name;
Void *nn;
{
   MNODE *node = (MNODE *)nn;

   if(node->mstart == mmacro) {
      stop_list = 1;
      mmacro = name;
   }
}

