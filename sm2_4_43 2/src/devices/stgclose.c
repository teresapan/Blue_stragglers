#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "tree.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm_declare.h"
/*
 * stg_close() Close the stdgraph kernel.  Free all storage associated with the
 * stdgraph descriptor.
 *
 */
#define FREE(I) { free((char *)I); I = NULL; }
#define SIZE 200

int abort_plot = 0;			/* abort current plot */
extern int sm_verbose;			/* be chatty? */
static int insert();
static char *sy_macro();

void
stg_close()
{
   char buff1[SIZE],		/* Buffers for writing command to system */
	buff2[SIZE+1];
   int c,
       i,j,k,
       n;			/* number of argument to SY */

   if(g_sg == NULL) {
      return;
   }
   
   stg_ctrl("CW");
   ttflush(g_out);

   if(g_sg->sg_outfile[0] != '\0') {		/* file was specified */
      (void)close(g_out);
      if(abort_plot) {
	 unlink(g_sg->sg_outfile);
      } else if(ttygets(g_tty,"SY",buff1,SIZE) != 0) {
         for(i = j = 0;buff1[i] != '\0' && j < SIZE;i++) {
	    if(buff1[i] == '$') {
	       if(buff1[i+1] == 'F') {	/* substitute file */
		  j = insert(sy_macro("F"),buff2,j);
	          i++;			/* skip 'F' */
	       } else if(buff1[i+1] == '{') { /* ${NAME} */
		  char *name;
		  
		  i += 2;		/* skip ${ */
		  name = &buff1[i];
		  for(;buff1[i] != '\0' && !isspace(buff1[i]);i++) {
		     if(buff1[i] == '}') {
			break;
		     }
		  }
		  if(buff1[i] != '}') {
		     msg_1s("Graphcap macro %s needs a closing }\n",name);
		     i--;
		     continue;
		  }
		  buff1[i] = '\0';
		  j = insert(sy_macro(name),buff2,j);
	       } else if(buff1[i+1] == '"') { /* $"prompt" */
		  i++;			/* skip " */
		  while(buff1[i + 1] != '\0' && buff1[i + 1] != '"') {
		     msg_1d("%c",buff1[++i]);
		  }
	          while(j < SIZE && (c = getchar()) != EOF && c != '\n') {
	             buff2[j++] = c;
	          }
	       } else if(isdigit(buff1[i+1]) ||
			 (buff1[i+1] == '[' &&
			  	isdigit(buff1[i+2]) && buff1[i+3] == ']')) {
		  int warn = 1;		/* warn about missing args */
		  if(buff1[i+1] == '[') {
		     i++;
		     warn = 0;
		  }
	          if((n = buff1[i+1] - '0') >= g_sg->sg_ac) {
	             if(warn || sm_verbose > 1)
		       msg_1d("Missing argument %d to SY\n",n);
	          } else {
	             for(k = 0;j < SIZE && (c = g_sg->sg_av[n][k]) != '\0';
		  						    j++,k++) {
	                buff2[j] = c;
		     }
	          }
	          i++;			/* skip digit */
		  if(!warn) i++;	/* skip ']' too */
	       } else {
	          buff2[j++] = buff1[i];
	       }
	    } else if(buff1[i] == '\n') {
	       buff2[j] = '\0';
	       if(system(buff2) == 127) { /* execute command */
		  msg_1s("Failed to execute command \"%s\"\n",buff2);
	       }
	       j = 0;			/* reset pointer */
	    } else if(buff1[i] == '\\') {	/* escape next char */
	       buff2[j++] = buff1[++i];
	    } else {
	       buff2[j++] = buff1[i];
	    }
	 }
	 if(j > 0) {
	    buff2[j] = '\0';
	    if(system(buff2) == 127) {	/* execute command */
	       msg_1s("Failed to execute command \"%s\"\n",buff2);
	    }
	 }
      }
   }
   FREE(g_sg);
   if(g_tty != NULL) FREE(g_tty);
}

/*
 * Change back to ascii terminal
 */
void
stg_idle()
{
   if(g_tty != NULL) stg_ctrl("GD");	/* this can be called while setting
					 up a new g_tty*/
   ttflush(g_out);
}

/*
 * Deal with SY macros
 */
#define MSIZE 40

typedef struct {
   char value[MSIZE];
} SY_NODE;

static char *vvalue;
static void tkill();
static Void *tmake();
static TREE tt = { NULL, tkill, tmake};

void
def_sy_macro(name,value)
char *name;				/* name to be defined */
char *value;				/* and its value */
{
   vvalue = value;

   insert_node(name,&tt);
}

void
kill_sy_macros()
{
   kill_tree(&tt);
}

/***************************************************************/
/*
 * make a new SY macro definition node
 */
static Void *
tmake(name,nn)
char *name;
Void *nn;			/* current value of node */
{
   SY_NODE *node;
   
   if(nn == NULL) {
      if((node = (SY_NODE *)malloc(sizeof(SY_NODE))) == NULL) {
	 msg_1s("malloc returns NULL in tmake() for %s\n",name);
	 return(NULL);
      }
      if(*vvalue == '$') {
	 (void)strncpy(node->value,print_var(&vvalue[1]),MSIZE);
      } else {
	 (void)strncpy(node->value,vvalue,MSIZE);
      }
      return((Void *)node);
   } else {
      return(nn);			/* n.b. no redefinition */
   }
}

/***************************************************************/

static void
tkill(nn)
Void *nn;
{
   free((char *)nn);
}

/***************************************************************/

/*
 * expand the definition of a SY macro, if it is defined
 */
static char *
sy_macro(name)
char *name;				/* name of macro */
{
   SY_NODE *node;

   if(strcmp(name,"F") == 0) {		/* special case */
      return(g_sg->sg_outfile);
   }
   
   if((node = (SY_NODE *)get_rest(name,&tt)) == NULL) {/* unknown variable */
      msg_1s("Unknown SY macro: ${%s}\n",name);
      return("");
   } else {
      return(node->value);
   }
}

/*
 * Insert "value" after buff[i]
 */
static int
insert(value,buff,i)
char *value;
char buff[];
int i;
{
   char c;
   int j;
   
   for(j = 0;i < SIZE && (c = value[j]) != '\0';i++,j++) {
      buff[i] = c;
   }
   return(i);
}
