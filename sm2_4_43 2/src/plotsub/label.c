#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "sm.h"
#include "defs.h"
#include "sm_declare.h"

void
sm_label( str )
char *str;
{
   int lsave;

   lsave = lltype;
   if(lltype != -10) sm_ltype( 0 );
   draw_string(str);
   sm_ltype(lsave);
}

/*
 * Puts a label according to loc:
 *	7  8  9
 *	4  5  6
 *	1  2  3
 * Left, center or right in x and y
 *
 * If loc == 0 don't actually draw anything (but set dimensions)
 */
void
sm_putlabel( loc, str)
int loc;
char *str;
{
   int lsave;
   float angrad,
    	 dx, dy,
    	 slength,
   	 sdepth,sheight,
         xasp, yasp,
    	 xsign, ysign,
    	 xstart, ystart;

   lsave = lltype;
   if(lltype != -10) sm_ltype( 0 );
   string_size(str,&slength,&sdepth,&sheight);
   if(loc != 0) {
      xsign = ((loc - 1)% 3) - 1;
      ysign = (loc - 1)/3 - 1;
      dx = (xsign - 1)*slength/2;
      if(ysign == 1) {
	 dy = sdepth;
      } else if(ysign == 0) {
	 dy = -(sheight - sdepth)/2;
      } else {
	 dy = -sheight;
      }
      
      if(aspect > 1.0) {
	 xasp = aspect;
	 yasp = 1.0;
      } else if (aspect < 1.0) {
	 xasp = 1.0;
	 yasp = aspect;
      } else {
	 xasp = 1.0;
	 yasp = 1.0;
      }
      
      angrad = aangle*PI/180.;
      dx /= sqrt(pow(xasp*cos(angrad),2.0) + pow(sin(angrad)/yasp,2.0));
      dy /= sqrt(pow(xasp*sin(angrad),2.0) + pow(cos(angrad)/yasp,2.0));
      xstart = xp + xasp*(dx*cos(angrad) - dy*sin(angrad));
      ystart = yp + (dx*sin(angrad) + dy*cos(angrad))/yasp;

      sm_grelocate(xstart,ystart);
      draw_string(str);
   }
   sm_ltype(lsave);
}

