#include "copyright.h"
/*
 * Print the current position of the cursor when (almost) any key is hit. If
 * you hit a digit, it will be followed by a linefeed, otherwise succesive
 * positions overwrite (digit => you'll get a number of values).
 * If you hit E, e, Q, or q, you will be leave the cursor. With E or e,
 * a "relocate x y" is put on the history, exiting with q or Q (or ^C)
 * leaves no history.
 *
 *
 * set_cursor_vector sets a pair of vectors from the position of the cursor
 * when `p' is hit; exit with `q'
 *
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "charset.h"
#include "vectors.h"
#include "devices.h"
#include "sm.h"
#include "sm_declare.h"

extern int sm_image_cursor;		/* is this an image cursor? */
extern int sm_interrupt;		/* Has someone hit ^C ? */

/*****************************************************************************/
/*
 * Note that this function is responsible for enabling/disabling graphics
 */
int
sm_cursor(flag)
int flag;			/* if 1, get value from image;
				   if -1, simply return current position */
{
   float ux,uy,			/* user coordinates */
	 val;			/* value in image */
   int key,			/* character hit by user */
       px,py;			/* SCREEN coords of cursor */

   sm_image_cursor = flag > 0 ? 1 : 0;

   if(flag < 0) {			/* relocate to current cursor position*/
      sm_enable();
      (void)CURSOR(1,&px,&py);		/* read the position */
      ux = (px - ffx)/fsx;		/* SCREEN --> user coords */
      uy = (py - ffy)/fsy;
      sm_relocate(ux,uy);

      return(0);
   }

   do {						/* wait for e or q */
      sm_enable();
      key = CURSOR(0,&px,&py);	                /* read the position */
      ux = (px - ffx)/fsx;			/* SCREEN --> user coords */
      uy = (py - ffy)/fsy;
      if(key == -1) {			/* no cursor */
	 break;
      }
      sm_disable();
      msg_1d("Cursor: %c",key);
      msg_1f(" (%10g,",ux);
      msg_1f("%10g)        ",uy);
      if(!sm_image_cursor) {
         msg("        \r");
      } else {
      	 if((val = image_val(ux,uy)) < 1e36) {
            msg_1f(" %10g              \r",val);
         } else {
	    msg(" ***              \r");
         }
      }
      (void)fflush(stdout);
      if(isdigit(key)) msg("\n");
   } while(!sm_interrupt && key != EOF && key != 'e' && key != 'q');

   msg("\n");
   
   sm_relocate(ux,uy);

   return(key);
}

/************************************************************/
/*
 * Define a pair of vectors from the positions of the cursor
 */
