#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "sm.h"
#include "image.h"
#include "sm_declare.h"

static IMAGE *image = NULL;		/* current image */
static REAL *c_levels = NULL;		/* contour levels */
extern int sm_interrupt,		/* respond to ^C */
	   sm_verbose;			/* control volubility */
static int nlevels = 0;			/* number of levels */

int
read_image(file,which,xmin,xmax,ymin,ymax,x0,x1,y0,y1)
char file[];
int which;
double xmin,xmax,ymin,ymax;
int x0,x1,y0,y1;
{
   char buff[30];			/* temp. buffer */

   x0++; x1++; y0++; y1++;		/* get_image is 1-based */
   freeimage(&image);
   if((image = get_image(file, which, x0, x1, y0, y1)) == NULL) {
      return(-1);
   }
   
   if(xmin != 0.0 || xmax != 0.0) {
      image->xmin = xmin;
      image->xmax = xmax;
   }
   if(ymin != 0.0 || ymax != 0.0) {
      image->ymin = ymin;
      image->ymax = ymax;
   }
   (void)sprintf(buff,"%g",image->xmin); set_image_variable("X0",buff);
   (void)sprintf(buff,"%g",image->xmax); set_image_variable("X1",buff);
   (void)sprintf(buff,"%g",image->ymin); set_image_variable("Y0",buff);
   (void)sprintf(buff,"%g",image->ymax); set_image_variable("Y1",buff);
   
   return(0);
}

/*
 * The values of icase correspond to the following lines:
 * icase == 1  => no line
 * icase == 7  => 2 and 8, or 5 and 3
 *		       v10	         v11
 *			|---5----6----8---|
 *			|  5     6     8  |
 *			| 5      6      8 |
 *			|5       6       8|
 *			4444444444444444434
 *			|        6      3 |
 *			|2       6     3  |
 *			| 2      6    3   |
 *			|__2_____6___3____|
 *		       v00		 v01
 *		      (x,y)
 */
