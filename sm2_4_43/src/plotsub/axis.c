#include "copyright.h"
/*********************************************************************/
/*       AXIS.C - draw box with axis                                 */
/*-------------------------------------------------------------------*/
/*       box()                                                       */
/*       axis()                                                      */
/*       ticksize()						     */
/*       format()						     */
/*       grid()							     */
/*       notation()						     */
/*								     */
/*********************************************************************/
#include "options.h"
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "vectors.h"
#include "defs.h"
#include "sm.h"
#include "sm_declare.h"

#define INT(X) (int)((X) + 0.5)
#define FRAC_MAX 0.8			/* maximum fraction of axis covered
					   by labels */
#define LABEL_OFFSET 400		/* distance of numbers from axis,
					   and axis label from numbers */

static char x_num_fmt[20]="",
	    y_num_fmt[20]="";
static float xsmall = 0.,
	     ysmall = 0.,
	     xbig   = 0.,
	     ybig   = 0.;
extern int sm_interrupt,			/* respond to ^C */
           sm_verbose;			/* how chatty should I be? */	    
static int check_ti(),
	   draw_axis(),
           ticklabel(),
	   x_numb_min = SCREEN_SIZE,	/* height + depth of x-axis numbers */
	   xlexp = -4,
           xhexp =  4,
	   y_numb_min = SCREEN_SIZE,	/* length of y-axis numbers */
           ylexp = -4,
           yhexp =  4,
	   TeX = -1;			/* Use TeX strings? */
static char *user_label = NULL;		/* user-supplied label for big ticks */
static void axistick(),
	    format_string(),
	    setup_ticks(),
  	    tick();
static void vec_axistick();
/*
 * these are the variables that control how to draw ticks and labels
 */
static float xasp,yasp;			/* aspect ratios */
static int draw_labels;			/* draw tick labels? */
static int is_clockwise;		/* ticks are clockwise */
static int is_parallel;			/* labels are parallel to axes */
static int no_ticks;			/* don't draw ticks */
static int size_bigtick;		/* length of big ticks */
static int size_smalltick;		/* and length of small ticks */
static float tick_sin, tick_cos;	/* axis direction for ticks */
/*
 * This one is an SM internal variable
 */
float label_offset_scale = 1.0;		/* How much to increase LABEL_OFFSET */

void
sm_box(labelxl, labelyl, labelxu, labelyr)
int labelxl, labelyl, labelxu, labelyr;
{
   REAL asave = aangle,
        ninety = 90.0,
        zero = 0.0;
   int lsave = lltype;

   if(lltype != -10) {			/* not erase */
      sm_ltype(0);
   }
   set_angle(&zero,1);
   
   x_numb_min = y_numb_min = SCREEN_SIZE;
/*
 * draw the entire box, so as to get the line joins right. But don't
 * try to get it right if some edges are missing.
 */
   if(labelxl == 4 || labelyl == 4 || labelxu == 4 || labelyr == 4) {
      ;					/* don't draw box here */
   } else {
      sc_relocate(gx1,gy1);
      sc_draw(gx2,gy1);
      sc_draw(gx2,gy2);
      sc_draw(gx1,gy2);
      sc_draw(gx1,gy1);
      sc_draw(gx2,gy1);
   }

   (void)sm_axis(fx1,fx2,xsmall,xbig,gx1,gy1,gx2-gx1,labelxl,0);
   (void)sm_axis(fx1,fx2,xsmall,xbig,gx1,gy2,gx2-gx1,labelxu,1);
   set_angle(&ninety,1);
   (void)sm_axis(fy1,fy2,ysmall,ybig,gx1,gy1,gy2-gy1,labelyl,1);
   (void)sm_axis(fy1,fy2,ysmall,ybig,gx2,gy1,gy2-gy1,labelyr,0);
   
   set_angle(&asave,1);
   sm_ltype(lsave);
}

