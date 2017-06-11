/*
 * Code to draw surfaces with hidden line removal. The algorithms
 * were invented by Gabor Toth (and implemented as SM macros!), and
 * converted to C by rhl.
 */
#include <stdio.h>
#include <math.h>
#include "copyright.h"
#include "options.h"
#include "sm.h"
#include "image.h"
#include "sm_declare.h"

#define EPS 1e-6			/* fuzz for floating comparisons */

typedef struct point {			/* a point on the the margin */
   struct point *next;			/* next point */
   float x,z,u;				/* (x,z) coordinates and u parameter */
} POINT;                                /* of a point */  
typedef struct {			/* the margin of the hidden region */
   POINT *first, *last;			/* first and last points */
} MARGIN;

static MARGIN top = { NULL, NULL};
static MARGIN bottom = { NULL, NULL};
static void add_left P_ARGS(( MARGIN *, double, double ));
static void add_right P_ARGS(( MARGIN *, double, double, int ));
static void free_margin P_ARGS(( MARGIN * ));
static void find_limits P_ARGS(( REAL *, int,  float *, float * ));
static void top2
  P_ARGS((MARGIN *, double, double, double, double, POINT *, POINT *, int ));
static void top3
  P_ARGS((MARGIN *,double, double, double, double, double, double, int ));
static void visible P_ARGS(( MARGIN *, int ));

extern int sm_verbose;			/* how chatty should I be? */
extern int sm_interrupt;			/* did they hit a ^C? */
static int draw_top = 1, draw_bottom = 1;
static float cxx = 1,cxy = 0,cyx = 0,cyy = 1,cyz = 0,czx = 0,czy = 0,czz = 1;
static REAL *new_x, *new_z, *old_x, *old_z;
static int nxz;
static int ifront_x, iback_x, ifront_y, iback_y;
                                     /* front and back indexes in x and y */
static float shift_x,  shift_z;
static float front_x,  front_y,  front_z;
static float back_x,   back_y,   back_z;
static float size_x,   size_y,   size_z;
static REAL backedge_x[4] = { 0, 1, 1, 0}, backedge_z[4] = { 0, 0, 2, 2};
static REAL frontedge_x[4] = { 0, 1, 1, 0}, frontedge_z[4] = { 0, 0, 1, 1};

static int signum_z = 1, signum_u = 1;
static int dix = 1, diy = 1, last_on_segment = 0;
static float fz1 = 0, fz2 = 1;
static double l     = 0.0;		/* distance to the projection point */
static double phi   = 0.5;		/* altitude (radians) */
static double theta = 0.5, theta0 = 0.5;/* azimuth (radians) */
static double z0inv = 0.0;              /* inverse of z0=projected(z=infty) */

static void axial P_ARGS(( REAL *, REAL *, int, double, REAL *, REAL *,int));
static void front_plane P_ARGS(( REAL *, REAL *, int, double, int ));
static void plane P_ARGS(( REAL *, REAL *, int, double, int ));
static void projective P_ARGS((REAL *,REAL *,int, double, REAL *, REAL *,int));
static void (*project) P_ARGS((REAL *,REAL *,int, double, REAL *, REAL*,int)) =
								    projective;

/*****************************************************************************/
/*
 * Returns the monotonic u parameter which always increases along the extreme
 * curve from front to back.
 */
static float 
u_func(x, z)
double x, z;
{
   return(signum_u*x/fabs(1.0 - z*z0inv));
}


/*****************************************************************************/

void
sm_set_viewpoint(t,p,ll)
double t,p,ll;
{
#  define DP 0.5
#  define DT 1.0
   double badp;

   if(t <= -90.0 || t >= 90.0 ) {
      msg("Please choose theta in [-90,90]\n");
      return;
   }
   if (t > 90.0-DT){
      t = 90.0-DT;
      if(sm_verbose > 1) {
         msg_1f("Theta too close to 90; changed to %f\n", t);
      }
   }
   if (t < -90.0+DT){
      t = -90.0+DT;
      if(sm_verbose > 1) {
         msg_1f("Theta too close to -90; changed to %f\n", t);
      }
   }

   if(p <= -180.0 || p >= 180.0 ) {
      msg("Please choose phi in (-180,180)\n");
      return;
   }
   for(badp = -180.0;badp < 181.0;badp += 90.0) {
      if(p > badp-DP && p <= badp) {
         p = badp-DP;
	 if(sm_verbose > 1) {
	    msg_1f("Phi too close to %f; ", badp);
	    msg_1f("changed to %f\n",p);
	 }
         break;
      }
      if(p < badp+DP && p >= badp) {
         p = badp + DP;
	 if(sm_verbose > 1) {
	    msg_1f("Phi too close to %f; ", badp);
	    msg_1f("changed to %f\n",p);
	 }
         break;
      }
   }

   l = ll;
   phi = p*(PI/180);
   theta0 = t*(PI/180);

}