int
sm_contour()
{
   char *ptr;
   float deltx,delty,			/* point spacing in user coords */
	 max_val,			/* maximum allowed float */
         no_val,			/* value signifying missing data */
   	 *vptr0,*vptr1,			/* pointers to rows in image */
	 v00,v10,v01,v11,		/* values in a square */
	 val,				/* value of current level */
	 valmin,valmax,			/* attempt to limit level search */
	 x,y,				/* bottom left of current rectangle */
	 xx0 = 0,xx1 = 0,		/* interpolated position */
         yy0 = 0,yy1 = 0;
   int icase,				/* which case to deal with */
       i,j,				/* counters in i and j */
       imin,imax,jmin,jmax;		/* min and max+1 values of i,j */
   REAL *lptr;				/* pointer to levels */

   if(image == NULL) {
      msg("No image is defined\n");
      return(-1);
   }
   if(c_levels == NULL) {
      msg("No levels are defined\n");
      return(-1);
   }
/*
 * Now figure out what part of the image is to be contoured; this is a little
 * tricky if some of the limits (xmin, xmax, fx1, fx2, etc.) are reversed
 */
   deltx = image->nx <= 1 ? 1 : (image->xmax - image->xmin)/(image->nx - 1);
   if(deltx > 0) {
      if(fx1 < fx2) {
	 imin = (image->xmin > fx1) ? 0 : (fx1 - image->xmin)/deltx + 1e-5;
	 imax = (image->xmax < fx2) ? image->nx - 1 :
     					  (fx2 - image->xmin)/deltx + 1e-5;
      } else {
	 imin = (image->xmin > fx2) ? 0 : (fx2 - image->xmin)/deltx + 1e-5;
	 imax = (image->xmax < fx1) ? image->nx - 1 :
     					  (fx1 - image->xmin)/deltx + 1e-5;
      }
   } else {
      if(fx1 < fx2) {
	 imin = (image->xmax > fx1) ? 0 : (fx1 - image->xmin)/deltx + 1e-5;
	 imax = (image->xmin < fx2) ? image->nx - 1 :
     					  (fx2 - image->xmin)/deltx + 1e-5;
      } else {
	 imin = (image->xmax > fx2) ? 0 : (fx2 - image->xmin)/deltx + 1e-5;
	 imax = (image->xmin < fx1) ? image->nx - 1 :
     					  (fx1 - image->xmin)/deltx + 1e-5;
      }
   }
   if(imin > imax) {
      j = imin; imin = imax; imax = j;
   }
   if(imin < 0) imin = 0;
   if(imax >= image->nx) imax = image->nx - 1;

   delty = image->ny <= 1 ? 1 : (image->ymax - image->ymin)/(image->ny - 1);
   if(delty > 0) {
      if(fy1 < fy2) {
	 jmin = (image->ymin > fy1) ? 0 : (fy1 - image->ymin)/delty + 1e-5;
	 jmax = (image->ymax < fy2) ? image->ny - 1 :
     					  (fy2 - image->ymin)/delty + 1e-5;
      } else {
	 jmin = (image->ymin > fy2) ? 0 : (fy2 - image->ymin)/delty + 1e-5;
	 jmax = (image->ymax < fy1) ? image->ny - 1 :
     					  (fy1 - image->ymin)/delty + 1e-5;
      }
   } else {
      if(fy1 < fy2) {
	 jmin = (image->ymax > fy1) ? 0 : (fy1 - image->ymin)/delty + 1e-5;
	 jmax = (image->ymin < fy2) ? image->ny - 1 :
     					  (fy2 - image->ymin)/delty + 1e-5;
      } else {
	 jmin = (image->ymax > fy2) ? 0 : (fy2 - image->ymin)/delty + 1e-5;
	 jmax = (image->ymin < fy1) ? image->ny - 1 :
     					  (fy1 - image->ymin)/delty + 1e-5;
      }
   }
   if(jmin > jmax) {
      j = jmin; jmin = jmax; jmax = j;
   }
   if(jmin < 0) jmin = 0;
   if(jmax >= image->ny) jmax = image->ny - 1;

   if(sm_verbose > 2) {
      msg_1d("Range of indices in image for contouring: %d",imin);
      msg_1d(" -- %d",imax);
      msg_1d(",  %d",jmin);
      msg_1d(" -- %d\n",jmax);
   }
/*
 * see if any pixels are out of range
 */
   max_val = pow(10.0,(float)(MAX_POW));
   for(i = imin;i < imax;i++) {		/* check row zero */
      if(image->ptr[jmin][i] != image->ptr[jmin][i]) { /* must be a NaN */
	 if(sm_verbose > 1) {
	    msg_1f("Image value of %g ",image->ptr[jmin][i]);
	    msg_1f("at (%g,",image->xmin + i*deltx);
	    msg_1f("%g) is too large\n",image->ymin + jmin*delty);
	 }
	 image->ptr[jmin][i] = max_val;
      }
   }
   for(j = jmin;j < jmax;j++) {		/* and check column zero */
      if(image->ptr[j][imin] != image->ptr[j][imin]) { /* a NaN */
	 if(sm_verbose > 1) {
	    msg_1f("Image value of %g ",image->ptr[j][imin]);
	    msg_1f("at (%g,",image->xmin + imin*deltx);
	    msg_1f("%g) is too large\n",image->ymin + j*delty);
	 }
	 image->ptr[j][imin] = max_val;
      }
   }
   
/*
 * find the `missing data point' value
 */
   if(*(ptr = print_var("missing_data")) != '\0') {
      no_val = atof(ptr);
   } else {
      no_val = max_val;
   }

   for(j = jmin;!sm_interrupt && j < jmax;j++) {
      if(sm_interrupt) break;
      
      y = image->ymin + j*delty;
      vptr0 = &image->ptr[j][imin];
      vptr1 = &image->ptr[j+1][imin];
      v01 = *vptr0++;
      v11 = *vptr1++;
      for(i = imin;i < imax;i++) {
	 x = image->xmin + i*deltx;
	 valmax = valmin = v00 = v01;
         if((v01 = *vptr0++) < valmin) {
	    valmin = v01;
         } else if(v01 > valmax) {
	    valmax = v01;
	 }
	 if((v10 = v11) < valmin) {
	    valmin = v10;
         } else if(v10 > valmax) {
	    valmax = v10;
	 }
	 v11 = *vptr1++;
         if(v11 != v11) {	/* it's NaN */
	    if(sm_verbose > 1) {
	       msg_1f("Image value of %g ",v11);
	       msg_1f("at (%g,",x + deltx);
	       msg_1f("%g) is too large\n",y + delty);
	    }
	    v11 = image->ptr[j + 1][i + 1] = max_val;
	 }
	 if(v00 == no_val || v01 == no_val || v10 == no_val || v11 == no_val) {
	    continue;
	 }

         if(v11 < valmin) {
	    valmin = v11;
         } else if(v11 > valmax) {
	    valmax = v11;
	 }
	 for(lptr = c_levels;
	     lptr < c_levels + nlevels && *lptr < valmin;lptr++) continue;
	 while(lptr < c_levels + nlevels && *lptr <= valmax) {
	    val = *lptr++;
	    icase = 1;
	    if(val > v00) icase++;
	    if(val > v01) icase += 2;
	    if(val > v10) icase += 4;
	    if(val > v11) icase = 9 - icase;
	    switch (icase) {
	     case 1:
	       continue;		/* continue to next level */
	     case 2:
	       xx0 = x + deltx*(val-v00)/(v01-v00);
	       yy0 = y;
	       xx1 = x;
	       yy1 = y + delty*(val-v00)/(v10-v00);
	       break;
	     case 3:
	       xx0 = x + deltx*(val-v00)/(v01-v00);
	       yy0 = y;
	       xx1 = x + deltx;
	       yy1 = y + delty*(val-v01)/(v11-v01);
	       break;
	     case 4:
	       xx0 = x;
	       yy0 = y + delty*(val-v00)/(v10-v00);
	       xx1 = x + deltx;
	       yy1 = y + delty*(val-v01)/(v11-v01);
	       break;
	     case 5:
	       xx0 = x;
	       yy0 = y + delty*(val-v00)/(v10-v00);
	       xx1 = x + deltx*(val-v10)/(v11-v10);
	       yy1 = y + delty;
	       break;
	     case 6:
	       xx0 = x + deltx*(val-v00)/(v01-v00);
	       yy0 = y;
	       xx1 = x + deltx*(val-v10)/(v11-v10);
	       yy1 = y + delty;
	       break;
	     case 7:			/* a saddle */
	       if(v00 > v01) {		/* draw cases 3 and 5 */
		  xx0 = x + deltx*(val-v00)/(v01-v00);
		  yy0 = y;
		  xx1 = x + deltx;
		  yy1 = y + delty*(val-v01)/(v11-v01);
		  sm_line(xx0,yy0,xx1,yy1);
		  xx0 = x + deltx*(val-v10)/(v11-v10);
		  yy0 = y + delty;
		  xx1 = x;
		  yy1 = y + delty*(val-v00)/(v10-v00);
	       } else {			/* draw cases 2 and 8 */
		  xx0 = x + deltx*(val-v00)/(v01-v00);
		  yy0 = y;
		  xx1 = x;
		  yy1 = y + delty*(val-v00)/(v10-v00);
		  sm_line(xx0,yy0,xx1,yy1);
		  xx0 = x + deltx*(val-v10)/(v11-v10);
		  yy0 = y + delty;
		  xx1 = x + deltx;
		  yy1 = y + delty*(val-v01)/(v11-v01);
	       }
	       break;
	     case 8:
	       xx0 = x + deltx*(val-v10)/(v11-v10);
	       yy0 = y + delty;
	       xx1 = x + deltx;
	       yy1 = y + delty*(val-v01)/(v11-v01);
	       break;
	     default:
	       msg_1d("Illegal value of icase %d\n",icase);
	       break;
	    }
	    sm_line(xx0,yy0,xx1,yy1);
	 }
      }
   }
   return(0);
}

