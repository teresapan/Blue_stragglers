#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "sm_declare.h"
/*
 * Shade the area inside a curve. If type==2, shade_hist first convert the
 * curve into a histogram, and then shade between that histogram and the
 * x axis.
 */
#include <stdio.h>
#include <math.h>
#include "sm.h"
#include "defs.h"
#include "devices.h"

#define NCROSS 500			/* maximum crossings per shade line */
  
extern int sm_interrupt,			/* respond to ^C */
  	   sm_verbose;			/* Be chatty? */
static float cosa,sina;			/* cos and sin of desired rotation */
static void draw_seg(),
            rotate(),
            shade_area();
static int scale_polygon(),
           shade_hist();

void
sm_shade(fdelta,a,b,npoint,type)
double fdelta;				/* spacing between lines */
REAL *a,*b;				/* (a,b) is curve defining region */
int npoint;				/* dimension of a,b */
int type;				/* shade an area (type == 1) or histogram (type == 2) */
{
   if(type == 1) {
      shade_area(fdelta, a, b, npoint);
   } else if(type == -2 || type == 2) {
      (void)shade_hist(fdelta , a, b, (type > 0 ? npoint : -npoint));
   } else {
      msg_1d("Unknown shading command: %d\n", type);
   }
}

static void
shade_area(fdelta,a,b,npoint)
double fdelta;				/* spacing between lines */
REAL *a,*b;				/* (a,b) is curve defining region */
int npoint;				/* dimension of a,b */
{
   float alph,				/* angle to rotate by */
   	 dy,				/* line spacing in y units */
   	 yval;				/* current (rotated) y value */
   int i,i1,
       j;				/* index into crossing[] */
   REAL crossing[NCROSS],		/* where line segments cross curve */
   	cx[4],cy[4];			/* corners of box */
   int delta = (int)fdelta;		/* spacing between lines */
   float shade_offset = fdelta - delta;	/* offset of lines from canonical position in units of delta */
/*
 * See if we are requested to use some fancy shading style
 */
   if(delta <= 0) {			/* a shade style */
      if(FILL_POLYGON(abs(delta),(short *)NULL,(short *)NULL,0) < 0) {
	 ;				/* no device support; fake fill
					   with closely spaced lines */
      } else {
	 if(delta == FILL_SOLID) {
	    short *x,*y;
	    int n;

	    if((n = scale_polygon(a,b,npoint,&x,&y)) < 0) {
	       msg("sm_shade: can't scale polygon\n");
	       return;
	    }
	    if(FILL_POLYGON(abs(delta),x,y,n) == 0) { /* OK */
	       free((char *)x);
	       return;
	    }
	 } else {
	    msg_1d("Unknown shade style: %d\n",abs(delta));
	    delta = 0;
	 }
      }
   }
/*
 * We just want to use lines to shade the area
 */
   if(delta < ldef) {
      delta = ldef;			/* no point drawing them closer */
   }
/*
 * Figure out angle that'll give aangle when drawn
 */
   if(sm_cos == 0.0) {
      cosa = 0;
      sina = sm_sin/fabs(sm_sin);
   } else if(sm_sin == 0.0) {
      cosa = sm_cos/fabs(sm_cos);
      sina = 0;
   } else {
      alph = atan((sm_sin/fsy)/(aspect*sm_cos/fsx));
      sina = fabs(sin(alph)/sm_sin)*sm_sin;
      cosa = fabs(cos(alph)/sm_cos)*sm_cos;
   }
   dy = delta*hypot(sina/fsx,cosa/fsy);
   if(dy == 0) dy = 1;
   
   cx[0] = fx1; cy[0] = fy1;
   cx[1] = fx2; cy[1] = fy1;
   cx[2] = fx1; cy[2] = fy2;
   cx[3] = fx2; cy[3] = fy2;
   rotate(a,b,npoint);
   rotate(cx,cy,4);

   sort_flt(cx,4);			/* find highest and lowest corners */
   sort_flt(cy,4);
/*
 * Now we can draw lines
 */
   for(yval = cy[0] + dy*shade_offset; !sm_interrupt && yval < cy[3];yval += dy) {
      for(i = j = 0;i < npoint;i++) {
	 i1 = (i == npoint - 1) ? 0 : i + 1; /* wrap start onto end */
	 if(i1 != 0 && (b[i1] - yval) == 0.0) {
	    continue;			/* don't find it twice */
	 }
	 if((b[i] - yval)*(b[i1] - yval) <= 0) { /* a crossing */
	    if(yval - b[i] == 0.0) {
	       crossing[j++] = a[i];	/* avoid division by zero */
	    } else {
	       crossing[j++] = a[i] +
		 (a[i1] - a[i])/(b[i1] - b[i])*(yval - b[i]);
	    }
	    if(j >= NCROSS) {
	       if(sm_verbose) {
		  msg_1d("Only %d boundary crossings per line in shade\n",
			 					NCROSS);
	       }
	       break;
	    }
	 }
      }
      if(j > 0) {			/* lines to draw */
	 sort_flt(crossing,j);
	 for(i = 0;i < j - 1;i += 2) {
	    draw_seg(crossing[i],crossing[i+1],yval);
	 }
	 if(j%2 == 1) {			/* an incomplete line */
	    draw_seg(crossing[j - 1],cx[3],yval);
	 }
      }
   }
}