void
sm_grid(line_freq,dirs)
int line_freq;  /* 0 for grid lines only at labelled ticks, 1 for all ticks */
int dirs;				/* which lines: 0: x&y, 1: x 2: y */
{
   REAL ninety = 90.0,
   	old_angle,
   	zero = 0.0;
   int alen;

   old_angle = aangle;

   draw_labels = 0;
/*
 * x axis
 */
   if(dirs == 0 || dirs & 01) {
      set_angle(&zero,1);
      setup_ticks(0,0);
      alen = (gx2 - gx1)/sqrt(pow(sm_cos*xasp,2.0) + pow(sm_sin/yasp,2.0));

      size_bigtick = (gy2 - gy1);
      size_smalltick = (line_freq) ? size_bigtick : 0;
      axistick(fx1, fx2, xsmall, xbig, gx1, gy1, alen, tick );
   }
/*
 * y axis
 */
   if(dirs == 0 || dirs & 02) {
      set_angle(&ninety,1);
      setup_ticks(0,1);
      alen = (gy2 - gy1)/sqrt(pow(sm_cos*xasp,2.0) + pow(sm_sin/yasp,2.0));

      size_bigtick = (gx2 - gx1);
      size_smalltick = (line_freq) ? size_bigtick : 0;
      axistick(fy1, fy2, ysmall, ybig, gx1, gy1, alen, tick );
   }
   set_angle(&old_angle,1);
}

/*
 * set axis label formats
 */
void
sm_format( x_format, y_format )
char *x_format, *y_format;
{
   if(!strcmp(x_format,"1")) {
       x_num_fmt[0] = '\0';
   } else if(strcmp(x_format,"0") != 0) {
       format_string( x_format, x_num_fmt);
   }
   if(!strcmp(y_format,"1")) {
       y_num_fmt[0] = '\0';
   } else if(strcmp(y_format,"0") != 0) {
       format_string( y_format, y_num_fmt);
   }
}

void
sm_ticksize(asx,abx,asy,aby)
double asx,abx,asy,aby;
{
   if(check_ti(asx,abx) == 0) {
      xsmall = asx; xbig = abx;
   }
   if(check_ti(asy,aby) == 0) {
      ysmall = asy; ybig = aby;
   }
}

static int
check_ti(small,big)
double small,big;
{   
   if(small > 0.0) {
      if(small > big) {
	 msg("Small tick spacing must be <= big spacing\n");
	 return(-1);
      }
   } else if(small == 0.0) {
      if(big != 0.0) {
	 msg("You cannot specify the big tick spacing but not the small\n");
	 return(-1);
      }
   } else {
      if(big != 0.0 && (-small) > fabs(big)) {
	 msg("Small tick spacing must be <= big spacing\n");
	 return(-1);
      }
   }
   return(0);
}


/*---------------------------------------------------------------------------*/
/*	Makes a labeled axis from A1 to A2 at (AX,AY), length ALEN.	     */
/*	  If ABig > 0, use it for spacing of large ticks		     */
/*	  If ASmall <0, make logarithmic axis, if =0, the default.	     */
/*	  If ASmall >0, try to use it for the small ticks.		     */
/*	  ILABEL = 0 for no labels, 1 for parallel, 2 for perpend.	     */
/*        ICLOCK%2 = 0 for counterclockwise, 1 for clockwise ticks	     */
/*        ICLOCK/2 = 0 for orthogonal, 1 for vertical, 2 for horizontal ticks*/
/*---------------------------------------------------------------------------*/
int
sm_axis(a1,a2,as,ab,ax,ay,alen,ilabel,iclock)
double a1,a2,			/* labels from a1 to a2  */
       as,ab; 			/* if as < 0, logarithmic */
