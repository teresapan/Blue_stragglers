/*
 * Floyd-Steinberg dithering.
 */
#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "image.h"
#include "vectors.h"
#include "sm_declare.h"

extern int sm_verbose;			/* be chatty? */

int
sm_dither(x_name, y_name, min, max, fac)
char *x_name, *y_name;			/* names of output vectors */
double min, max;			/* min and max for image */
int fac;				/* sqrt(number of dots per pixel) */
{
   float dx, dy;			/* size of a pixel (user units) */
   int dimen;				/* current size of [xy]vec */
   int i,j;
   IMAGE *image;			/* SM's image */
   int nx, ny;				/* unpacked from image */
   int npoint;				/* size of [xy]vec */
   float max_val = pow(10.0,(float)(MAX_POW));/* maximum allowed float */
   float no_val;			/* indicates missing data */
   float *s_errors, *prev_err, *row_err; /* propagated errors */
   float *row;				/* pointer to a row */
   char *ptr;
   float scale;				/* scale image's values */
   float t;				/* propagate dither errors */
   int x, y;				/* counters in x and y */
   int xfac_j;				/* == x*fac + j */
   float xx, yy;			/* position for a dot */
   float val;				/* the value of a pixel */
   VECTOR *xvec, *yvec;			/* output vectors */
   
   if((image = current_image()) == NULL) {
      msg("No image is defined\n");
      return(-1);
   }
   nx = image->nx; ny = image->ny;

   if(max - min == 0.0) {
      msg_1f("sm_dither: min == max (== %g)\n",min);
      return(-1);
   }
   if(nx <= 1 || ny <= 1) {
      msg_1d("sm_dither: image too small to dither (%dx",nx);
      msg_1d("%d\n",ny);
      return(-1);
   }
/*
 * allocate dither-error arrays
 */
   if((s_errors = (float *)malloc(2*(fac*nx + 1)*sizeof(float))) == NULL) {
      msg("sm_dither: Cannot allocate memory for error arrays\n");
      return(-1);
   }
   row_err = s_errors;
   prev_err = row_err + fac*nx + 1;

   for(i = 0;i < fac*nx + 1;i++) {
      row_err[i] = prev_err[i] = 0.5;
   }
   row_err++; prev_err++;		/* as we want to refer to err[-1] */
/*
 * and the output vectors
 */
   dimen = nx*ny*fac*fac/10;		/* an initial guess */
   if((xvec = make_vector(x_name,dimen,VEC_FLOAT)) == NULL) {
      return(-1);
   }
   if((yvec = make_vector(y_name,dimen,VEC_FLOAT)) == NULL) {
      vec_free(xvec);
      return(-1);
   }
/*
 * find the `missing data point' value
 */
   if(*(ptr = print_var("missing_data")) != '\0') {
      no_val = atof(ptr);
   } else {
      no_val = max_val;
   }
/*
 * setup the scale factors, and start dithering
 */
   scale = 1/(max - min);
   dx = (image->xmax - image->xmin)/(nx - 1);
   dy = (image->ymax - image->ymin)/(ny - 1);
		   
   npoint = 0;
   yy = image->ymin - dy*0.5/(float)fac;
   yy -= 0.5*dy;			/* center on the pixel */

   for(y = 0;y < ny;y++) {		/* process a row */
      row = image->ptr[y];

      for(i = 0;i < fac;i++) {		/* subraster for the row */
	 xx = image->xmin + dx*0.5/(float)fac - dx;
	 xx -= 0.5*dx;			/* center on the pixel */
	 yy += dy/(float)fac;

	 for(xfac_j = x = 0;x < nx;x++) {	/* process each column */
	    xx += dx;
	    if((val = row[x]) != val) {	/* it's NaN */
	       if(sm_verbose > 1) {
		  msg_1f("Image value of %g ",val);
		  msg_1f("at (%g,",image->xmin + x*dx);
		  msg_1f("%g) is too large\n",image->ymin + y*dy);
	       }
	       val = row[x] = max_val;
	    }

	    if(val == no_val) {
	       continue;
	    }
	       
	    val = (val <= min) ? 0 : (val >= max) ? 1 : scale*(val - min);
	    
	    for(j = 0;j < fac;j++, xfac_j++) { /* subdivide the pixel */
	       t = val + (7*row_err[xfac_j - 1] + 1*prev_err[xfac_j - 1] +
			  5*prev_err[xfac_j]    + 3*prev_err[xfac_j + 1])/16;
		         
	       if (t > 0.5) {
		  t -= 1.0;
		  if(npoint >= dimen) {
		     dimen *= 2;
		     if(vec_realloc(xvec,dimen) != 0) {
			msg_1s("Cannot realloc %s\n",xvec->name);
			vec_free(yvec);
			return(-1);
		     }
		     if(vec_realloc(yvec,dimen) != 0) {
			msg_1s("Cannot realloc %s\n",yvec->name);
			vec_free(xvec);
			return(-1);
		     }
		  }
		  xvec->vec.f[npoint] = xx + j*dx/(float)fac;
		  yvec->vec.f[npoint] = yy;
		  npoint++;
	       }
	       row_err[xfac_j] = t;
	    }
	 }
	 {
	    float *tmp = row_err;
	    row_err = prev_err;
	    prev_err = tmp;
	 }
      }
   }

   free(s_errors);
/*
 * reclaim unused space in vectors
 */
   if(vec_realloc(xvec,npoint) != 0) {
      msg_1s("Cannot realloc %s\n",xvec->name);
      vec_free(yvec);
      return(-1);
   }

   if(vec_realloc(yvec,npoint) != 0) {
      msg_1s("Cannot realloc %s\n",yvec->name);
      vec_free(xvec);
      return(-1);
   }

   return(0);
}
