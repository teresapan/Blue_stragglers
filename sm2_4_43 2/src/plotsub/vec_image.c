#include "copyright.h"
/*
 * This file contains a number of functions to do image operations on vectors.
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "image.h"
#include "vectors.h"
#include "sm_declare.h"

extern int sm_interrupt,		/* Have we seen a ^C? */
  	   sm_verbose;			/* shall we be chatty? */


/*****************************************************************************/
/*
 * Set the values of the IMAGE from the vector vals; xi and yi are indexes
 */
void
vec_set_image_from_index(xi,yi,vals)
VECTOR *xi,*yi,				/* vectors giving indices */
       *vals;				/* the values */
{
   int exchange = 0;			/* have vectors been exchanged ? */
   int i,j;
   IMAGE *image;
   int nx, ny;				/* unpacked from IMAGE */
   int nval;				/* number of values to set */
   float **ptr;				/* unpacked from IMAGE */
   VECTOR row, col;			/* indices for a complete row or
					   column of the IMAGE */
   float *rowptr;			/* == image->row[r] */
   VECTOR *temp;			/* used in switching x1 and yi */
   REAL val;				/* value of vals is scalar */
   int vals_is_scalar = 0;		/* is vals a scalar? */
   int xind, yind;			/* indices in x and y */
   char *xname, *yname;			/* names of vectors */
   
   if((image = current_image()) == NULL) {
      msg("Image is not defined\n");
      return;
   }
   nx = image->nx; ny = image->ny; ptr = image->ptr;
/*
 * check that all vectors are all numerical
 */
   if((xi != NULL && xi->type == VEC_STRING) ||
      (yi != NULL && yi->type == VEC_STRING) || vals->type == VEC_STRING) {
      if(xi != NULL && xi->type == VEC_STRING) {
	 msg_1s("Vector %s is not arithmetic",xi->name);
	 yyerror("show where");
      }
      if(yi != NULL && yi->type == VEC_STRING) {
	 msg_1s("Vector %s is not arithmetic",yi->name);
	 yyerror("show where");
      }
      if(vals->type == VEC_STRING) {
	 msg_1s("Vector %s is not arithmetic",vals->name);
	 yyerror("show where");
      }
      vec_free(xi);
      vec_free(yi);
      vec_free(vals);
      return;
   }

   vec_convert_long(xi);
   vec_convert_long(yi);
   vec_convert_float(vals);
/*
 * The cases xi == NULL and yi == NULL are used to mean "all valid values"
 */
   if(xi == NULL && yi == NULL) {	/* whole image */
      if(vals->dimen == 1) {
	 val = vals->vec.f[0];
	 for(j = 0;j < ny;j++) {
	    rowptr = ptr[j];
	    for(i = 0;i < nx;i++) {
	       rowptr[i] = val;
	    }
	 }
      } else if(vals->dimen != nx*ny) {
	 msg_2s("Vector %s has too %s values for image; ",vals->name,
		(vals->dimen < nx*ny ? "few" : "many"));
	 msg_1d("(%d not ",vals->dimen);
	 msg_1d("%d)\n",nx*ny);
      } else {
	 REAL *valptr = vals->vec.f;
	 for(j = 0;j < ny;j++, valptr += nx) {
	    rowptr = ptr[j];
	    if(sizeof(REAL) == sizeof(float)) {
	       memcpy(rowptr,valptr,nx*sizeof(float));
	    } else {
	       for(i = 0;i < nx;i++) {
		  rowptr[i] = valptr[i];
	       }
	    }
	 }
      }

      vec_free(vals);
      return;
   }
/*
 * Not the whole thing; that's harder
 */
   if(xi == NULL) {
      if(make_anon_vector(&row, image->nx, VEC_LONG) < 0) {
	 return;
      }
      row.name = "(row)";
      for(i = 0;i < nx;i++) {
	 row.vec.l[i] = i;
      }
      xi = &row;
   }
   if(yi == NULL) {
      if(make_anon_vector(&col, image->ny, VEC_LONG) < 0) {
	 return;
      }
      col.name = "(column)";
      for(i = 0;i < ny;i++) {
	 col.vec.l[i] = i;
      }
      yi = &col;
   }
/*
 * OK, time to work
 */
   xname = xi->name; yname = yi->name;	/* before we consider swapping them */

   if(xi->dimen < yi->dimen) {	/* make xi >= yi */
      temp = xi;
      xi = yi;
      yi = temp;
      exchange = 1;
   }

   if(yi->dimen == 0) {			/* swap them back, to free longer */
      temp = xi;
      xi = yi;
      yi = temp;
   } else if(yi->dimen == 1) {		/* scalar and vector */
      if(exchange) {
	 xind = yi->vec.l[0];
	 if(xind < 0 || xind >= nx) {
	    msg_1d("x-index %d is out of range ",xind);
	    msg_1d("0--%d\n",nx - 1);
	 } else {
	    if(vals->dimen == xi->dimen) {
	       nval = vals->dimen;
	    } else if(vals->dimen == 1) {
	       vals_is_scalar = 1;
	       nval = xi->dimen;
	    } else {
	       msg_2s("image[%s,%s]:",xname,yname);
	       msg_1s(" index vectors' dimensions differ from value's (%s)",
		     vals->name);
	       yyerror("show where");
	       nval = (vals->dimen < xi->dimen) ? vals->dimen : xi->dimen;
	    }

	    if(vals_is_scalar) {
	       val = vals->vec.f[0];
	       for(i = 0;i < nval;i++) {
		  yind = xi->vec.l[i];
		  if(yind < 0 || yind >= ny) {
		     msg_1d("y-index %d is out of range ",yind);
		     msg_1d("0--%d\n",ny - 1);
		     if(sm_interrupt) break;
		  } else {
		     ptr[yind][xind] = val;
		  }
	       }
	    } else {
	       for(i = 0;i < nval;i++) {
		  yind = xi->vec.l[i];
		  if(yind < 0 || yind >= ny) {
		     msg_1d("y-index %d is out of range ",yind);
		     msg_1d("0--%d\n",ny - 1);
		     if(sm_interrupt) break;
		  } else {
		     ptr[yind][xind] = vals->vec.f[i];
		  }
	       }
	    }
	 }
      } else {
	 yind = yi->vec.l[0];
	 if(yind < 0 || yind >= ny) {
	    msg_1d("y-index %d is out of range ",yind);
	    msg_1d("0--%d\n",ny - 1);
	 } else {
	    rowptr = ptr[yind];

	    if(vals->dimen == xi->dimen) {
	       nval = vals->dimen;
	    } else if(vals->dimen == 1) {
	       vals_is_scalar = 1;
	       nval = xi->dimen;
	    } else {
	       msg_2s("image[%s,%s]:",xname,yname);
	       msg_1s(" index vectors' dimensions differ from value's (%s)",
		     vals->name);
	       yyerror("show where");
	       nval = (vals->dimen < xi->dimen) ? vals->dimen : xi->dimen;
	    }

	    if(vals_is_scalar) {
	       val = vals->vec.f[0];
	       for(i = 0;i < nval;i++) {
		  xind = xi->vec.l[i];
		  if(xind < 0 || xind >= nx) {
		     msg_1d("x-index %d is out of range ",xind);
		     msg_1d("0--%d\n",nx - 1);
		     if(sm_interrupt) break;
		  } else {
		     rowptr[xind] = val;
		  }
	       }
	    } else {
	       if(xi == &row) {		/* copy entire row */
		  if(sizeof(REAL) == sizeof(float)) {
		     memcpy(rowptr,vals->vec.f,nval*sizeof(float));
		  } else {
		     for(i = 0;i < nval;i++) {
			rowptr[i] = vals->vec.f[i];
		     }
		  }
	       } else {
		  for(i = 0;i < nval;i++) {
		     xind = xi->vec.l[i];
		     if(xind < 0 || xind >= nx) {
			msg_1d("x-index %d is out of range ",xind);
			msg_1d("0--%d\n",nx - 1);
			if(sm_interrupt) break;
		     } else {
			rowptr[xind] = vals->vec.f[i];
		     }
		  }
	       }
	    }
	 }
      }
   } else {			/* both vectors */
      if(xi->dimen != yi->dimen) {
	 msg_2s("image[%s,%s]:",xname,yname);
	 msg(" index vectors' dimensions differ");
	 yyerror("show where");
      }
      if(vals->dimen == yi->dimen) {
	 nval = vals->dimen;
      } else if(vals->dimen == 1) {
	 vals_is_scalar = 1;
	 nval = yi->dimen;
	 val = vals->vec.f[0];
      } else {
	 msg_2s("image[%s,%s]:",xname,yname);
	 msg_1s(" index vectors' dimensions differ from value's (%s)",
								   vals->name);
	 yyerror("show where");
	 nval = (vals->dimen < yi->dimen) ? vals->dimen : yi->dimen;
      }
      
      for(i = 0;i < nval;i++) {
	 if(exchange) {
	    yind = xi->vec.l[i];
	    xind = yi->vec.l[i];
	 } else {
	    xind = xi->vec.l[i];
	    yind = yi->vec.l[i];
	 }

	 if(yind < 0 || yind >= ny) {
	    msg_1d("y-index %d is out of range ",yind);
	    msg_1d("0--%d\n",ny - 1);
	    if(sm_interrupt) break;
	 } else if(xind < 0 || xind >= nx) {
	    msg_1d("x-index %d is out of range ",xind);
	    msg_1d("0--%d\n",nx - 1);
	    if(sm_interrupt) break;
	 } else {
	    ptr[yind][xind] = vals_is_scalar ? val : vals->vec.f[i];
	 }
      }
   }

   vec_free(vals);
   vec_free(xi);
   vec_free(yi);
   return;
}