int ax,ay,alen,
    ilabel,iclock;
{
   int ax2,ay2;
   
   if(sm_interrupt) return(-1);

   if(ilabel == 4) return(0);		/* don't draw axis */

   setup_ticks(ilabel,iclock);

   alen /= sqrt(pow(sm_cos*xasp,2.0) + pow(sm_sin/yasp,2.0));
   ax2 = ax + alen*sm_cos*xasp + 0.5;
   ay2 = ay + alen*sm_sin/yasp + 0.5;
   sm_grelocate((float)ax,(float)ay);
   gdraw((float)ax2,(float)ay2);

   if(ilabel == 3) return(0);		/* don't mark ticks on axis */

   axistick(a1,a2,as,ab,ax,ay,alen);

   return(0);
}

/*****************************************************************************/
/*
 * Like axis, but instead of specifying as and ab specify two vectors,
 * giving the big and small ticks
 */
void
vec_axis(a1,a2,small,big,lab,ax,ay,alen,ilabel,iclock)
double a1,a2;
VECTOR *small,*big;
VECTOR *lab;
int ax,ay,alen,
    ilabel,iclock;
{
   int ax2,ay2;
   
   if(sm_interrupt) return;

   setup_ticks(ilabel,iclock);

   alen /= sqrt(pow(sm_cos*xasp,2.0) + pow(sm_sin/yasp,2.0));
   ax2 = ax + alen*sm_cos*xasp + 0.5;
   ay2 = ay + alen*sm_sin/yasp + 0.5;
   sm_grelocate((float)ax,(float)ay);
   gdraw((float)ax2,(float)ay2);

   if(ilabel == 3) return;		/* don't mark ticks on axis */

   vec_axistick(a1,a2,big,1,ax,ay,alen,lab);
   vec_axistick(a1,a2,small,0,ax,ay,alen,(VECTOR *)NULL);
}

static void
setup_ticks(ilabel,iclock)
int ilabel;
int iclock;
{
   int same_side;			/* draw ticks on same side as labels?*/
   
   if(iclock < 0 || (iclock & ~0xf) != 0) {
      msg_1d("Invalid value %d of iclock in setup_ticks; setting to 0\n",
	     							       iclock);
      iclock = 0;
   }

   TeX = (*print_var("TeX_strings") != '\0'); /* do we want TeX strings? */
   
   if(aspect > 1.0) {
      xasp = aspect;
      yasp = 1.0;
   } else {
      xasp = 1.0;
      yasp = aspect;
   }
/*
 * Process iclock
 */
   no_ticks = 0;

   switch (iclock & 0x1) {
    case 0: is_clockwise = 0; break;
    case 1: is_clockwise = 1; break;
   }
   iclock >>= 1;

   switch (iclock & 0x3) {
    case 0:				/* orthogonal tick */
      tick_sin = sm_sin;				
      tick_cos = sm_cos;
      break;
    case 1:				/* vertical tick */
      tick_sin = 0;
      tick_cos = (sm_cos>0)? 1 : -1;
      break;
    case 2:				/* horizontal tick */
      tick_sin = (sm_sin>0)? 1 : -1;
      tick_cos = 0;
      break;
    case 3:				/* no ticks at all (just labels) */
      no_ticks = 1;
      break;
   }
   iclock >>= 2;

   switch (iclock & 0x1) {
    case 0: same_side = 0; break;
    case 1: same_side = 1; break;
   }
   iclock >>= 1;
/*
 * now ilabel
 */
   draw_labels = 1;
   if(ilabel == 0) {
      draw_labels = 0;			/* don't label axis */
   } else if(ilabel == 1) {
      is_parallel = 1;
   } else {
      is_parallel = 0;
   }

   size_bigtick = (cheight/1.5)/hypot(tick_cos*xasp, tick_sin/yasp);
   if(same_side) {
      size_bigtick = -size_bigtick;
   }
   size_smalltick = size_bigtick/2;
}

/*****************************************************************************/
/*
 * functions to actually draw and label ticks
 */
static void
axistick(a1,a2,as,ab,ax,ay,alen)
double a1,a2,as,ab;
int ax,ay,alen;
{
   double step;
   int big = 0,				/* force a big tick, initialised to
					   placate flow-analysing compilers */
       ticks, smticks;
   
   if(a1 == a2) return;
/*
 *  work out the placement of the ticks for the case where the user
 *  lets SM decide the big tick spacing
 */
   if(as >= 0 ) {
      if(ab == 0) {
   	 float adiff, diff;
	 float frac;			/* fraction of axis filled by labels */
	 int exp_val;
	 float mantissa;
	 int nchars, nlabel;
   	 REAL oldang = aangle;
	 REAL zerop = 0.001;		/* zero + delta */
	 
	 nchars = 10;
/*
 * now work out how many labels can fit on the axis, depending on
 * axis length,  character size, number of characters allowed
 */
	 set_angle(&zerop,1);		/* force software chars for cheight */
	 nlabel = (alen/(nchars*cheight)) + 0.5;
	 set_angle(&oldang,1);
	 if(nlabel < 4) {
	    nlabel = 4;			/* at least 4 values initially */
	 } else if(nlabel > 200) {	/* too many. */
	    msg_1d("You have asked for %d axis labels; I'll give you 200\n",
		   nlabel);
	    if(sm_verbose && eexpand < 1e-2) {
	       msg_1f("Check the value of expand (currently %g)\n",eexpand);
	    }
	    nlabel = 200;
	 }
/*
 *  now work out the numeric range in the labels
 */
	 adiff = fabs(a2 - a1);
	 for(;;) {
	    diff = log10( adiff/ nlabel );
	    exp_val = diff;
	    if(diff < 0) exp_val = exp_val - 1;
	    mantissa = diff - exp_val;
	    
	    if(mantissa < .15 ) {
	       ticks = 1;
	       smticks = 5;
	    } else if ( mantissa < .5 ) {
	       ticks = 2;
	       smticks = 4;
	    } else if (mantissa < .85 ) {
	       ticks = 5;
	       smticks = 5;
	    } else {
	       ticks = 10;
	       smticks = 5;
	    }
/*
 * now work out the values at which each tick mark should be placed
 */
	    step = ticks*pow(10.0,(float)exp_val);

	    if(as > 0) {  /* The user specified only the SMALL tick spacing */
	       smticks = step/as + 0.5;
	    }
/*
 * we have an initial guess. See if it'd produce too crowded an axis
 */
	    frac = (float)draw_axis(a1,a2,as,ab,ax,ay,alen,
				    smticks,step,big,0)/alen;
	    if(sm_verbose > 1) {
	       msg_1f("Labels fill %.0f%% of axis\n",100*frac);
	    }
	    if(frac < FRAC_MAX || nlabel == 1) { /* we are done */
	       break;
	    } else {
	       nlabel--;
	    }
	 }
      } else if(ab > 0) {	/* user chose big and small ticks */
	 if(as == 0) {
	    as = ab/5;
	 }
	 step = ab;
	 smticks = step/as + 0.5;
      } else {				/* ab < 0 */
	 msg("You can't have big spacing < 0 with non-logarithmic axes\n");
	 return;
      }
   } else {				/* a logarithmic axis is specified  */
      step = (ab == 0.0) ? 10 : (ab > 0.0 ? ab : -ab);
      smticks = step/(-as) + 0.5;
   }

   draw_axis(a1,a2,as,ab,ax,ay,alen,smticks,step,big,1);
}

/*
 * Actually draw an axis now that we know what axis we want drawn.
 * If as < 0, a1 and a2 are the log_10 of the desired values
 *
 * return the total length of axis labels
 */