/*****************************************************************/
/*
 * Shade a curve as a histogram
 */
static int
shade_hist(fdelta,a,b,npoint)
double fdelta;				/* spacing between lines */
REAL *a,*b;				/* (a,b) is curve defining region */
int npoint;				/* dimension of a,b */
{
   REAL *x,*y;
   int i;
   int rotated = (npoint < 0) || (fabs(aangle - 90) < 1e-5);
   if(npoint < 0) npoint = -npoint;
/*
 * Need space for histogram `corners'; notice that we need 2N + 2 not 2N
 */
   if((x = (REAL *)malloc(2*(npoint + 1)*sizeof(REAL))) == NULL) {
      msg("Can't allocate storage for x\n");
      return(-1);
   }
   if((y = (REAL *)malloc(2*(npoint + 1)*sizeof(REAL))) == NULL) {
      msg("Can't allocate storage for y\n");
      free((char *)x);
      return(-1);
   }
/*
 * Convert curve to histogram, and anchor ends on x axis
 */
   if(rotated) {
       REAL yav;
       
       y[0] = y[1] = 0.5*(3*b[0] - b[1]);
       x[0] = 0;
       x[1] = a[0];
       for(i = 1;i < npoint;i++) {
	   yav = 0.5*(b[i - 1] + b[i]);
	   y[2*i] = y[2*i + 1] = yav;
	   x[2*i] = a[i - 1];
	   x[2*i + 1] = a[i];
       }
       yav = 0.5*(3*b[i - 1] - b[i - 2]);
       y[2*i] = y[2*i + 1] = yav;
       x[2*i] = a[i - 1];
       x[2*i + 1] = 0;		/* Anchor last x point on axis */
   } else {
       REAL xav;
       
       x[0] = x[1] = 0.5*(3*a[0] - a[1]);
       y[0] = 0;
       y[1] = b[0];
       for(i = 1;i < npoint;i++) {
	   xav = 0.5*(a[i - 1] + a[i]);
	   x[2*i] = x[2*i + 1] = xav;
	   y[2*i] = b[i - 1];
	   y[2*i + 1] = b[i];
       }
       xav = 0.5*(3*a[i - 1] - a[i - 2]);
       x[2*i] = x[2*i + 1] = xav;
       y[2*i] = b[i - 1];
       y[2*i + 1] = 0;		/* anchor last y point on axis */
   }

   (void)shade_area(fdelta,x,y,2*(npoint + 1)); /* do the actual shading */
   
   free((char *)x); free((char *)y);
   return(0);
}

/*****************************************************************************/
/*
 * Convert the boundary of a polygon to be filled to SM coordinates, and
 * clip at the edge of the current box
 */