/*****************************************************************************/
/*
 * Set the vector vals from the IMAGE; xi and yi are indexes
 */
void
vec_get_image_from_index(xi,yi,vals)
VECTOR *xi,*yi,*vals;
{
   int exchange = 0;			/* have vectors been exchanged ? */
   int i,j;
   IMAGE *image;
   float max_val;			/* maximum allowed float */
   int nx, ny;				/* unpacked from IMAGE */
   float **ptr;				/* unpacked from IMAGE */
   VECTOR row, col;			/* indices for a complete row or
					   column of the IMAGE */
   float *rowptr;			/* == image->row[r] */
   VECTOR *temp;			/* used in switching x1 and yi */
   int xind, yind;			/* indices in x and y */
/*
 * check that vectors are both floating
 */
   if((xi != NULL && xi->type == VEC_STRING) ||
      (yi != NULL && yi->type == VEC_STRING) || vals->type == VEC_STRING) {
      if(xi != NULL && xi->type == VEC_STRING) {
	 msg_1s("Vector %s is not arithmetic",xi->name);
	 yyerror("show where");
      }
      if(yi != NULL && yi->type == VEC_STRING) {
	 msg_1s("Vector %s is not arithmetic",yi->name);
	 yyerror("show where");
      }
      if(vals->type == VEC_STRING) {
	 msg_1s("Vector %s is not arithmetic",vals->name);
	 yyerror("show where");
      }
      vec_free(xi);
      vec_free(yi);
      vec_free(vals);
      return;
   }

   vec_convert_float(xi);
   vec_convert_float(yi);
   vec_convert_float(vals);

   max_val = pow(10.0,(float)(MAX_POW));
   
   if((image = current_image()) == NULL) {
      msg("Image is not defined\n");
      vec_value(vals,0.0);
      return;
   }
   nx = image->nx; ny = image->ny; ptr = image->ptr;
/*
 * The cases xi == NULL and yi == NULL are used to mean "all valid values"
 */
   if(xi == NULL && yi == NULL) {	/* whole image */
      vals->type = VEC_FLOAT;
      if(vec_malloc(vals,nx*ny) < 0) {
	 msg("Cannot allocate space to set vector from IMAGE\n");
	 vec_value(vals,0.0);
	 return;
      } else {
	 REAL *valptr = vals->vec.f;
	 for(j = 0;j < ny;j++, valptr += nx) {
	    rowptr = ptr[j];
	    if(sizeof(REAL) == sizeof(float)) {
	       memcpy(valptr,rowptr,nx*sizeof(float));
	    } else {
	       for(i = 0;i < nx;i++) {
		  valptr[i] = rowptr[i];
	       }
	    }
	 }
      }
      return;
   }
/*
 * not the whole thing; that's harder work
 */
   if(xi == NULL) {
      if(make_anon_vector(&row, image->nx, VEC_FLOAT) < 0) {
	 return;
      }
      for(i = 0;i < nx;i++) {
	 row.vec.f[i] = i;
      }
      xi = &row;
   }
   if(yi == NULL) {
      if(make_anon_vector(&col, image->ny, VEC_FLOAT) < 0) {
	 return;
      }
      for(i = 0;i < ny;i++) {
	 col.vec.f[i] = i;
      }
      yi = &col;
   }
/*
 * OK, time to work
 */
   if(xi->dimen < yi->dimen) {		/* make xi >= yi */
      temp = xi;
      xi = yi;
      yi = temp;
      exchange = 1;
   }
      
   if(yi->dimen == 0) {			/* swap them back, to free longer */
      temp = xi;
      xi = yi;
      yi = temp;
   } else if(yi->dimen == 1) {		/* scalar and vector */
      if(exchange) {
	 xind = yi->vec.f[0];
	 if(xind < 0 || xind >= nx) {
	    msg_1d("x-index %d is out of range ",xind);
	    msg_1d("0--%d\n",nx - 1);

	    for(i = 0;i < xi->dimen;i++) {
	       xi->vec.f[i] = max_val;
	    }
	 } else {
	    for(i = 0;i < xi->dimen;i++) {
	       yind = xi->vec.f[i];
	       if(yind < 0 || yind >= ny) {
		  msg_1d("y-index %d is out of range ",yind);
		  msg_1d("0--%d\n",ny - 1);
		  if(sm_interrupt) break;
		  xi->vec.f[i] = max_val;
	       } else {
		  xi->vec.f[i] = ptr[yind][xind];
	       }
	    }
	 }
      } else {
	 yind = yi->vec.f[0];
	 if(yind < 0 || yind >= ny) {
	    msg_1d("y-index %d is out of range ",yind);
	    msg_1d("0--%d\n",ny - 1);

	    for(i = 0;i < xi->dimen;i++) {
	       xi->vec.f[i] = max_val;
	    }
	 } else {
	    rowptr = ptr[yind];

	    if(xi == &row) {		/* copy entire row */
	       if(sizeof(REAL) == sizeof(float)) {
		  memcpy(xi->vec.f,rowptr,nx*sizeof(float));
	       } else {
		  for(i = 0;i < nx;i++) {
		     xi->vec.f[i] = rowptr[i];
		  }
	       }
	    } else {
	       for(i = 0;i < xi->dimen;i++) {
		  xind = xi->vec.f[i];
		  if(xind < 0 || xind >= nx) {
		     msg_1d("x-index %d is out of range ",xind);
		     msg_1d("0--%d\n",nx - 1);
		     if(sm_interrupt) break;
		     xi->vec.f[i] = max_val;
		  } else {
		     xi->vec.f[i] = rowptr[xind];
		  }
	       }
	    }
	 }
      }
   } else {			/* both vectors */
      if(xi->dimen != yi->dimen) {
	 if(exchange) {
	    msg_2s("image[%s,%s]:",yi->name,xi->name);
	 } else {
	    msg_2s("image[%s,%s]:",xi->name,yi->name);
	 }
	 msg(" vectors' dimensions differ");
	 yyerror("show where");
      }
      for(i = 0;i < yi->dimen;i++) {
	 if(exchange) {
	    yind = xi->vec.f[i];
	    xind = yi->vec.f[i];
	 } else {
	    xind = xi->vec.f[i];
	    yind = yi->vec.f[i];
	 }

	 if(yind < 0 || yind >= ny) {
	    msg_1d("y-index %d is out of range ",yind);
	    msg_1d("0--%d\n",ny - 1);
	    if(sm_interrupt) break;
	    xi->vec.f[i] = max_val;
	 } else if(xind < 0 || xind >= nx) {
	    msg_1d("x-index %d is out of range ",xind);
	    msg_1d("0--%d\n",nx - 1);
	    if(sm_interrupt) break;
	    xi->vec.f[i] = max_val;
	 } else {
	    xi->vec.f[i] = ptr[yind][xind];
	 }
      }
   }
   vec_free(yi);
   vals->dimen = xi->dimen;
   vals->type = VEC_FLOAT;
   vals->name = xi->name;
   vals->vec.f = xi->vec.f;
   return;
}