static int
draw_axis(a1,a2,as,ab,ax,ay,alen,smticks,step,big,drawit)
double a1,a2,as,ab;
int ax,ay,alen;
int smticks;
double step;
int big;
int drawit;				/* if true, actually draw labels */
{
   int i;
   int label_length = 0;		/* total length of labels drawn */
   int lscale;				/* lg(scale factor for value) */
   float length;
   int offset;
   double tick_place;
   double value;
   float xpos, ypos;
   
   if(a2 < a1) step = -step;
   if(as >= 0) {
      offset = (int)(a1/step);
      if(a1/step < 0) offset--;
      tick_place = step*offset;
      lscale = 0;
   } else {
      if(a2 > a1) {
	 offset = (int)a1;
      } else {
	 offset = (int)ceil(a1);
	 step /= 10.;
      }
      if(a1 < 0) offset--;
      lscale = offset;
      if(a2 > a1) {
	 tick_place = 0.0;
      } else {
	 tick_place = 1.0;
      }
   }
/*
 * these are the loops for making the tick marks; the while loop for the big
 * ticks and the for loop for the small ticks 
 */
   a1 -= 1.0e-6*(a2 - a1);	/* allow for roundoff */
   a2 += 1.0e-6*(a2 - a1);

   while(!sm_interrupt) {
      for(i = 0;i < smticks;i++) {
	 value = tick_place + step*i/smticks;
	 if (fabs(value) < fabs(step * 1.0e-12)) {
	    value = 0.0;
	 }
	 if(as >= 0) {
	    length = alen * (value - a1)/(a2 - a1);
	 } else {
	    if(ab >= 0.0 && value > 10.0) break;
	    if(value <= 0.0) {
	       if(smticks > 1) {	/* we'll never get anywhere if i==1 */
		  big = 1;		/* force next tick to be big */
		  continue;
	       } else {
		  value = 1;
	       }
	    } else if(value < 0.1) {
	       continue;
	    }
	    length = alen*(log10(value) + lscale - a1)/(a2 - a1);
	 }
	 if(length < -1) {
	    big = 0;			/* don't need a big tick */
	    continue;
	 } else if(length > alen) {
	    return(label_length);
	 }
	 xpos = length*sm_cos*xasp + ax + 0.5;
	 ypos = length*sm_sin/yasp + ay + 0.5;
	 if(drawit) {
	    tick(xpos,ypos,(i == 0 || big));
	    if(draw_labels) {
	       label_length += ticklabel(xpos,ypos,value,lscale,
					 		    (i == 0 || big),1);
	    }
	 } else {
	    label_length += ticklabel(xpos,ypos,value,lscale,
				      			    (i == 0 || big),0);
	 }
	 big = 0;
      }
      tick_place += step;
      if(as < 0.0) {
	 if(a2 > a1) {
	    if(tick_place >= 9.9999) {
	       tick_place = 0.0;
	       lscale++;
	       if(ab < 0) step /= 10;
	    }
	 } else {
	    if(tick_place < 0.1) {
	       tick_place = 1.0;
	       lscale--;
	       if(ab < 0) step *= 10;
	    }
	 }
      }
   }

   return(label_length);
}

static void
vec_axistick(a1,a2,vec,big,ax,ay,alen,vec_lab)
double a1,a2;
VECTOR *vec;				/* vector of tick marks */
int big;				/* are they big ticks? */
int ax,ay,alen;
VECTOR *vec_lab;			/* vector of labels to draw */
{
   double value;
   float length,
    	 xpos, ypos;
   int i;

   for(i = 0;i < vec->dimen;i++) {
      value = vec->vec.f[i];
      if(value < a1 || value > a2) continue;
      length = alen*(value - a1)/(a2 - a1);
      xpos = length*sm_cos*xasp + ax;
      ypos = length*sm_sin/yasp + ay;
      if(vec_lab == NULL) {
	 user_label = NULL;
      } else {
	 user_label = (vec_lab->dimen == 1) ?
				     vec_lab->vec.s.s[0] : vec_lab->vec.s.s[i];
      }
      tick(xpos,ypos,big);
      if(draw_labels) {
	 (void)ticklabel(xpos,ypos,value,0,big,1);
      }
   }
   user_label = NULL;
}

