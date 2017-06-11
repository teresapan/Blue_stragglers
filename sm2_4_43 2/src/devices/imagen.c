#include "copyright.h"
#include "options.h"
#ifdef IMAGEN
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "sm.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm_declare.h"

#define MAX 127
/*
 * Imagen Op codes
 */
#define CREATE_PATH 230
#define DRAW_IN_BLACK 15		/* used with DRAW_PATH */
#define DRAW_PATH 234
#define ENDPAGE 219
#define FILL_PATH 233
#define E_O_F 255
#define SET_PEN 232

static char *obp,
            obuff[3 + 4*MAX + 2];	/* buffer defining a segment */
static int p1, p2,			/* current pair of endpoints */
       old;				/* last point drawn */
     
static union {
   char c[4];
   short s[2];
   int i;
} tmp;

static union {
   char c[2];
   short s;
} num;					/* number of segments in obuff */

static void add(), do_path(), sadd();

int
im_setup(arg)
char *arg;
{
   old = -1;
   num.s = 0;
   obp = obuff;
   *obp++ = CREATE_PATH;
   obp += 2;				/* leave room for number of segments */
/*
 * now set some variables for SM
 */
   ldef = 10;
   return(0);
}

void
im_line(x1, y1, x2, y2)
int x1, y1, x2, y2;
{
   x1 = (int) ((float)x1 * xscreen_to_pix);
   y1 = (int) ((float)y1 * yscreen_to_pix);
   x2 = (int) ((float)x2 * xscreen_to_pix);
   y2 = (int) ((float)y2 * yscreen_to_pix);
/*
 * encode a (x,y) pair into one int.  It's ugly, but it makes life easy
 */
   tmp.s[0] = (short) x1;
   tmp.s[1] = (short) y1;
   p1 = tmp.i;
   tmp.s[0] = (short) x2;
   tmp.s[1] = (short) y2;
   p2 = tmp.i;

   if(p1 == old) {
      add(p2);
   } else if(p2 == old) {
      add(p1);
   } else {
      do_path(DRAW_PATH);
      add(p1); add(p2);
   }
}

int
im_lweight(lw)
double lw;
{
   unsigned char setc = lw + 0.5;
   unsigned char set_pen = SET_PEN;

   do_path(DRAW_PATH);
   ttwrite(g_out,(char *)&set_pen,1);
   ttwrite(g_out,(char *)&setc,1);
   return(0);
}

/****************************************************************************/
/*
 * add a point to the current path
 */
static void
add(p)
int p;
{
   tmp.i = old = p;
   
#ifdef vax
   *obp++ = tmp.c[1];
   *obp++ = tmp.c[0];
   *obp++ = tmp.c[3];
   *obp++ = tmp.c[2];
#else
   *obp++ = tmp.c[0];
   *obp++ = tmp.c[1];
   *obp++ = tmp.c[2];
   *obp++ = tmp.c[3];
#endif  
   
   if(++num.s >= MAX - 2) {
      do_path(DRAW_PATH);
      sadd(old);
   }
}

/****************************************************************************/
/*
 * Like add(), but never call do_path()
 */
static void
sadd(p)
int p;
{
   tmp.i = old = p;
   
#ifdef vax
   *obp++ = tmp.c[1];
   *obp++ = tmp.c[0];
   *obp++ = tmp.c[3];
   *obp++ = tmp.c[2];
#else
   *obp++ = tmp.c[0];
   *obp++ = tmp.c[1];
   *obp++ = tmp.c[2];
   *obp++ = tmp.c[3];
#endif  
   ++num.s;
}

static void
do_path(op)
int op;
{
   if(num.s == 0) {
      return;
   }
/*
 * fill in number of segments in obuff[1] & obuff[2]
 */
#ifdef vax
   obuff[1] = num.c[1];
   obuff[2] = num.c[0];
#else
   obuff[1] = num.c[0];
   obuff[2] = num.c[1];
#endif     
   *obp++ = op;
   *obp++ = DRAW_IN_BLACK;
   ttwrite(g_out, obuff, 3 + 4*num.s + 2);
   obp = obuff;
   *obp++ = CREATE_PATH;
   obp += 2;				/* leave room for number of segments */
   num.s = 0;
}

void
im_draw(x, y)
int x, y;
{
   im_line(xp, yp, x, y);
}

/*
 * Begin new page
 */
void
im_page()
{
   unsigned char endpage = ENDPAGE;
   
   do_path(DRAW_PATH);
   ttwrite(g_out,(char *)&endpage,1);
}

int
im_fill_pt(n)
int n;
{
   float dtheta, theta;
   static float old_ang;	/* old values of angle */
   int i,
       xpsize,			/* scale for points */
       ypsize;
   static int nvertex = -1,         /* number of vertices used last time */
	      old_xp,old_yp;	/* old values of xpsize, ypsize */
   struct point {
      int x,y;
   };
   static struct point vlist0[MAX_POLYGON + 1]; /* corners of point at (0,0) */

   if(n < 2) {
      im_line(xp,yp,xp,yp);
      return(0);
   }

   do_path(DRAW_PATH);		/* finish previous path */

   dtheta = 2*PI/n;
   xpsize = ypsize = PDEF*eexpand;
   if(aspect > 1.0) {                   /* allow for aspect ratio of screen */
      ypsize /= aspect;
   } else if(aspect < 1.0) {
      xpsize *= aspect;
   }

   if(n != nvertex || aangle != old_ang ||
      					xpsize != old_xp || ypsize != old_yp) {
      if(n > MAX_POLYGON) nvertex = MAX_POLYGON;
      else nvertex = n;

      theta = 3*PI/2 + dtheta/2 + aangle*PI/180;

      old_ang = aangle;
      old_xp = xpsize;
      old_yp = ypsize;

      for(i = 0;i < nvertex;i++) {
	 vlist0[i].x = xpsize*cos(theta) + 0.5;
	 vlist0[i].y = ypsize*sin(theta) + 0.5;
	 theta += dtheta;
      }
      vlist0[i].x = vlist0[0].x;
      vlist0[i].y = vlist0[0].y;
   }

   for(i = 0;i < nvertex;i++) {
      im_line(vlist0[i].x + xp,vlist0[i].y + yp,
	      vlist0[i+1].x + xp,vlist0[i+1].y + yp);
   }
   do_path(FILL_PATH);
   
   return(0);
}

void
im_close()
{
   unsigned char endpage = ENDPAGE;
   unsigned char eof = E_O_F;

   do_path(DRAW_PATH);
   ttwrite( g_out, (char *)&endpage, 1 );
   ttwrite( g_out, (char *)&eof, 1 );
}
#endif    
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif
