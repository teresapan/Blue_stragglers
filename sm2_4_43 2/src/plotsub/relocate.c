#include "copyright.h"
#include <stdio.h>
#include <math.h>
#include "options.h"
#include "sm.h"
#include "defs.h"
#include "devices.h"
#include "sm_declare.h"

int offscreen = 1;			/* is the current point offscreen? */
static void set_sep_off();
extern int sm_verbose;			/* be chatty? */
static int windows = 0;			/* we're using the window command */

/*
 * Relocate, first transforming to SCREEN coords
 */
void
sm_relocate(x,y)
double x,y;				/* position in user coordinates */
{
   xp = ffx + fsx * x;
   yp = ffy + fsy * y;

   if(IN_RANGE(x,fx1,fx2) && IN_RANGE(y,fy1,fy2)) {
      RELOC((int)xp,(int)yp);
   } else {
      offscreen = 1;
   }
}

/*
 * Relocate, coords already in SCREEN
 */
void
sc_relocate(x,y)
long x,y;
{
   if(x == xp && y == yp) return;	/* we're already there */
   
   xp = x; yp = y;
   if(IN_RANGE(xp,gx1,gx2) && IN_RANGE(yp,gy1,gy2)) {
      RELOC((int)xp,(int)yp);
   } else {
      offscreen = 1;
   }
}

/*
 * Relocate in SCREEN coords, no questions asked
 */
void
sm_grelocate(x,y)
double x, y;
{
   xp = x;
   yp = y;
   RELOC((int)xp,(int)yp);
}

/*
 * Draw, converting from user to SCREEN, and clipping
 */
void
sm_draw(x,y)
double x,y;
{
   double dxq = ffx + fsx * x;
   double dyq = ffy + fsy * y;
   long	xc, yc, xd, yd;
   long	xq = dxq, yq = dyq;
/*
 * Worry that converting e.g. 902315536159.6 to long can result in
 * -2147483648; e.g. gcc 2.95 under Linux
 */
   if(xq < 0 && dxq > 0) {
      xq = -(xq + 1);			/* out of range of long*/
   }
   if(yq < 0 && dyq > 0) {
      yq = -(yq + 1);			/* out of range of long */
   }

   if(cross(xp,yp,xq,yq,gx1,gy1,gx2,gy2,&xc,&yc,&xd,&yd) >= 0) {
      draw_dline( xc, yc, xd, yd );
   }
   xp = xq;
   yp = yq;
}

/*
 * Draw, coords already SCREEN, with clipping
 */
void
sc_draw(xq,yq)
long xq,yq;
{
   long xc,yc,xd,yd;

   if(cross(xp,yp,xq,yq,gx1,gy1,gx2,gy2,&xc,&yc,&xd,&yd) >= 0) {
      draw_dline(xc,yc,xd,yd);
   }
   xp = xq;
   yp = yq;
}

/*
 * Draw in SCREEN coords, no limits imposed
 */
void
gdraw(xq,yq)
double xq,yq;
{
   long xc,yc,xd,yd;

   if(cross(xp,yp,(int)xq,(int)yq,
	       0,0,SCREEN_SIZE,SCREEN_SIZE,&xc,&yc,&xd,&yd) >= 0) {
      draw_dline(xc,yc,xd,yd);
   }
   xp = xq;
   yp = yq;
}

/*
 * Return segment XC,YC,XD,YD, which is the segment X1,Y1,X2,Y2 clipped to
 * lie inside the box X3,Y3,X4,Y4, if the initial segment lies at least
 * partially within the box; in this case return 1 if the box crosses a
 * boundary, 0 if it lies fully within the box. Return -1 if never in the box.
 *
 * It is assumed that X3 < X4 and Y3 < Y4.
 * (y1 is a function in math.h, so call y1 yy1)
 */
