#include "copyright.h"
/*
 * These routines maintain a binary tree.
 * The tree used is a Weight-balanced tree (BB[1-sqrt(2)/2]).
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "tree.h"
#include "sm_declare.h"

#define LOCAL 01			/* is this a local node? */
#define WEIGHT(N) ((N == NULL) ? 1 : (N)->weight)	/* weight of a node */

extern int sm_interrupt,		/* handle ^C interrupts */
           sm_verbose;			/* be chatty? */
static int local;			/* has new node got restricted scope?*/
static TNODE *i_delete_node(),
	     *lrot(),*rrot(),
	     *makenode(),
	     *i_insert_node();
static Void *i_get_rest();
static void i_kill_tree(),
	    i_list_nodes();


/***************************************************************/
/*
 * add something to tree (maybe local)
 */
void
insert_node(name,funcs)
char *name;
TREE *funcs;
{
   local = 0;				/* a node with global scope */
   funcs->root = i_insert_node(name,funcs->root,funcs);
}

void
insert_node_local(name,funcs)
char *name;
TREE *funcs;
{
   local = 1;				/* a node with restricted scope */
   funcs->root = i_insert_node(name,funcs->root,funcs);
}

static TNODE *
i_insert_node(name,node,funcs)
char *name;
TNODE *node;
TREE *funcs;
{
   int compare;

   if(node == NULL)                     /* make new node */
      return(makenode(name,funcs));

   compare = strncmp(name,node->name,NAMESIZE);

   if(compare == 0) {                            /* redefinition */
      if(local) {
	 TNODE *new;

	 if((new = makenode(name,funcs)) == NULL) {
	    free((char *)node);
	    return(NULL);
	 }
	 new->scope_list = node;
	 new->left = node->left;
	 new->right = node->right;
	 node = new;
      } else {
	 if((node->rest = (*funcs->make)(node->name,node->rest)) == NULL) {
	    free((char *)node);
	    return(NULL);
	 }
      }
      return(node);
   } else if(compare < 0) {
      node->left = i_insert_node(name,node->left,funcs); /* in left branch */
   } else {
      node->right = i_insert_node(name,node->right,funcs); /* to right */
   }

   node->weight = WEIGHT(node->left) + WEIGHT(node->right);
   
   if(99*WEIGHT(node->left) > 70*WEIGHT(node)) {
      if(99*WEIGHT(node->left->left) > 41*WEIGHT(node->left)) {
      	 node = rrot(node);
      } else {
      	 node->left = lrot(node->left);
      	 node = rrot(node);
      }
   } else if(99*WEIGHT(node->left) < 29*WEIGHT(node)) {
      if(99*WEIGHT(node->left) < 58*WEIGHT(node->right)) {
      	 node = lrot(node);
      } else {
      	 node->right = rrot(node->right);
      	 node = lrot(node);
      }
   }
   return(node);
}

/***************************************************************/
/*
 * make new node
 */
static TNODE *
makenode(name,funcs)
char *name;
TREE *funcs;
{
   TNODE *newnode;

   if((newnode = (TNODE *)malloc(sizeof(TNODE))) == NULL) {
      msg("malloc returns NULL in makenode()\n");
      return(NULL);
   }

   (void)strcpy(newnode->name,name);
   newnode->left = NULL;
   newnode->right = NULL;
   newnode->weight = 2;		/* 1 for each sub-tree (here NULL) */
   if(local) {
      newnode->scope_list = (TNODE *)1;	/* i.e. not NULL */
   } else {
      newnode->scope_list = NULL;
   }

   if((newnode->rest = (*funcs->make)(newnode->name,NULL)) == NULL) {
      free((char *)newnode);
      return(NULL);
   }

   return(newnode);
}

/***************************************************************/
/*
 * Two functions, lrot() and rrot() to do left and right rotations
 *
 * lrot :	  N		    R
 *		 / \		   / \
 *		L   R	  -->	  N   r
 *		   / \		 / \
 *		  l   r		L   l
 */
static TNODE *
lrot(node)
TNODE *node;
{
   TNODE *temp;

   temp = node;
   node = node->right;
   temp->right = node->left;
   node->left = temp;
/*
 * Now update the weights
 */
   temp->weight = (WEIGHT(temp->left) + WEIGHT(temp->right));
   node->weight = (WEIGHT(node->left) + WEIGHT(node->right));

   return(node);
}

static TNODE *
rrot(node)
TNODE *node;
{
   TNODE *temp;

   temp = node;
   node = node->left;
   temp->left = node->right;
   node->right = temp;
/*
 * Now update the weights
 */
   temp->weight = (WEIGHT(temp->left) + WEIGHT(temp->right));
   node->weight = (WEIGHT(node->left) + WEIGHT(node->right));

   return(node);
}

