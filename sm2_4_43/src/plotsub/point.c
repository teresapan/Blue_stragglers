#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "vectors.h"
#include "yaccun.h"
#include "sm.h"
#include "devices.h"
#include "defs.h"
#include "sm_declare.h"

#define NS 1				/* markers are given as PTYPE N S */
#define STRING 2			/* markers are strings */
#define SYMBOL 4			/* markers are special symbols */
#define SYMSIZE 50		/* Maximum number of points in user symbol */

static char *point_strings = NULL;
static char str_name[40];		/* name of string ptype vector */
static REAL *point_style = NULL;
extern int sm_interrupt;			/* true following a ^C */
static int nsym = 0,			/* num. of points in special PTYPE */
      	   num_pt_string = 0,		/* number of strings specified */
           num_pt_style = 0,
	   symbol[3*SYMSIZE],		/* list describing special PTYPE */
  	   type_of_points = NS;		/* what sort of points? */

void
sm_ptype(pstyle,nstyle)
REAL *pstyle;
int nstyle;
{
   if(point_style != NULL) {
      free((char *)point_style);
      point_style = NULL;
   }

   if(nstyle == 0) {
      num_pt_style = 0;
      return;
   }

   if((point_style = (REAL *)malloc((unsigned)nstyle*sizeof(REAL)))
       								== NULL) {
        msg("Can't allocate space for point style vector!\n");
	num_pt_style = 0;
        return;
   }
   num_pt_style = nstyle;
   
   if(pstyle == NULL) {			/* use default */
      point_style[0] = 41;
      return;
   }

   (void)memcpy((char *)point_style,(Const char *)pstyle,
		nstyle*sizeof(REAL));
   type_of_points = NS;
}

/*
 * Return a string describing the current ptype
 */
char *
whatis_ptype()
{
   static char value[50];
   
   switch(type_of_points) {
    case NS:
      if(point_style == NULL) {
	 (void)sprintf(value,"4 1");
      } else if(num_pt_style == 1) {
	 (void)sprintf(value,"%d %d",(int)point_style[0]/10,
		       (int)point_style[0]%10);
      } else {
	 (void)sprintf(value,"%d %d %d",(int)point_style[0]/10,
		       (int)point_style[0]%10, num_pt_style);
      }
      return(value);
    case STRING:
      return(str_name);
    case SYMBOL:
      return("(symbol)");
    default:
      return("(unknown)");
   }
   /*NOTREACHED*/
}

/*********************************************************/
/*
 * Specify a set of strings to label points
 * The strings are passed as a single array of npoint contiguous strings
 * of length VEC_STR_SIZE each, and correspond to the second part of
 * the VEC_STRING struct in a VECTOR
 */
void
ptype_str(name,strings,npoint)
char *name;				/* name of vector */
char *strings;
int npoint;
{
   if(point_style != NULL) {
      free((char *)point_style);
      point_style = NULL;
   }

   if(point_strings != NULL) {
      free(point_strings);
      point_strings = NULL;
   }
   
   if(npoint == 0) {
      num_pt_string = 0;
      return;
   }

   if((point_strings = malloc((unsigned)npoint*VEC_STR_SIZE)) == NULL) {
      msg("Can't allocate space for ptype's strings\n");
      num_pt_string = 0;
      return;
   }
   num_pt_string = npoint;

   (void)memcpy(point_strings,strings,num_pt_string*VEC_STR_SIZE);

   type_of_points = STRING;
   strncpy(str_name,name,sizeof(str_name) - 1);
}

