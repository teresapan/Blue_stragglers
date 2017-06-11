#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm.h"
#include "sm_declare.h"
/*
 * stg_char -- Draw a text string.  The string is drawn at the position (X,Y)
 */
int
stg_char(text,x,y)
char text[];			/* text string */
int x,y;			/* position to write string */
{
   if(text == NULL) {		/* can we write hardware characters? */
      if(g_sg->sg_starttext[0] == '\0') {
	 return(-1);
      } else {
	 stg_char_size(0,(eexpand == 1.0 && aangle == 0.0));
	 return(0);
      }
   }

   g_reg[1].i = x*xscreen_to_pix + 0.5;
   g_reg[2].i = y*yscreen_to_pix + 0.5;
   g_reg[E_IOP].i = 0;
   if(stg_encode(g_sg->sg_starttext,g_mem,g_reg) == OK) {
      ttwrite(g_out, g_mem, g_reg[E_IOP].i);
   }
/*
 * Draw the characters.
 */
   ttwrite(g_out,text,strlen(text));
/*
 * Tidy up
 */
   g_reg[E_IOP].i = 0;
   if (stg_encode (g_sg->sg_endtext, g_mem, g_reg) == OK) {
      ttwrite(g_out, g_mem, g_reg[E_IOP].i);
   }
/*
 * invalidate plot position, as we don't know where we are
 */
   g_xc = g_yc = -1;
   return(0);
}