void
set_cursor_vectors(name_x,name_y,name_z)
char *name_x,			/* name of x vector */
     *name_y,			/* and name of y vector */
     *name_z;			/* optional name of intensity vector */
{
   REAL *temp_x,		/* temporary space for x vector */
	 *temp_y,		/* and for y vector */
	 *temp_z;		/* and maybe for z vector */
   int dimen,			/* the dimension of the vectors */
       i,
       key,			/* key hit by user */
       px, py,			/* the cursor values in physical coords */
       size;			/* size of temp_[xyz] */
   VECTOR *vec_x, *vec_y = NULL, *vec_z; /* the new vectors */

   size = 50;
   if((temp_x = (REAL *)malloc(size*sizeof(REAL))) == NULL) {
      msg("Can't allocate storage for temp_x\n");
      return;
   }
   if((temp_y = (REAL *)malloc(size*sizeof(REAL))) == NULL) {
      msg("Can't allocate storage for temp_y\n");
      free((char *)temp_x);
      return;
   }
   if(name_z == NULL) {
      temp_z = NULL;
   } else {
      if((temp_z = (REAL *)malloc(size*sizeof(REAL))) == NULL) {
	 msg("Can't allocate storage for temp_z\n");
	 free((char *)temp_x);
	 free((char *)temp_y);
	 return;
      }
   }

   sm_enable();
   
   for(i = 0;;i++) {
      if(i >= size) {
	 size += 50;
	 if((temp_x = (REAL *)realloc((char *)temp_x,size*sizeof(REAL)))
	    							     == NULL) {
	    msg("Can't reallocate storage for temp_x\n");
	    free((char *)temp_y);
	    if(temp_z != NULL) free((char *)temp_z);
	    return;
	 }
	 if((temp_y = (REAL *)realloc((char *)temp_y,size*sizeof(REAL)))
	    							     == NULL) {
	    msg("Can't reallocate storage for temp_y\n");
	    free((char *)temp_x);
	    if(temp_z != NULL) free((char *)temp_z);
	    return;
	 }
	 if(temp_z != NULL) {
	    if((temp_z = (REAL *)realloc((char *)temp_z,size*sizeof(REAL)))
	    							     == NULL) {
	       msg("Can't reallocate storage for temp_z\n");
	       free((char *)temp_x);
	       free((char *)temp_y);
	       return;
	    }
	 }
      }
      do {					/* wait for e, m, p, or q */
	 if(sm_interrupt) {			/* respond to ^C */
	    msg("\n");
	    free((char *)temp_x);
	    free((char *)temp_y);
	    if(temp_z != NULL) free((char *)temp_z);
	    return;
	 }
	 key = CURSOR(0, &px, &py);	/* read the SCREEN position */
	 sm_disable();
	 if(key == -1) {		/* no cursor */
	    break;
	 }
         temp_x[i] = (px - ffx)/fsx;		/* SCREEN --> user coords */
	 temp_y[i] = (py - ffy)/fsy;
	 if(temp_z != NULL) {
	    temp_z[i] = image_val(temp_x[i],temp_y[i]);
	 }
	 if(key == 'e' || key == 'p') {
	    msg_1f("Cursor  x: %10g",temp_x[i]);
	    msg_1f("  y: %10g",temp_y[i]);
	    if(temp_z != NULL) {
	       msg_1f("  I: %10g",temp_z[i]);
	    }
	    msg("                   \r");
	 }
      } while(!sm_interrupt && key != EOF && key != 'e' && key != 'm' && 
	      key != 'p' && key != 'q');
      sm_enable();
      sm_relocate(temp_x[i],temp_y[i]);
      if(key == 'p' || key == 'q') {
	 msg("\n");
	 break;
      }
      sm_dot();
      GFLUSH();
   }
   if(key == 'p') {
      dimen = i + 1;			/* keep the last point */
   } else {
      dimen = i;			/* omit the last point */
   }

   if(dimen == 0) {			/* empty vectors */
      free(temp_x); temp_x = NULL;
      free(temp_y); temp_y = NULL;
      if(temp_z != NULL) {
	 free(temp_z); temp_z = NULL;
      }
   } else {
      if((temp_x = (REAL *)realloc((char *)temp_x,dimen*sizeof(REAL))) == NULL){
	 msg("Can't reallocate storage for temp_x\n");
	 free((char *)temp_y);
	 if(temp_z != NULL) free((char *)temp_z);
	 return;
      }
      if((temp_y = (REAL *)realloc((char *)temp_y,dimen*sizeof(REAL))) == NULL){
	 msg("Can't reallocate storage for temp_y\n");
	 free((char *)temp_x);
	 if(temp_z != NULL) free((char *)temp_z);
	 return;
      }
      if(temp_z != NULL) {
	 if((temp_z = (REAL *)realloc((char *)temp_z,dimen*sizeof(REAL)))
	    == NULL) {
	    msg("Can't reallocate storage for temp_z\n");
	    free((char *)temp_x);
	    free((char *)temp_y);
	    return;
	 }
      }
   }
   
   if((vec_x = make_vector(name_x,1,VEC_FLOAT)) == NULL ||
			(vec_y = make_vector(name_y,1,VEC_FLOAT)) == NULL) {
      if(vec_y == NULL) vec_free(vec_x);
      free((char *)temp_x);
      free((char *)temp_y);
   } else {
      free((char *)(vec_x->vec.f));
      vec_x->vec.f = temp_x;
      vec_x->dimen = dimen;
      free((char *)(vec_y->vec.f));
      vec_y->vec.f = temp_y;
      vec_y->dimen = dimen;
   }

   if(temp_z != NULL) {
      if((vec_z = make_vector(name_z,1,VEC_FLOAT)) == NULL) {
	 vec_free(vec_x);
	 vec_free(vec_y);
	 free((char *)temp_z);
      } else {
	 free((char *)(vec_z->vec.f));
	 vec_z->vec.f = temp_z;
	 vec_z->dimen = dimen;
      }
   }
}