void
sm_points( x, y, npts )
REAL x[], y[];
int npts;
{
   float expfrac, temp;
   int j,n,
       nside = 4, istyle = 1;
   REAL oldangle, oldexp;

   if(type_of_points == SYMBOL) {
      sm_plotsym(x,y,npts,symbol,nsym);
      return;
   } else if(type_of_points == STRING) {
      if(point_strings == NULL) {
	 msg("no PTYPE strings are defined\n");
	 return;
      }
      if(num_pt_string == 1) {
	 for(j = 0;!sm_interrupt && j < npts;j++) {
	    sm_relocate(x[j],y[j]);
	    sm_putlabel(5,point_strings);
	 }
      } else {
	 if(npts != num_pt_string) {
	    msg("number of points doesn't match number of PTYPE strings\n");
	    if(npts > num_pt_string) npts = num_pt_string;
	 }
	 for(j = 0;!sm_interrupt && j < npts;j++) {
	    sm_relocate(x[j],y[j]);
	    sm_putlabel(5,point_strings + j*VEC_STR_SIZE);
	 }
      }
      return;
   } else if(type_of_points != NS) {
      msg_1d("Unknown sort of ptype: %s\n",type_of_points);
      return;
   }
/*
 * Must be (a vector of) regular PTYPE n s points
 */
   if(point_style == NULL) {
      sm_ptype((REAL *)NULL,1);		/* set default type */
   }

   oldexp = eexpand;
   oldangle = aangle;
/*
 * First deal with the points that have a vector of ptypes, then the rest
 */
   n = (num_pt_style < npts) ? num_pt_style : npts;
   for(j = 0;!sm_interrupt && j < n;j++) {
      temp = point_style[j] + .001;
      nside = temp/10;
      istyle = (int)temp % 10;
      expfrac = temp - (int)temp;
      if(expfrac < .01) {		/* no expansion is specified */
	 if(j < n_expand_vec) {
	    set_eexpand(expand_vec[j]);
	 } else {
	    set_eexpand(oldexp);
	 }
      } else {
	 set_eexpand(oldexp*expfrac);
      }
	 
      if(j < n_angle_vec) {
	 aangle = angle_vec[j];
	 sm_sin = sin(aangle*PI/180);
	 sm_cos = cos(aangle*PI/180);
      } else if(j == n_angle_vec) {
	 set_angle(&oldangle,1);
      }
	 
      if (IN_RANGE(x[j], fx1,fx2) && IN_RANGE(y[j], fy1,fy2)) {
         sm_draw_point(x[j],y[j],nside,istyle );
      }
   }  
/*
 * All the rest are the same ptype, but expansions may differ
 */
   for(;!sm_interrupt && j < npts; j++) {
      if(j < n_angle_vec) {
	 aangle = angle_vec[j];
	 sm_sin = sin(aangle*PI/180);
	 sm_cos = cos(aangle*PI/180);
      } else if(j == n_angle_vec) {
	 set_angle(&oldangle,1);
      }
      temp = point_style[0] + .001;
      nside = temp/10;
      istyle = (int)temp % 10;
      expfrac = temp - (int)temp;
      if(expfrac < .01) {		/* no expansion is specified */
	 if(j < n_expand_vec) {
	    set_eexpand(expand_vec[j]);
	 } else {
	    set_eexpand(oldexp);
	 }
      } else {
	 set_eexpand(oldexp*expfrac);
      }
      if (IN_RANGE(x[j], fx1,fx2) && IN_RANGE(y[j], fy1,fy2)) {
         sm_draw_point(x[j],y[j],nside,istyle);
      }
   }  
   
   set_angle(&oldangle,1);
   set_eexpand(oldexp);
}

/**************************************************/
/*
 * Only plot those points for which l is true
 */
void
sm_points_if( x, y, l, npts )
REAL x[], y[],
      l[];
int npts;
{
   int i,j;

   for(i = 0;i < npts;i++) {
      if(!l[i]) break;
   }

   for(j = i;i < npts;i++) {
      if(l[i]) {
	 x[j] = x[i];
	 y[j++] = y[i];
      }
   }

   sm_points(x,y,j);
}

/**************************************************/
/*
 * Plot a user defined symbol at each position (x,y).
 * The symbol is defined by the array symbol, where each triple
 * of values means flag dx dy. Flag is 1 to relocate rather than draw,
 * and dx and dy are offsets from (x,y) in screen coordinates. The current
 * expand and angle are applied to these offsets.
 */