/************************************************/

void
delete_node(name,funcs)
char *name;
TREE *funcs;
{
   local = 0;
   funcs->root = i_delete_node(name,funcs->root,funcs);
}

void
delete_node_local(name,funcs)
char *name;
TREE *funcs;
{
   local = 1;
   funcs->root = i_delete_node(name,funcs->root,funcs);
}

static TNODE *
i_delete_node(name,node,funcs)
char *name;
TNODE *node;
TREE *funcs;
{
   int compare;
   TNODE *temp;
   
   if(node == NULL) return(NULL);
   
   if((compare = strcmp(name,node->name)) < 0) {
      node->left = i_delete_node(name,node->left,funcs);
   } else if(compare > 0) {
      node->right = i_delete_node(name,node->right,funcs);
   } else {				/* found it! */
      if(node->scope_list != NULL) {	/* a local variable */
	 if(!local) {
	    if(sm_verbose > 1) {
	       msg_1s("Attempt to delete a local quantity %s\n",name);
	    }
	    return(node);
	 }

	 if(node->scope_list != (TNODE *)1) { /* a real node */
	    (*funcs->kill)(node->rest);
	    temp = node->scope_list;
	    temp->left = node->left;
	    temp->right = node->right;
	    temp->weight = node->weight;
	    free((char *)node);
	    return(temp);
	 }
      }
      
      if(node->left == NULL) {		/* that is easy */
	 (*funcs->kill)(node->rest);
	 temp = node->right;
	 free((char *)node);
      	 node = temp;
      } else if(node->right == NULL) {	/* so is that */
         (*funcs->kill)(node->rest);
      	 temp = node->left;
	 free((char *)node);
      	 node = temp;
      } else {				/* but now we must do some work */
         if(WEIGHT(node->left) > WEIGHT(node->right)) {
            if(WEIGHT(node->left->left) < WEIGHT(node->left->right)) {
               node->left = lrot(node->left);
            }
            node = rrot(node);
            node->right = i_delete_node(name,node->right,funcs);
         } else {
            if(WEIGHT(node->right->left) > WEIGHT(node->right->right)) {
               node->right = rrot(node->right);
            }
            node = lrot(node);
            node->left = i_delete_node(name,node->left,funcs);
         }
      }    	 
   }
   if(node != NULL) {
      node->weight = WEIGHT(node->left) + WEIGHT(node->right);
   }

   return(node);
}

/*******************************************/

void
list_nodes(funcs,function)		/* Apply `function' to each node */
TREE *funcs;
void (*function)();			/* function to execute at each node */
{
   i_list_nodes(funcs->root,function);
}

static void
i_list_nodes(node,function)		/* list nodes */
TNODE *node;
void (*function)();			/* function to execute at each node */
{
   if(node == NULL) return;

   i_list_nodes(node->left,function);
   if(sm_interrupt) return;
   (*function)(node->name,node->rest);
   i_list_nodes(node->right,function);
}

/**************************************************************/
/*
 * Get the `rest' of a node
 */
static TNODE *lnode = NULL;		/* last node retrieved */

Void *
get_rest(name,funcs)
char *name;
TREE *funcs;
{
   return(i_get_rest(name,funcs->root));
}

static Void *
i_get_rest(name,node)
char *name;
TNODE *node;
{
   int compare;

   if(node == NULL) {                   /* unknown macro */
      return(NULL);
   }

   if((compare = strcmp(name,node->name)) == 0) {	/* we're there! */
      lnode = node;
      return(node->rest);
   } else if(compare < 0) {
      return(i_get_rest(name,node->left));
   } else {
      return(i_get_rest(name,node->right));
   }
}

/*
 * return the "rest" field of the node on the scope list of the last node
 * accessed by any of the tree functions. Use this with care; in particular
 * lnode may be dangling after a delete operation.
 */
Void *
get_lnode_scope_rest()
{
   return(lnode == NULL ||
	  lnode->scope_list == NULL || lnode->scope_list == (TNODE *)1 ?
					       NULL : lnode->scope_list->rest);
}

/*
 * Kill an entire tree structure
 */
void
kill_tree(funcs)
TREE *funcs;
{
   i_kill_tree(funcs->root,funcs);
   funcs->root = NULL;
}

static void
i_kill_tree(node,funcs)
TNODE *node;
TREE *funcs;
{
   if(node == NULL) return;

   i_kill_tree(node->left,funcs);
   i_kill_tree(node->right,funcs);
   (*funcs->kill)(node->rest);
   free((char *)node);
}