int
cross( x1, yy1, x2, y2, x3, y3, x4, y4, xc, yc, xd, yd )
long x1, yy1, x2, y2, x3, y3, x4, y4;
long *xc, *yc, *xd, *yd;
{
    float x, y, xcc, ycc, xdd, ydd;
    int crosses = 0;			/* does the segment cross the box? */

    xcc = x1;
    ycc = yy1;
    xdd = x2;
    ydd = y2;

/* Cross left bdy?  */
    if ( (xcc-x3)*(xdd-x3) < 0 ) {
         crosses = 1;
         y = ycc + (ydd - ycc) * (x3 - xcc)/(xdd - xcc);
         if (xcc > x3) {
             xdd = x3;
             ydd = y;
         } else {
             xcc = x3;
             ycc = y;
         }
    }

/* Cross right bdy?  */
    if ( (xcc-x4)*(xdd-x4) < 0 ) {
         crosses = 1;
         y = ycc + (ydd - ycc) * (x4 - xcc)/(xdd - xcc);
         if (x4 > xcc) {
             xdd = x4;
             ydd = y;
         } else {
             xcc = x4;
             ycc = y;
         }
    }

/* Cross bottom bdy?  */
    if ( (ycc-y3)*(ydd-y3) < 0 ) {
         crosses = 1;
         x = xcc + (xdd - xcc) * (y3 - ycc)/(ydd - ycc);
         if (ycc > y3) {
             xdd = x;
             ydd = y3;
         } else {
             xcc = x;
             ycc = y3;
         }
    }

/* Cross top bdy?  */
    if ( (ycc-y4)*(ydd-y4) < 0 ) {
         crosses = 1;
         x = xcc + (xdd - xcc) * (y4 - ycc)/(ydd - ycc);
         if (y4 > ycc) {
             xdd = x;
             ydd = y4;
         } else {
             xcc = x;
             ycc = y4;
         }
    }

    *xc = xcc;
    *yc = ycc;
    *xd = xdd;
    *yd = ydd;
    if ( (IN_RANGE( xcc, x3, x4)) && (IN_RANGE( ycc, y3, y4 )) &&
         (IN_RANGE( xdd, x3, x4)) && (IN_RANGE( ydd, y3, y4 )) ) {
         return(crosses ? 1 : 0);
     } else {
         return(-1);
     }
}

/*
 * set the location
 */
int
sm_location(ax1,ax2,ay1,ay2)
int ax1,ax2,ay1,ay2;
{
   if(ax1==ax2 || ay1==ay2) {
      msg("Illegal range\n");
      return(-1);
   }

   if(windows && sm_verbose > 0) {
      msg("It's confusing to mix LOCATION with WINDOW; caveat locator\n");
   }

   gx1=ax1; gy1=ay1;
   gx2=ax2; gy2=ay2;
   set_scale();
   reset_label_pos();
   return(0);
}


/*
 * Sets the GX and GY so that there are NX windows across and NY windows up
 * and down, and the present GX and GY are for the KX, KY window where the
 * counting starts across at the lower left. If KX != KX2 or KY != KY2, the
 * window selected corresponds to the range of wondows KZ -- KZ2
 *
 * NX = NY = 1 resets to the state before WINDOW was called
 */