void
sm_plotsym(x,y,npoint,insym,n)
REAL x[],y[];				/* data points */
int npoint,				/* size of x and y */
    insym[],				/* array describing symbol */
    n;					/* number of points in insym */
{
   float ca,sa,				/* cos and sin of angle */
         ex_x,ex_y;			/* expansion (aspect corrected) */
   int i,j,
       ix,iy,
       *sym;

   if((sym = (int *)malloc(3*n*sizeof(int))) == NULL) {
      fprintf(stderr,"Can't allocate storage for sym in plotsym\n");
      return;
   }

   ca = sm_cos;				/* in case n_angle_vec pts are */
   sa = sm_sin;				/* out of range, let's play safe */
   ex_x = ex_y = expand_vec[0];
   if(aspect > 1.0) {			/* allow for aspect ratio of screen */
      ex_x *= aspect;
   } else if (aspect < 1.0) {
      ex_y /= aspect;
   }
   for(j = 0;j < n;j++) {		/* rotate and expand */
      sym[3*j + 1] = ex_x*(insym[3*j+1]*ca - insym[3*j + 2]*sa);
      sym[3*j + 2] = ex_y*(insym[3*j+2]*ca + insym[3*j + 1]*sa);
   }

   for(i = 0;i < npoint;i++) {
      ix = ffx + fsx * x[i];
      iy = ffy + fsy * y[i];
      if(IN_RANGE(ix,gx1,gx2) && IN_RANGE(iy,gy1,gy2)) {
	 if(i < n_angle_vec || i < n_expand_vec) {
	    if(i < n_angle_vec) {
	       ca = cos(PI*angle_vec[i]/180);
	       sa = sin(PI*angle_vec[i]/180);
	    } else {
	       ca = sm_cos;
	       sa = sm_sin;
	    }
	    ex_x = ex_y = expand_vec[i < n_expand_vec ? i : 0];
	    if(aspect > 1.0) {		/* allow for aspect ratio of screen */
	       ex_x *= aspect;
	    } else if (aspect < 1.0) {
	       ex_y /= aspect;
	    }
	    for(j = 0;j < n;j++) {		/* rotate and expand */
	       sym[3*j + 1] = ex_x*(insym[3*j+1]*ca - insym[3*j + 2]*sa);
	       sym[3*j + 2] = ex_y*(insym[3*j+2]*ca + insym[3*j + 1]*sa);
	    }
	 }

         sc_relocate(ix,iy);
	 for(j = 0;j < n;j++) {
	    if(insym[3*j]) {
	       sc_relocate(ix + sym[3*j+1],iy + sym[3*j+2]);
	    } else {
	       sc_draw(ix + sym[3*j+1],iy + sym[3*j+2]);
	    }
	 }
      }
   }
   free((char *)sym);
}

/*
 * Define a user-defined symbol
 */
void
setsym(list)
TOK_LIST *list;
{
   char c;
   int i,j;
   
   for(i = j = 0;j < 3*SYMSIZE && i < list->nitem;j += 3) {
      if(list->i_list[i]->str[0] == '\n' && list->i_list[i]->str[1] == '\0') {
	 continue;
      }
      if(c = list->i_list[i]->str[0], isalpha(c)) {
	 if(c == 'd' || c == 'D') {		/* draw */
	    symbol[j] = 0;
	 } else if(c == 'm' || c == 'M' ||
		   c == 'r' || c == 'R') {	/* move/relocate */
	    symbol[j] = 1;
	 } else {
	    msg_1s("Unknown letter in symbol definition `%s'\n",
		   list->i_list[i]->str);
	    symbol[j] = 0;
	 }
	 i++;
      } else {
	 symbol[j] = 0;
      }
      if(list->nitem - i < 2) {
	 msg("You must specify symbol points as `[d][mr] x y'\n");
	 nsym = 0;
	 break;
      }
      symbol[j + 1] = atoi(list->i_list[i]->str);
      symbol[j + 2] = atoi(list->i_list[i + 1]->str);
      i += 2;
   }
   if(j == SYMSIZE*3) {
      msg_1d("Sorry, only %d points are allowed in a symbol\n",SYMSIZE);
   }
   nsym = j/3;
   type_of_points = SYMBOL;
}

