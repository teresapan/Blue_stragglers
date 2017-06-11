#include "copyright.h"
/*
 * This is the code to handle the variable tree
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "tree.h"
#include "sm_declare.h"

#define IS_INTERNAL 01			/* variable is an internal SM one */
#define USE_INTERNAL 02			/* use its internal value */

typedef struct {
   char flag;
   union {
      char *s;
      int i;
   } value;
} VNODE;

static char *vvalue;
static int stop_list;			/* stop listing variables? */
static void save_var(),
	    vlist(),
	    vkill();
static Void *vmake(),
	    *vmake_internal();
static TREE vv = { NULL, vkill, vmake};

void
setvar(name,value)
char *name;
char value[];
{
   vvalue = value;

   if(isdigit(name[0])) {
      add_marg((unsigned int)(name[0] - '0'),value);
   } else if(strcmp(value,"delete") == 0) {
      delete_node(name,&vv);
      if(vvalue == NULL) {		/* it was an internal variable */
	 setvar_internal(name);
      }
   } else {
      insert_node(name,&vv);
   }
}

/***************************************************************/
/*
 * Set a variable to always reflect the current value of a (C) SM
 * variable
 */
void
setvar_internal(name)
char *name;
{
   vv.make = vmake_internal;
   insert_node(name,&vv);
   vv.make = vmake;
}

/***************************************************************/
/*
 * Create or delete a local variable
 */
void
setvar_local(name)
char *name;
{
   vvalue = "";
   insert_node_local(name,&vv);
}

void
delvar_local(name)
char *name;
{
   delete_node_local(name,&vv);
}

/***************************************************************/
static int begin,		/* alphabetical beginning and */
	   endd;		/* end of range to print */

void
varlist(b,e)		/* list defined variables in left-subtree order */
int   b,		/* but only if in range [begin-endd]   */
      e;
{
   begin = b;				/* set up external variables */
   endd = e;
   stop_list = 0;

   list_nodes(&vv,(void (*)())vlist);
}

/*
 * return value of variable
 */
char *
print_var(name)
char *name;
{
   VNODE *node;

   if((node = (VNODE *)get_rest(name,&vv)) == NULL) {/* unknown variable */
      if(isdigit(name[0])) {
	 return(print_arg(name[0] - '0',1));
      } else {
	 return("");
      }
   } else {
      if(node->flag & USE_INTERNAL) {
	 return(get_variable(node->value.i));
      } else {
	 return(node->value.s);
      }
   }
}

/*
 * Does a variable exist?  Return 1 for regular variables, 2 for internals
 */
int
exists_var(name)
char *name;
{
   VNODE *node;

   if((node = (VNODE *)get_rest(name,&vv)) == NULL) {/* unknown variable */
      if(isdigit(name[0])) {
	 return(*print_arg(name[0] - '0', 0) == '0' ? 0 : 1);
      } else {
	 return(0);
      }
   } else {
      if(node->flag & USE_INTERNAL) {
	 return(2);
      } else {
	 return(1);
      }
   }
}

/***************************************************************/
/*
 * Save all variables to a file, file pointer fil
 */
static FILE *fil;

void
save_variables(ffil)
FILE *ffil;
{
   fil = ffil;
   list_nodes(&vv,(void (*)())save_var);
}

static void
save_var(name,nn)
char *name;
Void *nn;
{
   if(*name == '_') return;		/* don't save variables starting _ */
   if(((VNODE *)nn)->flag & IS_INTERNAL) {
      return;
   }
   
   fprintf(fil,"%s\t%s\n",name,((VNODE *)nn)->value.s);
}

/***************************************************************/

static void
vkill(nn)
Void *nn;
{
   if(((VNODE *)nn)->flag & IS_INTERNAL) vvalue = NULL;
   free((char *)nn);
}

/***************************************************************/
/*
 * print the variable at a node
 */
static void
vlist(name,nn)
char *name;
Void *nn;
{
   if(stop_list) return;
   if(((VNODE *)nn)->flag & USE_INTERNAL) return;
   
   if(name[0] > endd) return;
   if(name[0] >= begin) {
      msg_2s("%-10s %s",name,((VNODE *)nn)->value.s);
      if(more("\n") < 0) stop_list = 1;	/* get 20 lines at a time */
   }
}

/***************************************************************/
/*
 * make new node
 */
static Void *
vmake(name,nn)
char *name;
Void *nn;			/* current value of node */
{
   VNODE *node;
   int flag = 0;

   if(nn != NULL) {
      flag = ((VNODE *)nn)->flag;
      flag &= ~USE_INTERNAL;		/* turn it off */
      free((char *)nn);
   }
   if((node = (VNODE *)malloc(sizeof(VNODE) + strlen(vvalue) + 1)) == NULL) {
      msg_1s("malloc returns NULL in vmake() for %s\n",name);
      return(NULL);
   }
   node->value.s = (char *)(node + 1);	/* skip the VNODE stuff */
   
   (void)strcpy(node->value.s,vvalue);
   node->flag = flag;

   return((Void *)node);
}

/*
 * make a new internal node
 */
static Void *
vmake_internal(name,nn)
char *name;
Void *nn;			/* current value of node */
{
   int i;
   VNODE *node;

   if((i = index_variable(name)) < 0) {
      msg_1s("%s is not a valid internal variable\n",name);
      return(NULL);
   }

   vvalue = "";
   if((node = (VNODE *)vmake(name,nn)) == NULL) {
      return(NULL);
   }

   node->flag |= IS_INTERNAL | USE_INTERNAL;
   node->value.i = i;
   
   return((Void *)node);
}