/*****************************************************************************/
/*
 * Draw a wireframe, with hidden-surface removal
 */
void
sm_draw_surface(type,z1,z2,x,nx,y,ny)
int type;				/* 0 no hide, 1 top, 2 bottom, 3 both*/
double z1,z2;				/* Limits for the image values */
REAL *x;				/* Optional vector for x coordinates */
int nx;
REAL *y;				/* Optional vector for y coordinates */
int ny;
{
   int auto_limits = 1;			/* Choose limits automatically? */
   int clip_z;			        /* clip surface to lie in 3d box? */
   REAL edge_x[8],edge_z[8];		/* Edges of the projected image */
   int free_x = 0,free_y = 0;		/* Need I free x and y? */
   int interpolate;			/* Interpolate in image? */
   int i,j;
   IMAGE *image;
   REAL *z, tmp;

   if((image = current_image()) == NULL) {
      msg("No image is defined\n");
      return;
   }
/*
 * Interpret type
 */
   draw_top = (type%10 & 01);
   draw_bottom = (type%10 & 02);
   type /= 10;
   auto_limits = (type%10 == 0) ? 1 : 0;
/* 
 * We use negative l (distance) for axial projection instead of type.
 * type /= 10;
 * project = (type%10 == 1) ? axial : projective;
 */
   project = (l < 0.0) ? axial : projective;

   type /= 10;
   interpolate = (type%10 == 0) ? 1 : 0;

   type /= 10;
   clip_z = (type%10 == 0) ? 1 : 0;
   
   if(x == NULL) {
      nx = image->nx;
      if((x = (REAL *)malloc(nx*sizeof(REAL))) == NULL) {
	 msg("Can't allocate storage for x in draw_surface\n");
	 return;
      }
      free_x = 1;
      for(i = 0;i < nx;i++) x[i] = i;
   }
   if(y == NULL) {
      ny = image->ny;
      if((y = (REAL *)malloc(ny*sizeof(REAL))) == NULL) {
	 msg("Can't allocate storage for y in draw_surface\n");
	 if(free_x) free((char *)x);
	 return;
      }
      free_y = 1;
      for(i = 0;i < ny;i++) y[i] = i;
   }
   if(nx < 2 || ny < 2) {
      msg_1d("You need at least 2 points for a surface plot (nx,ny = %d",nx);
      msg_1d(",%d)\n",ny);
      if(free_x) free((char *)x);
      if(free_y) free((char *)y);
      return;
   }

   fz1 = z1;
   fz2 = z2;
   if(fz2 == fz1) {
      tmp = (fz1 != 0.0)? 0.05*fabs(fz1) : 0.05;
      fz1 -= tmp;
      fz2 += tmp;
   }

/*
 * Some temporary space
 */
   if((z = (REAL *)malloc(5*nx*sizeof(REAL))) == NULL) {
      msg("Can't allocate storage for z in draw_surface\n");
      if(free_x) free((char *)x);
      if(free_y) free((char *)y);
      return;
   }
   new_x = z + nx; new_z = new_x + nx;
   old_x = new_z + nx; old_z = old_x + nx;
/*
 * Bookkeeping
 */
/*
 * If x (or y) is in decreasing order, it needs to be reordered. For 
 * interpolation the array elements are swapped around the middle element,
 * for no interpolation the coordinates are mirrored to the middle value.
 */
   if(x[nx-1] < x[0]){
      if(interpolate){
         for(i = 0;i < (nx - 1)/2;i++) {
	    tmp = x[i]; x[i] = x[nx-1-i]; x[nx-1-i] = tmp;
	 }
      } else {
         tmp = x[0] + x[nx-1];
         for(i = 0;i < nx;i++) {
	    x[i] = tmp-x[i];
	 }
      }
   }
   if(y[ny-1]<y[0]){
      if(interpolate) {
          for(i = 0;i < (ny - 1)/2;i++) {
	     tmp = y[i]; y[i] = y[ny-1-i]; y[ny-1-i] = tmp;
	  }
      } else {
          tmp = y[0] + y[ny-1];
	  for(i = 0;i < ny;i++) {
	     y[i] = tmp - y[i];
	  }
      }
   }

   signum_z = ( fz2 > fz1 ) ? 1 : -1;
   theta = theta0*signum_z;

   z0inv = (l > 0) ? -tan(theta)/l : 0.0;

   if( phi <= 0.0 ) {
      ifront_x = nx - 1;
      iback_x = 0;
      dix = -1;
   } else {
      ifront_x = 0;
      iback_x = nx - 1;
      dix = 1;
   }
   back_x  = x[iback_x];
   front_x = x[ifront_x];

   if( (fabs(phi)-PI/2.0) <= 0.0 ) {
      ifront_y = 0;
      iback_y = ny - 1;
      diy = 1;
   } else {
      ifront_y = ny - 1;
      iback_y = 0;
      diy = -1;
   }
   back_y  = y[iback_y];
   front_y = y[ifront_y];

   if( theta0 < 0.0 ) {
      back_z  = fz2;
      front_z = fz1;
   } else {
      back_z  = fz1;
      front_z = fz2;
   }

   size_x = fabs(back_x - front_x);
   size_y = fabs(back_y - front_y);
   size_z = fabs(back_z - front_z);

   if(project == projective) {
      cxx = cos(phi);
      cxy = -sin(phi);
      czx = sin(phi)*sin(theta);
      czy = cos(phi)*sin(theta);
      czz = cos(theta);
      if(l > 0) {
	 cyx = sin(phi)*cos(theta);
	 cyy = cos(phi)*cos(theta);
	 cyz = -sin(theta);
      }
   } else if(project == axial) {
/*
 * If we take axial projection from the back, left and right should be swapped 
 * on the screen, accordingly the shift_x coefficient changes sign 
 */
      shift_x = -diy* tan(phi)/size_y*size_x;
      shift_z = tan(theta)/size_y*size_z;
   }

   nxz = nx;
/*
 * Find the limits of the cube enclosing the surface
 */
   edge_x[0] = front_x;	  edge_z[0] = fz1;
   edge_x[1] = back_x;    edge_z[1] = fz1;
   edge_x[2] = front_x;   edge_z[2] = fz2;
   edge_x[3] = back_x;    edge_z[3] = fz2;
   (*project)(edge_x, edge_z, 4, front_y, frontedge_x, frontedge_z, 1);
   (*project)(edge_x, edge_z, 4, back_y,  backedge_x,  backedge_z, 1);
   
   for(i = 0;i < 4;i++) {
      edge_x[i] = frontedge_x[i];
      edge_x[i + 4] = backedge_x[i];
      edge_z[i] = frontedge_z[i];
      edge_z[i + 4] = backedge_z[i];
   }


/* 
 * If back_x projects to smaller x value then front_x the u parameter needs 
 * to be fixed by signum_u to preserve its monotonically GROWING nature
 */ 

   signum_u = ( edge_x[1] > edge_x[0] ) ? 1: -1;

   if(auto_limits) {
      float xmin,xmax,zmin,zmax;

      find_limits(edge_x,8,&xmin,&xmax);
      find_limits(edge_z,8,&zmin,&zmax);

/* 
 * If we take axial projection from the back, left and right should be swapped 
 * on the screen. Top and bottom are swapped if the limits fz1 > fz2. 
 */

      if( project == axial && fabs(phi) > PI/2.0 ){
        tmp = xmin; 
	xmin = xmax;
	xmax = tmp;
      }

      if( signum_z == -1 ){
        tmp = zmin; 
	zmin = zmax;
	zmax = tmp;
      }

      sm_limits(xmin,xmax,zmin,zmax);
   }
/*
 * Draw the surface
 */
   for(j = ifront_y;!sm_interrupt && j != iback_y + diy;j += diy) {
      if(interpolate) {
	 for(i = 0;i < nxz;i++) {
	    z[i] = image_val(x[i],y[j]);
	 }
      } else {
	 for(i = 0;i < nxz;i++) {
	    z[i] = image_val((float)i,(float)j);
	 }
      }
      if(j == ifront_y) {
	 front_plane(x,z,nxz,y[ifront_y],clip_z);
      } else {
	 plane(x,z,nxz,y[j],clip_z);
      }
   }

   if(free_x) free((char *)x);
   if(free_y) free((char *)y);
   free_margin(&top);
   free_margin(&bottom);
}