void
sm_window( nx, ny, kx, ky, kx2, ky2)
int nx, ny, kx, ky, kx2, ky2;
{
   int delta_x, delta_y,			/* size of each little box */
       off_x, off_y,			/* offset of box in its region */
       sep_x,sep_y,			/* seperation between regions */
       touch_x,touch_y;			/* should boxes touch in x/y? */
   static int oldgx1 = -1, oldgx2, oldgy1, oldgy2,
	      size_x,size_y,
   	      start_x,start_y,		/* bottom left corner of windows */
	      wx1 = 0, wx2 = 0, wy1 = 0, wy2 = 0;

   reset_label_pos();			/* invalidate cached label positions */

   if(nx == 1 && ny == 1) {		/* Return to original size */
      if(windows && oldgx1 >= 0) {
	 windows = 0;
	 (void)sm_location(oldgx1,oldgx2,oldgy1,oldgy2);
      } else {
	 windows = 0;
      }
      return;
   }

   if(nx == 0) {			/* support old-style window touching */
      nx = 1; ny = -ny;
   }
   if(ny == 0) {
      ny = 1; nx = -nx;
   }

   if(nx < 0) {				/* should the windows touch? */
      touch_x = 1;
      nx = -nx;
   } else if(nx == 1) {			/* so boxes fill window 1 1 1 1 box */
      touch_x = 1;
   } else {
      touch_x = 0;
   }
   if(ny < 0) {
      touch_y = 1;
      ny = -ny;
   } else if(ny == 1) {
      touch_y = 1;
   } else {
      touch_y = 0;
   }

   if(kx2 < 0) kx2 = kx;
   if(kx < 1) {
      if(sm_verbose > 0) {
	 msg("specified window is out of range; must be >= 1\n");
      }
      kx = 1;
   }
   if(kx > nx || kx2 > nx) {
      if(sm_verbose > 0) {
	 msg_1d("specified window is out of range; must be <= %d\n",ny);
      }
      kx2 = nx;
   }
   if(kx > kx2) kx = kx2;

   if(ky2 < 0) ky2 = ky;
   if(ky < 1) {
      if(sm_verbose > 0) {
	 msg("specified window is out of range; must be >= 1\n");
      }
      ky = 1;
   }
   if(ky > ny || ky2 > ny) {
      if(sm_verbose > 0) {
	 msg_1d("specified window is out of range; must be <= %d\n",ny);
      }
      ky2 = ny;
   }
   if(ky > ky2) ky = ky2;
/*
 * If this is the first call to window must save the new physical plot
 * location, so as to restore it when we are done with windows.
 */
   if(!windows) {
      oldgx1 = gx1;
      oldgx2 = gx2;
      oldgy1 = gy1;
      oldgy2 = gy2;

      set_sep_off(&sep_x,&sep_y,&off_x,&off_y);

      start_x = gx1 - off_x;
      start_y = gy1 - off_y;
      size_x = gx2 - start_x + sep_x;	/* windows stick out sep_x past gx2 */
      size_y = gy2 - start_y + sep_y;
   }
/*
 * magic formulae to keep the little boxes from overlapping
 * assume that the x-axis numbers have depth 2*cheight
 *		   	  label has depth    2*cheight
 *					(total off_x)
 *		   y-axis numbers have width 5*cwidth
 *		    	  label has width    2*cheight
 *					(total off_y)
 * allow for a gap of sep_x between boxes horizontally, and sep_y vertically
 *
 * You can modify the sizes of the `gutters' between the windows by setting
 * the SM variables $x_gutter or $y_gutter
 *
 * Also allow a space of width start_x and depth start_y at the bottom left
 * of the collection of windows:
 *
 * | start || off | window | sep | off | window | sep | off | window | sep ||
 *               gx1                                                gx2
 *          <--------------------------- size ----------------------------->
 *
 * If nx or ny is negative, we want the boxes to touch. In this case
 * simply use the values of oldg[xy][12] to position the boxes (note that
 * we support the old syntax, where the other direction was specified as 0
 * to achieve this)
 */
   set_sep_off(&sep_x,&sep_y,&off_x,&off_y);

   delta_x = size_x/nx;
   delta_y = size_y/ny;

   if(touch_x) {
      wx1 = oldgx1 + (kx - 1)*(oldgx2 - oldgx1)/nx;
      wx2 = oldgx1 + kx2*(oldgx2 - oldgx1)/nx;
   } else {
      wx1 = (kx - 1)*delta_x + off_x + start_x;
      wx2 = (wx1 - off_x) + delta_x*(kx2 - kx + 1) - sep_x;
   }
   if(touch_y) {
      wy1 = oldgy1 + (ky - 1)*(oldgy2 - oldgy1)/ny;
      wy2 = oldgy1 + ky2*(oldgy2 - oldgy1)/ny;
   } else {
      wy1 = (ky - 1)*delta_y + off_y + start_y;
      wy2 = (wy1 - off_y) + delta_y*(ky2 - ky + 1) - sep_y;
   }
/*
 * now set the globals gx1, gx2, gy1, gy2 to the new window
 */
   windows = 0;				/* suppress warning from location */
   if(sm_location(wx1,wx2,wy1,wy2) < 0) {
      return;
   }
   windows = 1;
}

/*****************************************************************************/
/*
 * Calculate the magic separations and offsets
 */
static void
set_sep_off(sep_x,sep_y,off_x,off_y)
int *sep_x,*sep_y,*off_x,*off_y;
{
   REAL oldang,
   	zerop = 0.001;			/* 0' : not quite zero */
   float fac;				/* factor to adjust sizes by */
   char *ptr;

   
   oldang = aangle;
   set_angle(&zerop,1);			/* force software character sizes */
   *sep_x = cheight;
   *sep_y = 1.5*cwidth;
   *off_x = 2*cheight + 5*cwidth;
   *off_y = 2*cheight + 2*cheight;
   set_angle(&oldang,1);

   if(*(ptr = print_var("x_gutter")) != '\0') {
      fac = atof(ptr);
      *sep_x *= fac; *off_x *= fac;
   }
   if(*(ptr = print_var("y_gutter")) != '\0') {
      fac = atof(ptr);
      *sep_y *= fac; *off_y *= fac;
   }
}