static int
scale_polygon(a,b,npoint,xx,yy)
REAL *a,*b;
int npoint;
short **xx,**yy;
{
   int crosses;				/* does a segment cross the box? */
   int i,j;
   int n;				/* number of points needed */
   short *x,*y;				/* equivalent to *xx, *yy */
   long x1,y1,x2,y2;			/* ends of a segment */
   long x3,y3,x4,y4;			/* intersection of seg with box */
/*
 * see how many extra points we might need; such points are generated when
 * a line in the polygon crosses our bounding box
 */
   for(n = npoint,i = 0;i < npoint;i++) {
      if(a[i] < fx1 || a[i] > fx2 || b[i] < fy1 || b[i] > fy2) n += 2;
   }
   n++;					/* we'll close the polygon */

   if((x = (short *)malloc(2*n*sizeof(short))) == NULL) {
      msg("sm_shade: can't allocate storage for x\n");
      return(-1);
   }
   y = x + n;
   *xx = x; *yy = y;
   
   x2 = ffx + fsx*a[0];
   y2 = ffy + fsy*b[0];
   for(i = 1, j = 0;i <= npoint;i++) {
      x1 = x2; y1 = y2;
      x2 = ffx + fsx*a[i%npoint];
      y2 = ffy + fsy*b[i%npoint];

      if((crosses = cross(x1,y1,x2,y2,gx1,gy1,gx2,gy2,&x3,&y3,&x4,&y4)) == 0) {
	   x[j] = x3;			/* segment is within box */
	   y[j++] = y3;
      } else if(crosses == 1) {
	   x[j] = x3;			/* segment crosses box */
	   y[j++] = y3;
	   x[j] = x4;
	   y[j++] = y4;
      } else {			/* see if it crosses one of the sides of the
				   box extrapolated to infinity (e.g. x==gx1)*/
	 if(x3 == gx1 || x4 == gx1) {	/* left side */
	    x[j] = gx1;
	    y[j++] = ((x3 == gx1 ? y3 : y4) > gy2) ? gy2 : gy1;
	 } else if(x3 == gx2 || x4 == gx2) { /* right side */
	    x[j] = gx2;
	    y[j++] = ((x3 == gx2 ? y3 : y4) > gy2) ? gy2 : gy1;
	 } else if(y3 == gy1 || y4 == gy1) { /* bottom side */
	    y[j] = gy1;
	    x[j++] = ((y3 == gy1 ? x3 : x4) > gx2) ? gx2 : gx1;
	 } else if(y3 == gy2 || y4 == gy2) { /* top side */
	    y[j] = gy2;
	    x[j++] = ((y3 == gy2 ? x3 : x4) > gx2) ? gx2 : gx1;
	 }
      }
      
   }
/*
 * I'm pretty sure that this can't happen...
 */
   if(j > n) {
      msg("scale_polygon: array wasn't big enough. Please tell RHL nicely\n");
   }

   return(j);
}

/*****************************************************************/
/*
 * draw a line between (x0,y) and (x1,y) (rotated coordinates)
 */
static void
draw_seg(x0,x1,y)
double x0,x1,y;
{
   float yy0,yy1;			/* y0 and y1 are Bessel functions
					   that lint worries about */

   yy0 = y*cosa + x0*sina;		/* rotate back */
   x0 = x0*cosa - y*sina;
   yy1 = y*cosa + x1*sina;
   x1 = x1*cosa - y*sina;

   sm_line(x0,yy0,x1,yy1);
}

/*****************************************************************/
/*
 * Rotate a pair of vectors through -asin(sina) == -alph
 */
static void
rotate(a,b,n)
REAL *a,*b;				/* arrays to rotate */
int n;					/* size of a,b */
{
   float x;
   
   while(--n >= 0) {
      x = a[n]*cosa + b[n]*sina;	/* rotate by -alph */
      b[n] = -a[n]*sina + b[n]*cosa;
      a[n] = x;
   }
}