void
sm_levels(levs,nlevs)
REAL levs[];
int nlevs;
{
   REAL *levels;
   
   if(c_levels != NULL) {
      free((char *)c_levels);
      c_levels = NULL;
   }

   if((levels = (REAL *)malloc((unsigned)(nlevs)*sizeof(REAL))) == NULL) {
      msg("Can't allocate space for levels\n");
      return;
   }
   memcpy((char *)levels,(char *)levs,nlevs*sizeof(REAL));

   c_levels = levels;
   nlevels = nlevs;
   
   sort_flt(c_levels, nlevels);
}
	
void
sm_minmax(min,max)
float *min,*max;			/* maximum and minimum of image */
{
   float deltx,delty,
         fx_1, fx_2, fy_1, fy_2,	/* fx1 etc. with fx_1 < fx_2 */
   	 *ptr,*end,
         val,
         xmin, xmax, ymin, ymax;
   int j,
       imin,imax,jmin,jmax;		/* min and max+1 values of i,j */
   
   *max = -1e36;
   *min = 1e36;

   if(image == NULL) {
      msg("Image is not defined\n");
      return;
   }

   if(fx1 < fx2) {
      fx_1 = fx1; fx_2 = fx2;
   } else {
      fx_1 = fx2; fx_2 = fx1;
   }
   if(image->xmax > image->xmin) {
      xmin = image->xmin; xmax = image->xmax;
   } else {
      xmin = image->xmax; xmax = image->xmin;
   }
   deltx = (xmax - xmin)/(image->nx - 1);
   imin = (xmin > fx_1) ? 0 : (fx_1 - xmin)/deltx;
   imax = (xmax < fx_2) ? image->nx - 1 : (fx_2 - xmin)/deltx + 1e-5;
   if(imin > imax) {
      j = imin; imin = imax; imax = j;
   }

   if(fy1 < fy2) {
      fy_1 = fy1; fy_2 = fy2;
   } else {
      fy_1 = fy2; fy_2 = fy1;
   }
   if(image->ymax > image->ymin) {
      ymin = image->ymin; ymax = image->ymax;
   } else {
      ymin = image->ymax; ymax = image->ymin;
   }
   delty = (ymax - ymin)/(image->ny - 1);
   jmin = (ymin > fy_1) ? 0 : (fy_1 - ymin)/delty;
   jmax = (ymax < fy_2) ? image->ny - 1 : (fy_2 - ymin)/delty + 1e-5;
   if(jmin > jmax) {
      j = jmin; jmin = jmax; jmax = j;
   }
   
   for(j = jmin;!sm_interrupt && j <= jmax;j++) {
      ptr = &image->ptr[j][imin];
      end = &image->ptr[j][imax];
      while(ptr <= end) {
      	 val = *ptr++;
      	 if(val < *min) {
      	    *min = val;
      	 } else if(val > *max) {
      	    *max = val;
      	 }
      }
   }
   if(*min > *max) *min = *max;		/* failed to find an extremum */
   else if(*max < *min) *max = *min;
}