void
sm_dot()
{
   REAL uxp = (xp - ffx)/fsx,
	uyp = (yp - ffy)/fsy;
   
   if(type_of_points == SYMBOL) {
      sm_plotsym(&uxp,&uyp,1,symbol,nsym);
   } else {
      sm_points(&uxp,&uyp,1);
   }
}

/*
 * Now do the work of plotting points.
 */
#define INC_THETA			/* theta += dtheta */ \
  temp = c_theta*c_dtheta - s_theta*s_dtheta; \
  s_theta = s_theta*c_dtheta + c_theta*s_dtheta; \
  c_theta = temp
#define INC_THA INC_THETA		/* we reuse c_theta etc. for tha */
#define DEC_THB				/* thb -= thetab */ \
  temp = c_thetab*c_dtheta + s_thetab*s_dtheta; /* thb -= dtheta */ \
  s_thetab = s_thetab*c_dtheta - c_thetab*s_dtheta; \
  c_thetab = temp;
#define STELLAR 0.25			/* amount to star symbols by */
  
void
sm_draw_point(ux,uy,n,istyle)
double ux,uy;				/* user coordinates to draw point */
int n, istyle;
{
   float dtheta, theta,
         tha,				/* we never actually use thb */
         c_theta,s_theta,		/* cos/sin(theta) and cos/sin(tha) */
         c_thetab,s_thetab,		/* cos/sin(thb) */
         c_dtheta,s_dtheta,		/* cos/sin(dtheta) */
   	 ps_x,ps_y,			/* size of points */
   	 temp;
   int j,
       ldef2,				/* we need a local version of ldef */
       xp0,yp0,				/* xp,yp at start of point */
       ymin,ymax,			/* range in y of point */
       y,xx,yy,
       xa1, ya1, xa2, ya2, xb1, yb1, xb2, yb2,
       xa, ya, xc, yc, xb, yb, xmid, ymid,
       zig;				/* Are we zigging or zagging? */
   
   xp0 = ffx + fsx*ux;			/* screen coordinates */
   yp0 = ffy + fsy*uy;
   sc_relocate(xp0, yp0);

   if(n < 2) {
      if(DOT(xp0,yp0) < 0) {	/* failed */
	 draw_dline(xp0,yp0,xp0,yp0);
      }
      xp = xp0;
      yp = yp0;
      return;
   }
   
   xc = xp0;
   yc = yp0;
   
   ps_x = ps_y = PDEF*eexpand;		/* size of points */
   if(aspect > 1.0) {			/* allow for aspect ratio of screen */
      ps_x *= aspect;
   } else if (aspect < 1.0) {
      ps_y /= aspect;
   }
   
   dtheta = 2*PI/n;
   theta = (3*PI + dtheta)/2 + aangle*PI/180;
   c_theta = cos(theta);
   s_theta = sin(theta);
   c_dtheta = cos(dtheta);
   s_dtheta = sin(dtheta);

   xa = ps_x*c_theta + xc;  
   ya = ps_y*s_theta + yc;

   if(n == 2) {
      sc_relocate(xa,ya);
      INC_THETA;
      sc_draw((int)(ps_x*c_theta + xc),(int)(ps_y*s_theta + yc));
   } else if(istyle == 0) {
      sc_relocate(xa,ya);
      for (j=0;j < n;j++)  {
	 INC_THETA;
	 sc_draw((int)(ps_x*c_theta + xc),(int)(ps_y*s_theta + yc));
      }
   } else if (istyle == 1) {
      for(j = 1;j <= n;j++) {
	 sc_relocate(xa,ya);
	 if(2*(int)(n/2) == n) {
	    if(j > n/2) {
	       xp = xp0; yp = yp0;
	       return;
	    }
	    sc_draw(2*xc - xa,2*yc - ya);
	 } else {
	    sc_draw(xc,yc);
	 }
	 INC_THETA;
	 xa = ps_x*c_theta + xc;  
	 ya = ps_y*s_theta + yc;
      }
   } else if (istyle == 2) {
      sc_relocate(xa,ya);
      for (j=1; j<=n; j++)  {
	 INC_THETA;
	 xb = ps_x*c_theta + xc;  
	 yb = ps_y*s_theta + yc;
	 xmid = STELLAR*(xa + xb - 2*xc) + xc;
	 ymid = STELLAR*(ya + yb - 2*yc) + yc;
	 sc_draw(xmid,ymid);
	 sc_draw(xb,yb);
	 xa = xb;
	 ya = yb;
      }
   } else if (istyle == 3) {
      sc_relocate(xp,yp);
      if(FILL_PT(n) < 0) {
	 zig = 0;			/* start with a zag */
	 ymax = -1;
	 ymin = 0;
	 ldef2 = 0.8*ldef;		/* we need a smaller value */
/*
 * Find edges of symbol, and draw perimeter
 */
	 sc_relocate(xa,ya);
	 tha = theta;			/* placate flow-analysing compilers */
	 for(j = 1;j <= n;j++) {
	    INC_THETA;
	    y = ps_y*s_theta;
	    if(y < ymin) {
	       ymin = y;
	       tha = theta + j*dtheta;
	    }
	    ymax = (ymax < y) ? y : ymax;
	    sc_draw((int)(xc + ps_x*c_theta),(int)(yc + y));
	 }
	 
	 /* thb = tha; We never actually use thb, just its co/sine */
	 c_theta = c_thetab = cos(tha); /* use c_theta for tha */
	 s_theta = s_thetab = sin(tha);
	 xb1 = xa1 = ps_x*c_theta;
	 yb1 = ya1 = ps_y*s_theta;
	 INC_THA;
	 DEC_THB;
	 xa2 = ps_x*c_theta;
	 ya2 = ps_y*s_theta;
	 xb2 = ps_x*c_thetab;
	 yb2 = ps_y*s_thetab;
	 if(abs(ya2 - ya1) < ldef2) {
	    INC_THA;
	    xa1 = xa2;
	    ya1 = ya2;
	    xa2 = ps_x*c_theta;
	    ya2 = ps_y*s_theta;
	 }
	 if(abs(yb2 - yb1) < ldef2) {
	    DEC_THB;
	    xb1 = xb2;
	    yb1 = yb2;
	    xb2 = ps_x*c_thetab;
	    yb2 = ps_y*s_thetab;
	 }
	 for(j = ymin;j <= ymax;j += ldef2) {
	    if(zig) {
	       zig = 0;			/* i.e. zag */
	       while(ya2 < j) {
		  INC_THA;
		  xx = ps_x*c_theta;
		  yy = ps_y*s_theta;
		  if(1 || abs(yy - ya2) > ldef2) {
		     xa1 = xa2;
		     ya1 = ya2;
		     xa2 = xx;
		     ya2 = yy;
		  }
	       }
	       xa = xp0 + xa1 + ((long)xa2-xa1)*(j-ya1)/(ya2-ya1);
	       sc_draw(xa,yp0 + j);	/* right hand edge */
	    } else {
	       zig = 1;
	       while(yb2 < j) {
		  DEC_THB;
		  xx = ps_x*c_thetab;
		  yy = ps_y*s_thetab;
		  if(1 || abs(yy - yb2) > ldef2) {
		     xb1 = xb2;
		     yb1 = yb2;
		     xb2 = xx;
		     yb2 = yy;
		  }
	       }
	       xb = xp0 + xb1 + ((long) xb2-xb1)*(j-yb1)/(yb2-yb1);
	       sc_draw(xb,yp0 + j);	/* left hand edge */
	    }
	 }
      }
   }
   xp = xp0; yp = yp0;
}