/*****************************************************************************/
/*
 * Draw the axis labels
 */
void
sm_xlabel( str )
char *str;
{
   float slength,
   	 sdepth,sheight,
	 xstart,ystart;
   int lsave;
   
   lsave = lltype;
   if(lltype != -10) sm_ltype( 0 );
   string_size(str,&slength,&sdepth,&sheight);
   xstart = gx1 + (gx2 - gx1 - slength)/2.0;

   if(y_numb_min == SCREEN_SIZE) {	/* no idea where axis labels are */
      ystart =  gy1 - 1.5*cheight;
   } else {
      ystart = y_numb_min;
   }
   ystart -= sheight + eexpand*LABEL_OFFSET*atof(print_var("label_offset"));
   if(ystart < 100 + sdepth) {
      if(sm_verbose) {
	 int dy  = (100 + sdepth) - ystart;
	 dy = 500*(int)(dy/500 + 0.999);
	 if(dy > 0) {
	    msg("The x-axis label will probably overlap the tick numbers;\n");
	    msg_1d("you may want to increase the starting y LOCATION by %d\n",
		   dy);
	 }
      }
      ystart = 100 + sdepth;
   }

   sm_grelocate(xstart,ystart);
   draw_string(str);
   sm_ltype( lsave );
}

void
sm_ylabel( str )
char *str;
{
   float slength,
   	 sdepth,sheight,
     	 xstart, ystart;
   int lsave;
   REAL angsave,
  	ninety = 90.0;
   
   angsave = aangle;
   lsave = lltype;
   if(lltype != -10) sm_ltype( 0 );
   if(aangle == 0.0) {
      set_angle(&ninety,1);
   }
   string_size(str,&slength,&sdepth,&sheight);
   
   if(x_numb_min == SCREEN_SIZE) {	/* no idea where axis labels are */
      xstart =  gx1 - 4*cwidth - sdepth;
   } else {
      xstart = x_numb_min;
   }
   xstart -= sdepth + eexpand*LABEL_OFFSET*atof(print_var("label_offset"));
   if(xstart < 100 + sheight) {
      if(sm_verbose) {
	 int dx  = (100 + sheight) - xstart;
	 dx = 500*(int)(dx/500 + 0.999);
	 if(dx > 0) {
	    msg("The y-axis label will probably overlap the tick numbers;\n");
	    msg_1d("you may want to increase the starting x LOCATION by %d\n",
		   dx);
	 }
      }
      xstart = 100 + sheight;
   }

   ystart = gy1 + (gy2 - gy1 - slength)/2.0;
   sm_grelocate(xstart,ystart);
   draw_string(str);
   set_angle(&angsave,1);
   sm_ltype(lsave);
}

/*****************************************************************************/
/*
 * Invalidate the saved label positions
 */
void
reset_label_pos()
{
   x_numb_min = y_numb_min = SCREEN_SIZE;
}

/*************************************************************/
/*
 * draw the tick marks at position xloc, yloc
 */
static void
tick(xloc,yloc,is_bigtick)
double xloc, yloc;
int is_bigtick;
{
   int size;			/* size of tickmark */
   float xtick, ytick;		/* end of tick */

   if(no_ticks) {
      return;
   }

   size = (is_bigtick) ? size_bigtick : size_smalltick;
   if(is_clockwise) {
      size = -size;
   }

   xtick = xloc - size*tick_sin;
   ytick = yloc + size*tick_cos;

   sm_grelocate(xloc,yloc);
   gdraw(xtick,ytick);
}

/*
 *  put on the tick labels  at location xloc, yloc. Note that if the ticks
 * are clockwise, the labels are anticlockwise.
 * Return the length of the label parallel to the axis
 */

static int
ticklabel(xloc,yloc,value,lscale,is_bigtick,drawit)
double xloc, yloc,
       value;
