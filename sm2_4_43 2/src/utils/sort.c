#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "sm_declare.h"
/*
 * Given a vector, and (maybe an array of indices), return a sorted list
 * (maybe of indices).
 *
 * The sort is a Shell sort, see e.g. Press et.al. Numerical Recipes.
 *
 * Actually, SM is a victim of its own success and a Shell sort is a
 * Bad Idea for large arrays; sort_flt has been replaced by a call to qsort()
 */
#include <math.h>

extern int sm_interrupt;		/* respond to ^C */

typedef struct {
   union {
      REAL f;
      LONG l;
      char *s;
   } val;
   int ind;
} SORT_IND;

static int
compar_flt(const void *va, const void *vb)
{
   const REAL a = *(REAL *)va;
   const REAL b = *(REAL *)vb;

   return((a < b) ? -1 : (a > b) ? 1 : 0);
}

static int
compar_flt_ind(const void *va, const void *vb)
{
   const SORT_IND *a = *(SORT_IND **)va;
   const SORT_IND *b = *(SORT_IND **)vb;

   return((a->val.f < b->val.f) ? -1 : (a->val.f > b->val.f) ? 1 : 0);
}

static int
compar_long_ind(const void *va, const void *vb)
{
   const SORT_IND *a = *(SORT_IND **)va;
   const SORT_IND *b = *(SORT_IND **)vb;

   return((a->val.l < b->val.l) ? -1 : (a->val.l > b->val.l) ? 1 : 0);
}

static int
compar_str_ind(const void *va, const void *vb)
{
   const SORT_IND *a = *(SORT_IND **)va;
   const SORT_IND *b = *(SORT_IND **)vb;

   return(strcmp(a->val.s, b->val.s));
}

/*****************************************************************************/
/*
 * Template for routines that sort indices
 */
#define SORT_INDS(FNAME, TYPE, UNAME, COMPAR) \
void \
FNAME(vec,ind,dimen) \
TYPE vec[];			/* array of vectors to sort */ \
int ind[],			/* index array for sorted vectors */ \
    dimen;			/* dimension of vector */ \
{ \
   int i; \
   SORT_IND **vec_ind, *vec_ind_s; \
 \
   if(dimen <= 1) { \
      ind[0] = 0; \
      return; \
   } \
/* \
 * Allocate the array to sort \
 */ \
   if((vec_ind = malloc(dimen*sizeof(SORT_IND *))) == NULL || \
      (vec_ind_s = malloc(dimen*sizeof(SORT_IND))) == NULL) { \
      msg("Unable to allocate auxiliary arrays to sort\n"); \
 \
      if(vec_ind != NULL) { \
	 free(vec_ind); \
      } \
      return; \
   } \
/* \
 * Unpack the input vectors \
 */ \
   for(i = 0;i < dimen;i++) { \
      vec_ind[i] = vec_ind_s + i; \
      vec_ind[i]->val.UNAME = vec[i]; \
      vec_ind[i]->ind = i; \
   } \
/* \
 * Do the work \
 */ \
   qsort(vec_ind, dimen, sizeof(vec_ind[0]), COMPAR); \
/* \
 * Pack the results \
 */ \
   for(i = 0;i < dimen;i++) { \
      /* vec[i] = vec_ind[i]->val.UNAME; don't sort input array */ \
      ind[i] = vec_ind[i]->ind; \
   } \
/* \
 * And cleanup \
 */ \
   free(vec_ind_s); \
   free(vec_ind); \
}

/*****************************************************************************/
/*
 * Define the sort_*_ind functions
 */
SORT_INDS(sort_flt_inds, REAL, f, compar_flt_ind)
SORT_INDS(sort_long_inds, LONG, l, compar_long_ind)
SORT_INDS(sort_str_inds, char *, s, compar_str_ind)

/*********************************************************/

void
sort_flt(vec,dimen)
REAL vec[];			/* array of vectors to sort */
int dimen;			/* dimension of vector */
{
   qsort(vec, dimen, sizeof(REAL), compar_flt);
}
