#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "sm.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm_declare.h"
/*
 * stg_reloc -- Output a device move instruction to move to the position (x,y)
 * in SCREEN coordinates.
 *
 * stg_dot  -- draw a single pixel dot at (x,y)
 */
void
stg_reloc(x, y)
int x, y;			/* destination */
{
   int dx = 0, dy = 0;			/* offset from previous point;
					   initialise to make gcc happy */
   int len;				/* length of a string */
   int relative = 0;			/* use a relative move? */

   x = (int)(x*xscreen_to_pix + 0.5);
   y = (int)(y*yscreen_to_pix + 0.5);

   if(x == g_xc && y == g_yc) {
      return;
   }

   if(g_xc >= 0 &&
      (*g_sg->sg_startmove_rel != '\0' || *g_sg->sg_endmove_rel != '\0')) {
      int adx, ady;			/* |dx|, |dy| */
      dx = x - g_xc;
      adx = (g_xc < x) ? dx : -dx;
      if(adx < x) {
	 dy = y - g_yc;
	 ady = (g_yc < y) ? dy : -dy;
	 if(ady < y) {			/* use a relative move */
	    relative = 1;
	 }
      }
   }

   if(relative) {
      len = strlen(g_sg->sg_startmove_rel);
      if(len > 0) {
	 ttyputs(g_out,g_tty,g_sg->sg_startmove_rel,len,1);
      }
      
      g_reg[1].i = dx;
      g_reg[2].i = dy;
      g_reg[E_IOP].i = 0;
      if(stg_encode(g_xy,g_mem,g_reg) == OK) {
	 ttwrite(g_out,g_mem,g_reg[E_IOP].i);
      } else {
	 msg_1d("Encode failed: move rel %d", dx);
	 msg_1d(" %d", dy);
      }

      len = strlen(g_sg->sg_endmove_rel);
      if(len > 0) {
	 ttyputs(g_out, g_tty, g_sg->sg_endmove_rel, len, 1);
      }
   } else {
      len = strlen(g_sg->sg_startmove);
      if(len > 0) {
	 ttyputs(g_out,g_tty,g_sg->sg_startmove,len,1);
      }
      
      g_reg[1].i = x;
      g_reg[2].i = y;
      g_reg[E_IOP].i = 0;
      stg_encode(g_xy,g_mem,g_reg);
      ttwrite(g_out,g_mem,g_reg[E_IOP].i);

      len = strlen(g_sg->sg_endmove);
      if(len > 0) {
	 ttyputs(g_out, g_tty, g_sg->sg_endmove, len, 1);
      }
   }

   g_xc = x;
   g_yc = y;
}

/***************************************************************/
/*
 * Draw a sm_dot. Note that we have just moved to the current sm_location,
 * so if startmark is null we needn't move there again
 */
int
stg_dot(x, y)
int x, y;			/* location */
{
   if(g_sg->sg_startmark[0] == '\0') {
      if(g_sg->sg_endmark[0] == '\0') {
	 return(-1);
      }
   } else {
      ttyputs(g_out,g_tty,g_sg->sg_startmark,strlen(g_sg->sg_startmark),1);
      g_reg[1].i = x*xscreen_to_pix + 0.5; /* convert SCREEN to device coords*/
      g_reg[2].i = y*yscreen_to_pix + 0.5;
      g_reg[E_IOP].i = 0;
      stg_encode(g_xy,g_mem,g_reg);
      ttwrite(g_out,g_mem,g_reg[E_IOP].i);
   }

   ttyputs(g_out,g_tty,g_sg->sg_endmark,strlen(g_sg->sg_endmark),1);
   return(0);
}