int lscale;
int is_bigtick;
int drawit;				/* actually draw label? */
{
    static char string[41];
    REAL ang;
    float dx, dy, xstart = 0, ystart = 0; /* initialise for gcc's happiness */
    int label_offset;			/* scaled LABEL_OFFSET */
    float sdepth,sheight,slength;
    float par_length;

    if(!is_bigtick) {
       return(0);
    }
    if(user_label != NULL) {
       strcpy(string,user_label);
    } else {
       num_to_ascii(value,lscale,string);
    }

    label_offset = eexpand*LABEL_OFFSET*atof(print_var("label_offset")) + 0.5;

    if (is_parallel) {			/* parallel to axis */
       string_size(string,&slength,(float *)NULL,&sheight);
       par_length = slength;
       if(drawit) {
	  dx = -slength/2;
	  if(is_clockwise) {		/* clockwise */
	     dy = label_offset;
	  } else {				/* widdershins */
	     dy = -(label_offset + sheight);
	  }
	  
	  dx /= sqrt(pow(sm_cos*xasp,2.0) + pow(sm_sin/yasp,2.0));
	  dy /= sqrt(pow(sm_sin*xasp,2.0) + pow(sm_cos/yasp,2.0));
	  xstart = (dx*sm_cos - dy*sm_sin)*xasp + xloc;
	  ystart = (dx*sm_sin + dy*sm_cos)/yasp + yloc;
	  sm_grelocate(xstart,ystart);
	  draw_string(string);
       }
    } else {				/* perpendicular to axis */
       ang = aangle - 90;
       set_angle(&ang,1);
       string_size(string,&slength,&sdepth,&sheight);
       par_length = sheight + sdepth;
       if(drawit) {
	  if(is_clockwise) {		/* clockwise */
	     dx = -slength - label_offset;
	  } else {				/* widdershins */
	     dx = label_offset;
	  }
	  dy = -sheight/2;
	  
	  dx /= sqrt(pow(sm_cos*xasp,2.0) + pow(sm_sin/yasp,2.0));
	  dy /= sqrt(pow(sm_sin*xasp,2.0) + pow(sm_cos/yasp,2.0));
	  xstart = (dx*sm_cos - dy*sm_sin)*xasp + xloc;
	  ystart = (dx*sm_sin + dy*sm_cos)/yasp + yloc;
	  sm_grelocate(xstart,ystart);
	  draw_string(string);
       }
       ang = aangle + 90;
       set_angle(&ang,1);
    }

    if(drawit) {
       if(x_numb_min > xstart && xstart < gx1) x_numb_min = xstart;
       if(y_numb_min > ystart && ystart < gy1) y_numb_min = ystart;
    }

    return(par_length);
}

/******************************************************/
/*
 * convert a number to a string for plotting
 *
 *  value    = the value to be printed on the plot (input)
 *  string = string to be printed on the plot (output)
 */
#define NDIGIT 6			/* maximum number of digits */