/*****************************************************************************/
/*
 * Draw the front plane
 */
static void
front_plane(x,z,n,y0,clip_z)
REAL *x;				/* x values */
REAL *z;				/* corresponding z values */
int n;					/* dimension of x and z */
double y0;				/* y value of plane */
int clip_z;				/* clip surface to lie in 3d box? */
{
   int i;

   (*project)(x,z,n,y0,new_x,new_z, clip_z);
   if(draw_top) {
      for(i = ifront_x;i != iback_x+dix;i += dix) {
	 add_right( &top, new_x[i], new_z[i], 0 );
      }
   }
   if(draw_bottom) {
      for(i = ifront_x;i != iback_x + dix;i += dix) {
	 add_right(&bottom, new_x[i], new_z[i], 0 );
      }
   }
   sm_conn(new_x,new_z,n);
}

static void 
plane(x,z,n,y,clip_z)
REAL *x;				/* x values */
REAL *z;				/* corresponding z values */
int n;					/* dimension of x and z */
double y;				/* y value of plane */
int clip_z;				/* clip surface to lie in 3d box? */
{
   int i;

   {
      REAL *tmp;
      tmp = old_x; old_x = new_x; new_x = tmp;
      tmp = old_z; old_z = new_z; new_z = tmp;
   }
   
   (*project)(x,z,n,y,new_x,new_z, clip_z);
   if(draw_top || draw_bottom) {
      sm_relocate(old_x[ifront_x],old_z[ifront_x]);
      sm_draw(new_x[ifront_x],new_z[ifront_x]);
      if(draw_top) {
	 visible(&top,signum_u*signum_z);
      }
      if(draw_bottom) {
	 visible(&bottom,-signum_u*signum_z);
      }
   } else {
      sm_conn(new_x,new_z,n);
      for(i = 0;i < n;i ++ ) {
	 sm_relocate(old_x[i],old_z[i]);
	 sm_draw(new_x[i],new_z[i]);
      }
   }
}

