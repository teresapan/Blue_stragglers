#include "copyright.h"
/*
 * Handle DO/FOREACH loops
 *
 * Push information onto a stack, and return the next element of the list
 * from the top of the appropriate stack
 */
#include <stdio.h>
#include "options.h"
#include "vectors.h"			/* needed by yaccun.h */
#include "yaccun.h"
#include "sm_declare.h"

#define DSTACKSIZE 5			/* amount to increment stacks by */
#define NSIZE 80			/* maximum length of name */

typedef struct {
   REAL var,
   	end,
   	inc;
   char name[NSIZE];			/* name of loop */
} DSTACK;

typedef struct {
   char name[NSIZE];			/* name of loop */
   int ctr;				/* counter through list */
   TOK_LIST *list;
} FSTACK;

static char *do_name;
extern int sm_verbose;			/* how chatty should I be? */
static int dindex = -1,			/* index to stack */
  	   findex = -1;			/* index to stack */
static REAL do_var,			/* used for efficiency */
  	    do_end,
  	    do_inc;
static DSTACK *dstack;			/* the stack itself */
static FSTACK *fstack;			/* the stack itself */
static unsigned n_dstack = 0,		/* max number on do stack */
		n_fstack = 0;		/* max number on foreach stack */

void
push_dstack(name,start,end,inc)
char *name;
double start,
       end,
       inc;
{
   if(n_dstack == 0) {
      n_dstack += DSTACKSIZE;
      if((dstack = (DSTACK *)malloc(n_dstack*sizeof(DSTACK))) == NULL) {
	 msg("Can't alloc do stack - panic exit\n");
	 (void)fflush(stderr); (void)fflush(stdout);
	 abort();
      }
   } else if(dindex >= 0) {
      dstack[dindex].var = do_var;	/* save old value */
   }
   
   if(++dindex >= n_dstack) {
      n_dstack += DSTACKSIZE;
      if(sm_verbose > 1) msg("Extending do stack\n");
      if((dstack = (DSTACK *)realloc((char *)dstack,n_dstack*sizeof(DSTACK)))
								 == NULL) {
	 msg("Can't realloc do stack - panic exit\n");
	 (void)fflush(stderr); (void)fflush(stdout);
	 abort();
      }
   }
      
   (void)strcpy(dstack[dindex].name,name);
   do_name = dstack[dindex].name;
   dstack[dindex].var = do_var = start;
   dstack[dindex].end = do_end = end;
   dstack[dindex].inc = do_inc = inc;
}

/***********************************************************/
/*
 * Empty the DO stack
 */
void
flush_dstack()
{
   for(;dindex >= 0;dindex--) {
      setvar(dstack[dindex].name,"delete");
   }
}

/***********************************************************/
/*
 * Pop the DO stack
 */
void
pop_dstack()
{
   setvar(dstack[dindex].name,"delete");
   if(--dindex >= 0) {
      do_name = dstack[dindex].name;
      do_var = dstack[dindex].var;
      do_end = dstack[dindex].end;
      do_inc = dstack[dindex].inc;
   }
}

/**************************************************************/
/*
 * DEFINE and increment the DO variable
 */
int
next_do()
{
   char word[80];
   
   if(do_var/do_inc > do_end/do_inc + 1e-4) {
      pop_dstack();
      return(-1);
   } else {
      (void)sprintf(word,"%.20g",do_var);
      setvar(do_name,word);
      do_var += do_inc;
      return(0);
   }
}

/*************************************************************/
/*
 * Now the code for FOREACH loops
 */
void
push_fstack(name,list)
char *name;
TOK_LIST *list;
{
   if(++findex >= n_fstack) {
      n_fstack += DSTACKSIZE;
      if(findex == 0) {
	 if((fstack = (FSTACK *)malloc(n_fstack*sizeof(FSTACK))) == NULL) {
	    msg("Can't alloc foreach stack - panic exit\n");
	    (void)fflush(stderr); (void)fflush(stdout);
	    abort();
	 }
      } else {
	 if(sm_verbose > 1) msg("Extending foreach stack\n");
	 if((fstack = (FSTACK *)realloc((char *)fstack,
					n_fstack*sizeof(FSTACK))) == NULL) {
	    msg("Can't realloc foreach stack - panic exit\n");
	    (void)fflush(stderr); (void)fflush(stdout);
	    abort();
	 }
      }
   }
      
   (void)strcpy(fstack[findex].name,name);
   fstack[findex].ctr = 0;
   fstack[findex].list = list;
}

/***********************************************************/
/*
 * Empty FOREACH stack and free memory
 */
void
flush_fstack()
{
   for(;findex >= 0;findex--) {
      freelist(fstack[findex].list);
      setvar(fstack[findex].name,"delete");
   }
}


/***********************************************************/
/*
 * Empty FOREACH stack and free memory
 */
void
pop_fstack()
{
   setvar(fstack[findex].name,"delete");
   freelist(fstack[findex--].list);
}

/**************************************************************/
/*
 * DEFINE the FOREACH variable and increment the counter
 */
int
next_foreach()
{
   if(fstack[findex].ctr >= fstack[findex].list->nitem) {
      pop_fstack();
      return(-1);
   } else {
      setvar(fstack[findex].name,
		     fstack[findex].list->i_list[fstack[findex].ctr++]->str);
      return(0);
   }
}
