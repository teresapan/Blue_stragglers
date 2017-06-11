#include "copyright.h"
/*
 * handle allocation of TOK_LIST for the parsers
 */
#include <stdio.h>
#include "options.h"
#include "vectors.h"
#include "yaccun.h"
#include "sm_declare.h"
#define DELTA_TOK 30			/* initial value of TOK_LIST.nmax */

TOK_LIST *
getlist(dd)
TOK_LIST *dd;				/* list to be created/extended */
{
   int i;

   if(dd == NULL) {			/* created */
      if((dd = (TOK_LIST *)malloc(sizeof(TOK_LIST))) == NULL) {
	 msg("Can't get space for list\n");
	 return(NULL);
      }
      dd->nmax = DELTA_TOK;		/* number of items allocated */
      dd->nitem = 0;			/* none used yet */
      if((dd->i_space = malloc((unsigned)dd->nmax*sizeof(VECTOR_OR_STRING)))
								     == NULL) {
	 msg("Can't get space for t_list items\n");
	 free((char *)dd);
	 return(NULL);
      }
      if((dd->i_type = (int *)malloc((unsigned)dd->nmax*sizeof(int))) == NULL){
	 msg("Can't get space for types of t_list items\n");
	 free((char *)dd->i_space);
	 free((char *)dd);
	 return(NULL);
      }
   } else {				/* extended */
      free((char *)dd->i_list);			/* it's outdated */
      dd->nmax += DELTA_TOK;			/* new number of items */
      if((dd->i_space = realloc(dd->i_space,
		       (unsigned)dd->nmax*sizeof(VECTOR_OR_STRING))) == NULL) {
	 msg("Can't realloc space for t_list items\n");
	 free((char *)dd);
	 return(NULL);
      }
      if((dd->i_type =
	  (int *)realloc(dd->i_type,(unsigned)dd->nmax*sizeof(int))) == NULL) {
	 msg("Can't realloc space for types of t_list items\n");
	 free((char *)dd->i_space);
	 free((char *)dd);
	 return(NULL);
      }
   }

   if((dd->i_list = (VECTOR_OR_STRING **)malloc(sizeof(char *)*
					       (unsigned)dd->nmax)) == NULL) {
      msg("Can't get space for t_list pointers\n");
      free(dd->i_space);
      free((char *)dd);
      return(NULL);
   }
   for(i = 0;i < dd->nmax;i++) {		/* assign pointers */
      dd->i_list[i] = (VECTOR_OR_STRING *)dd->i_space + i;
   }
   return(dd);
}

/************************************************/

void
freelist(dd)
TOK_LIST *dd;				/* list to free */
{
   if(dd != NULL) {
      if(dd->i_type != NULL) {
	 free((char *)dd->i_type);
      }
      if(dd->i_space != NULL) {
	 free(dd->i_space);
      }
      if(dd->i_list != NULL) {
	 free((char *)dd->i_list);
      }
      free((char *)dd);
   }
}