void
sm_delimage()
{
   freeimage(&image);
   kill_keywords();			/* kill keywords from header */

   if(c_levels != NULL) {
      free((char *)c_levels);
      c_levels = NULL;
   }
}

void
freeimage(imptr)
IMAGE **imptr;
{
   if(*imptr == NULL) return;

   if(strcmp((*imptr)->name,"Data Not Malloced") != 0) { /* free everything */
      if((*imptr)->ptr != NULL) {
	 free((char *)(*imptr)->ptr);
      }
      if((*imptr)->space != NULL) {
	 free((*imptr)->space);
      }
   }
   free((char *)(*imptr));

   *imptr = NULL;
}

/***************************************************************/
/*
 * return a value from the image
 */
float
image_val(x,y)
double x,y;
{
   float dx,dy,				/* grid spacing in x,y */
	 v,				/* value at (x,y) */
	 v1,v2,				/* intermediate values */
   	 **z;				/* z == image->ptr */
   int i,j;

   if(image == NULL) {
      msg("Image is not defined\n");
      return(1.01*NO_VALUE);
   }
   x -= image->xmin;
   y -= image->ymin;

   if(image->nx == 1) {
      dx = 0;
      i = 0;
   } else {
      dx = (image->xmax - image->xmin)/(image->nx - 1);
      i = x/dx;
   }
   if(image->ny == 1) {
      dy = 0;
      j = 0;
   } else {
      dy = (image->ymax - image->ymin)/(image->ny - 1);
      j = y/dy;
   }

   if(i < 0) {
      x = 0.0;
      i = 0;
   } else if(i >= image->nx - 1) {
      i = image->nx - 2;
      x = image->xmax - image->xmin;
   }
   if(j < 0) {
      y = 0.0;
      j = 0;
   } else if(j >= image->ny - 1) {
      j = image->ny - 2;
      y = image->ymax - image->ymin;
   }

   z = image->ptr;
   if(dx == 0.0 || dy == 0.0) {			/* degenerate cases */
      if(dx == 0.0) {
	 if(dy == 0.0) {			/* very degenerate */
	    v = z[0][0];
	 } else {
	    v = z[j][0] + (y/dy - j)*(z[j+1][0] - z[j][0]);
	 }
      } else {
	 v = z[0][i] + (x/dx - i)*(z[0][i+1] - z[0][i]);
      }
   } else {
      v1 = z[j][i]  +  (x/dx - i)*(z[j][i+1]  -  z[j][i]);
      v2 = z[j+1][i] + (x/dx - i)*(z[j+1][i+1] - z[j+1][i]);
      v = v1 + (y/dy - j)*(v2 - v1);
   }
   return(v);
}

/*
 * Return the current image
 */
IMAGE *
current_image()
{
   return(image);
}

/*************************************************************/
/*
 * This function is used by non-interactive SM to 
 * define an array to be contoured. We assume that storage for data
 * and pointers has been allocated elsewhere
 */
int
sm_defimage(data,xmin,xmax,ymin,ymax,nx,ny)
float **data;			/* pointer to pointers to data */
double xmin,xmax,ymin,ymax;	/* size of data array in user coords */
int nx,ny;			/* dimension of data array */
{
   freeimage(&image);

   if((image = (IMAGE *)malloc(sizeof(IMAGE))) == NULL) {
      fprintf(stderr,"Can't allocate storage for image\n");
      return(-1);
   }
   sprintf(image->name,"Data Not Malloced");
   image->nx = nx;
   image->ny = ny;
   image->ptr = data;
   image->space = (char *)data[0];

   image->xmin = xmin; image->xmax = xmax;
   image->ymin = ymin; image->ymax = ymax;
   return(0);
}

void
set_image_name(name)
char *name;
{
   if(image != NULL) {
      sprintf(image->name,"%s",name);
   }
}