void
num_to_ascii(value,lscale,string)
double value;
int lscale;
char *string;
{
   char *char_value,
	*s;
   int decpt,				/* position of decimal point */
       exponent = 0,			/* exponential notation? */
       lexp,hexp,			/* lowest and highest exponents */
       line_up,				/* how to line up exponent */
       sign;
   
   if(aangle <= 45.) {     /*   x axis   */
      lexp = xlexp;
      hexp = xhexp;
      if (x_num_fmt[0] != '\0' ) {	/* user supplied the x-axis format */
         (void)sprintf(string,x_num_fmt,value*pow(10.0,(float)lscale));
         return;
      } 
   } else {
      lexp = ylexp;
      hexp = yhexp;
      if(y_num_fmt[0] != '\0' ) {	/* the user supplied y-axis format */
         (void)sprintf( string, y_num_fmt,value*pow(10.0,(float)lscale));
         return;
      }
   }
   if(TeX < 0) {
      TeX = (*print_var("TeX_strings") != '\0'); /* do we want TeX strings? */
   }
/*
 * OK, now deal with the number ourelves
 */
   char_value = ecvt(value,NDIGIT,&decpt,&sign);
   decpt += lscale;
   for(s = char_value + NDIGIT - 1;*s == '0';s--) *s = '\0';
   
   s = string;
   if(*char_value == '\0') {
      sprintf(s,"0");
      return;
   }
   if(decpt <= lexp || decpt > hexp) {
      exponent = decpt - 1;
      decpt = 1;
   }
   if(sign) *s++ = '-';
   if(decpt > 0) {
      while(decpt-- != 0) {
	 *s++ = (*char_value == '\0' ? '0' : *char_value++);
      }
      if(*char_value != '\0') *s++ = '.';
      while(*char_value != '\0') *s++ = *char_value++;
   } else {
      sprintf(s,"0."); s += 2;
      while(decpt++ != 0) *s++ = '0';
      while(*char_value != '\0') *s++ = *char_value++;
   }
   *s = '\0';
   if(exponent == 0 && lexp != hexp) {
      return;
   }
/*
 * Now the exponential part
 */
   if(string[0] == '1' && string[1] == '\0') {
      s = string;			/* string is "1" from string */
   } else {
      (void)sprintf(s,TeX ? "\\times " : "\\g*");
      s += TeX ? 7 : 3;
   }
   (void)sprintf(s,TeX ? "10^{" : "10\\\\u");
   s += TeX ? 4 : 5;
/*
 * now line the exponents up prettily (i.e. if exponent between 0 and 9,
 * put a space between the 10 and the exponent, else not) if $line_up_exponents
 * is not defined. If it's defined and equal to 1 use a space, otherwise
 * a + sign
 */
   line_up = atoi(print_var("line_up_exponents"));
   if(exponent >= 0 && exponent <= 9 && line_up > 0) {
      *s++ = (line_up == 1) ? ' ' : '+';
   }
   (void)sprintf(s,"%d",exponent);

   if(TeX) {
      while(*s != '\0') s++;
      sprintf(s,"}");
   }
   return;
}

/************************************************/
/*
 * Given a Fortran format string, produce a C one. If the format
 * includes a %, it is passed through unscathed.
 */
static void
format_string(in,out)
char *in,			/* input format */
     *out;			/* output format */
{
   char c;
   int i,j,n;

   (void)str_scanf(in,"%c",&c);
   if(c == 'd' || c == 'D' || c == 'E') c = 'e';
   else if(c == 'F') c = 'f';
   else if(c == 'G') c = 'g';
   
   switch (c) {
   case '\0':
      return;
   case '%':
      (void)sprintf(out,"%s",in);
      return;
   case 'e':
   case 'f':
   case 'g':
      if((n = str_scanf(in,"%*c%d.%d",&i,&j)) <= 0) {	/* no width field */
	 (void)sprintf(out,"%%%c",c);
      } else if(n == 1) {
	 (void)sprintf(out,"%%%d%c",i,c);
      } else {
	 (void)sprintf(out,"%%%d.%d%c",i,j,c);
      }
      break;
   case 'i':
   case 'I':
      if(str_scanf(in,"%*c%d.%d",&i) <= 0) {	/* no width field */
	 (void)sprintf(out,"%%g");
      } else {
	 (void)sprintf(out,"%%%dg",i);
      }
      break;
   default:
      msg_1s("Illegal format %s\n",in);
      (void)sprintf(out,"%%g");
      break;

   }
}

void
sm_notation(xlo, xhi, ylo, yhi)
double xlo, xhi, ylo, yhi;
{
   if(xlo == 0 && xhi == 0) {
      xlo = -4; xhi = 4;
   }
   if(ylo == 0 && yhi == 0) {
      ylo = -4; yhi = 4;
   }
   xlexp = xlo;
   xhexp = xhi;
   ylexp = ylo;
   yhexp = yhi;
}