/*****************************************************************************/
static POINT *pleft0 = NULL;		/* starting guess for pleft */

static void
visible(lim,type)
MARGIN *lim;				/* extremum curve */
int type;				/* 1 for top, -1 for bottom */
{
   int i;

   add_left(lim,new_x[ifront_x],new_z[ifront_x]);
   pleft0 = lim->first;
   for(i = ifront_x; i != iback_x; i += dix){
      top3(lim, new_x[i], new_z[i], new_x[i+dix], new_z[i+dix],
	      old_x[i+dix], old_z[i+dix], type);
   }
}

/*****************************************************************************/
/*
 * Add two segments to the extremal line
 */
static void
top3(lim,leftx,leftz,midx,midz,rightx,rightz,sign)
MARGIN *lim;
double leftx,leftz;
double midx,midz;
double rightx,rightz;
int sign;
{
   POINT *pleft, *hpmid, *pmid, *pright;
   MARGIN new;				/* new segment of margin */
   float tmidz;
   POINT *ptr,*next,*pright_next;
   double leftu, midu, rightu;

   leftu  = u_func( leftx, leftz);
   midu   = u_func( midx, midz);
   rightu = u_func( rightx, rightz);

   for(ptr = pleft0;;ptr = next) {
      next = ptr->next;
      if(next == NULL || next->u > leftu + EPS) {
	 break;
      }
   }
   pleft = ptr;

   for(ptr = pleft;;ptr = next) {
      next = ptr->next;
      if(next == NULL || next->u > midu - EPS) {
	 break;
      }
   }
   hpmid = ptr;
   pmid = next;

   for(ptr = pmid;ptr != NULL;ptr = ptr->next) {
      if(ptr->u > rightu - EPS) {
	 break;
      }
   }
   pright = ptr;

   pleft0 = pleft;

   new.first = new.last = NULL;
   add_left(&new,pleft->x,pleft->z);	/* pleft is now on lim and new */
   last_on_segment = 1;
   
   top2(&new,leftx,leftz,midx,midz,pleft,pmid,sign);

   tmidz = midx*(pmid->z - hpmid->z) - midz*(pmid->x - hpmid->x) - 
           pmid->z*hpmid->x + pmid->x*hpmid->z ;

   if(sign*tmidz <= 0) {
      add_right(&new,midx,midz,1);
   }
   top2(&new,midx,midz,rightx,rightz,hpmid,pright,sign);
   add_right(&new,pright->x,pright->z,1);

/* Let add_right do the drawing !!!!! */
/*
   sm_relocate(new.first->x,new.first->z);
   for(ptr = new.first->next;ptr != NULL;ptr = ptr->next) {
      sm_draw(ptr->x,ptr->z);
   }
*/

/*
 * Replace segment of old margin with this new one, first freeing
 * the superceded points
 */
   if(pright->next == NULL) {		/* pright was at end of margin */
      if(new.first->next == NULL) {
	 msg("This can't happen in top3\n");
      }
      lim->last = new.last;
   }
   pright_next = pright->next;		/* pright will be freed in this loop */
   for(ptr = pleft->next;ptr != pright_next;) {
      next = ptr->next;
      free((char *)ptr);
      ptr = next;
   }
   if(new.first->next != NULL) {	/* some points were added */
      pleft->next = new.first->next;	/* first is a copy of pleft */
      new.last->next = pright_next;
   }
   free((char *)new.first);		/* a copy of pleft */
}

