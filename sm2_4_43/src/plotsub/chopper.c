#include "copyright.h"
#include <math.h>
#include <stdio.h>
#include "options.h"
#include "sm.h"
#include "devices.h"
#include "defs.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm_declare.h"

extern int offscreen;			/* is the current point offscreen? */
static float totdist = 0;	/* total distance drawn, so ltype cts at pts */

#define NSTROKE   4			/* number of strokes in a pattern */
#define DOT_ON    50
#define DOT_OFF   100
#define SHORT_ON  320
#define SHORT_OFF 320
#define LONG_ON   800
#define LONG_OFF  320

#define EPS 1e-2
/*
 * chop up a line; negative ltypes mean that |ltype| is supported in hardware.
 * Note that ltype 10/11 are used for erasing lines
 */
void
chopper(ixa,iya,ixb,iyb)
int ixa,iya,ixb,iyb;
{
   float dist, dtogo, frac,
   	 step;				/* step when drawing thick lines */
   int d,
       i, j, istart,
       index,
       lw1, lw2,
       ix1, ix2, iy1, iy2,
       onofflen,
       start,
       xc, yc, xd, yd;
   static int strokes[][NSTROKE] = {
      { DOT_ON, DOT_OFF, DOT_ON, DOT_OFF },
      { SHORT_ON, SHORT_OFF, SHORT_ON, SHORT_OFF },
      { LONG_ON, LONG_OFF, LONG_ON, LONG_OFF },
      { SHORT_ON, DOT_OFF, DOT_ON, DOT_OFF },
      { LONG_ON, DOT_OFF, DOT_ON, DOT_OFF },
      { LONG_ON, LONG_OFF, SHORT_ON, SHORT_OFF },
   };
   int onoff[NSTROKE];			/* the pattern in use */
 
   dist = 0.0;
   if(lltype == 0) {
      index = istart = start = 0;	/* pacify compiler about
					   set-before-used */
   } else {
      
      if((index = abs(lltype) - 1) >= sizeof(strokes)/sizeof(strokes[0])) {
	 msg_1d("Illegal ltype: %d\n",lltype); /* we can't get here, as this
						  is checked in ltype(),
						  but we may as well check */
	 return;
      }
      onofflen = 0;
      for(i = 0; i < NSTROKE; i++) {
	 onoff[i] = lltype_expand*strokes[index][i];
	 onofflen += onoff[i];
      }
      
      start = (int)totdist % onofflen;
      for(istart = 0;istart < 4;istart++) {
	 if(start < onoff[istart]) break;
         start -= onoff[istart];
      }
   }

   if(abs(iyb-iya) > abs(ixb-ixa) ) {	/* thick lines drawn side by side */
      step = MIN((float)(ldef/2), 1.0/xscreen_to_pix);
   } else {				/* thick lines one above the other */
      step = MIN((float)(ldef/2), 1.0/yscreen_to_pix);
   }
 
   if(llweight > 1) {
      lw1 = -ldef*(llweight-1)/(2*step);
      lw2 = ldef*llweight/(2*step);
   } else {
      lw1 = lw2 = 1;
   }

   for(j = lw1;j <= lw2;j++) {
      if(iya == iyb && ixa == ixb) { 
         ix1 = MAX( MIN( ixa+(int)(lw1*ldef/2), SCREEN_SIZE), 0);
         ix2 = MAX( MIN( ixb+(int)(lw2*ldef/2), SCREEN_SIZE), 0);
         iy1 = MAX( MIN( NINT(iya+j*step), SCREEN_SIZE), 0);
         iy2 = MAX( MIN( NINT(iyb+j*step), SCREEN_SIZE), 0);
      } else if (abs(iyb-iya) > abs(ixb-ixa) ) {
         ix1 = MAX( MIN(NINT(ixa+j*step), SCREEN_SIZE), 0);
         iy1 = iya;
         ix2 = MAX( MIN(NINT(ixb+j*step), SCREEN_SIZE), 0);
         iy2 = iyb;
      } else {
         ix1 = ixa;
         iy1 = MAX( MIN(NINT(iya+j*step), SCREEN_SIZE), 0);
         ix2 = ixb;
         iy2 = MAX( MIN(NINT(iyb+j*step), SCREEN_SIZE), 0);
      }
 
      if (lltype == 0) {
	 if(offscreen || iy2 != xp || iy1 != yp) {
	    LINE(ix1,iy1,ix2,iy2);
	    offscreen = 0;
	 } else {
	    DRAW(ix2,iy2);
	 }
      } else {
         dist = hypot( (double)(ix2-ix1), (double)(iy2-iy1) );
         if(dist == 0) {
	    if(istart%2 == 0) LINE(ix1,iy1,ix2,iy2);
         } else {
            i = istart;
 	    d = onoff[i++ % 4] - start;
            dtogo = dist;
 	    if(istart%2 == 0) {			/* start with black segment */
               xc = ix1;
               yc = iy1;
 	    } else {				/* start with blank segment */
	       if(d > dtogo || dist < EPS) {
		  continue;
	       }
 	       frac = d/dtogo;
               dtogo -= d;
               xc = ix1 + frac*(ix2 - ix1);
               yc = iy1 + frac*(iy2 - iy1);
               d = onoff[i++%4]; 
 	    }
 
            for(;;) {
	       if(d > dtogo) {
 	       	  d = dtogo;
 	       }
	       if(dtogo < EPS) {
		  if(dtogo > 0)
		    LINE(xc,yc,ix2,iy2);
		  break;
	       }
 	       frac = d/dtogo;
               xd = xc + frac*(ix2 - xc) + 0.5;
               yd = yc + frac*(iy2 - yc) + 0.5;
 
 	       LINE(xc,yc,xd,yd);
 
	       dtogo -= d;
               d = onoff[i++ % 4];
	       if(dtogo <= EPS) {
	       	  break;
	       }
 	       frac = d/dtogo;
               xc = xd + frac*(ix2 - xd) + 0.5;
               yc = yd + frac*(iy2 - yd) + 0.5;
 
	       dtogo -= d;
               d = onoff[i++ % 4]; 
            }
         }         
      } 
   }
   totdist += dist;
}

/*****************************************************************************/
/*
 * reset the line chopper
 */
void
reset_chopper()
{
   totdist = 0;
}