/*****************************************************************************/
/*
 * One segment and pointers to the start and end of the portion of
 * the extreme curve that covers the segment in u
 */
static void
top2(new,x1,z1,x2,z2,p1,p2,sign)
MARGIN *new;
double x1,z1,x2,z2;
POINT *p1,*p2;
int sign;
{

   float cutx;
   float linec,linex,linez;
   float dtpi,dtph;
   float tpxi,tpzi,tpxh,tpzh;
   POINT *ptr;
   POINT *last = p2->next;

   linex = x2 - x1;
   linez = z2 - z1;
   linec = (x2*z1 - x1*z2);

   ptr = p1;
   tpxi = ptr->x; tpzi = ptr->z;
   dtpi = sign*(linez*tpxi - linex*tpzi + linec);
   ptr = ptr->next;
   for(;ptr != last;ptr = ptr->next) {
      tpxh = tpxi; tpzh = tpzi;
      dtph = dtpi;
      tpxi = ptr->x; tpzi = ptr->z;
      dtpi = sign*(linez*tpxi - linex*tpzi + linec);

      if(dtph*dtpi < 0) {
	 cutx = (tpxh*dtpi - tpxi*dtph)/(dtpi - dtph);
	 if( (x1-cutx)*(x2-cutx) <= 0.0 ) {
	    add_right(new,cutx,(tpzh*dtpi - tpzi*dtph)/(dtpi - dtph),1);
	 }
      }
      if(dtpi < 0) {
	 if(ptr->next != last) {
	    add_right(new,tpxi,tpzi,0);
	 }
      }
   }

}

/*****************************************************************************/
/*
 * Draw a 3-d box
 */
static void
axis_3d(a1,a2,big,x1,y1,x2,y2,ilabel,iclock)
double a1,a2;
int big;
double x1,y1,x2,y2;
int ilabel,iclock;
{
}

/*****************************************************************************/
/*
 * Draw a 3-d box
 */
void
box_3d(xlab,ylab)
char *xlab,*ylab;
{
   if(frontedge_z[0] <= backedge_z[0]) {
      axis_3d(front_x,back_x,2,frontedge_x[0],frontedge_z[0],
					    frontedge_x[1],frontedge_z[1],1,0);
      if(frontedge_x[0] <= backedge_x[0]) {
	 axis_3d(front_y,back_y,2,frontedge_x[1],frontedge_z[1],
					      backedge_x[1],backedge_z[1],1,0);
      } else {
	 axis_3d(back_y,front_y,2,backedge_x[0],backedge_z[0],
					    frontedge_x[0],frontedge_z[0],1,0);
      }
   } else {
      axis_3d(front_x,back_x,2,frontedge_x[2],frontedge_z[2],
					    frontedge_x[3],frontedge_z[3],1,1);
      if(frontedge_x[0] <= backedge_x[0]){
	 axis_3d(front_y,back_y,2,frontedge_x[3],frontedge_z[3],
					      backedge_x[3],backedge_z[3],1,1);
      } else {
	 axis_3d(back_y,front_y,2,backedge_x[2],backedge_z[2],
					    frontedge_x[2],frontedge_z[2],1,1);
      }
   }
   if(frontedge_x[0] <= backedge_x[0]) {
      axis_3d(fz1,fz2,2,frontedge_x[0],frontedge_z[0],
					    frontedge_x[2],frontedge_z[2],2,1);
      sm_xlabel(xlab);
   } else {
      axis_3d(fz1,fz2,2,backedge_x[0],backedge_z[0],
					      backedge_x[2],backedge_z[2],2,1);
      sm_xlabel(xlab);
   }
   sm_ylabel(ylab);
}

/*****************************************************************************/
/*
 * Define the different projections
 */
static void
axial(x,z,n,y,x_p,z_p,clip_z)
REAL *x,*z;
int n;					/* dimension of x,z */
double y;
REAL *x_p,*z_p;				/* projected x, z values */
int clip_z;				/* NOTUSED */
{
   int i;
   float dy = fabs( y - front_y );

   for(i = 0;i < n;i++) {
      x_p[i] = x[i] + dy*shift_x;
      z_p[i] = z[i] + dy*shift_z;
   }
}

static void
projective(x,z,n,y,x_p,z_p,clip_z)
REAL *x,*z;
int n;					/* dimension of x,z */
double y;
REAL *x_p,*z_p;				/* projected x, z values */
int clip_z;
{
   int i;
   float shrink;
   float px,py,pz;

   py = (y - front_y)/size_y;
   for(i = 0;i < n;i++) {
      px = (x[i] - front_x)/size_x;
      pz = z[i];
      if(clip_z) {
	 pz = (pz < fz1) ? fz1 : ((pz > fz2) ? fz2 : pz);
      }
      pz = (pz - front_z)/size_z;
      x_p[i] = cxx*px + cxy*py;
      z_p[i] = czx*px + czy*py + czz*pz;
      if(l <= 0) {
	 continue;
      }
      shrink = 1 + (cyx*px + cyy*py + cyz*pz)/l;
      if(shrink <= 0) {			/* point's behind the observer */
	 msg("projective: you cannot get here. Please tell RHL\n");
      } else {
	 x_p[i] /= shrink;
	 z_p[i] /= shrink;
      }
   }
}

/*****************************************************************************/
/*
 * Find suitable limits given a vector x
 */
static void
find_limits(x,n,min,max)
REAL *x;
int n;
float *min,*max;
{
   float diff;
   int i;
   
   *min = 1e30; *max = -1e30;
   for(i = 0;i < n;i++) {
      if(x[i] > *max) {
	 *max = x[i];
      }
      if(x[i] < *min) {
	 *min = x[i];
      }
   }
   if((diff = *max - *min) == 0) {
      if ((diff = *min) == 0) {
         diff = 1.0;
      }
   }
   diff *= 0.05;
   *min -= diff;
   *max += diff;
}
   
/*****************************************************************************/
/*
 * Functions to manipulate the edges of the visible region
 */
static void
add_left(lim,x,z)
MARGIN *lim;
double x,z;
{
   POINT *new;

   if((new = (POINT *)malloc(sizeof(POINT))) == NULL) {
      msg("Can't allocate storage for new in add_left\n");
      return;
   }
   new->x = x; new->z = z; new->u = u_func(x,z);

   new->next = lim->first;
   if(lim->last == NULL) {
      lim->last = new;
   }
   lim->first = new;
}

static void
add_right(lim,x,z,on_segment)
MARGIN *lim;
double x,z;
int on_segment;
{
   POINT *new;
   float u;

   u = u_func(x,z);

   if(lim->last != NULL && u <= lim->last->u) {
     return;
  }

/* 
   This version does not work much better either for phi=0 or +-PI/2, though
   one would expect some improvement by relaxing the condition for add_right.

   if(lim->last != NULL && 
      fabs(x - lim->last->x)<EPS && fabs(z - lim->last->z)<EPS ) {
         return;
   }
*/

   if(on_segment && last_on_segment ){
      sm_relocate(lim->last->x, lim->last->z);
      sm_draw(x,z);
   }
   last_on_segment = on_segment;
       
   if((new = (POINT *)malloc(sizeof(POINT))) == NULL) {
      msg("Can't allocate storage for new in add_right\n");
      return;
   }
   new->x = x; new->z = z; new->u = u;
   new->next = NULL;
   if(lim->first == NULL) {
      lim->first = new;
   } else {
      lim->last->next = new;
   }
   lim->last = new;

}

/*****************************************************************************/
/*
 * Free a margin
 */
static void
free_margin(lim)
MARGIN *lim;
{
   POINT *ptr,*tmp;

   for(ptr = lim->first;ptr != NULL;) {
      tmp = ptr->next;
      free((char *)ptr);
      ptr = tmp;
   }
   lim->first = lim->last = NULL;
}
