#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm.h"
#include "sm_declare.h"
/*
 * stg_line -- move to (x1,y1) and draw a line to (x2,y2)
 * stg_draw -- draw a line to (x,y) from current position
 * stg_ltype -- set the linetype
 * stg_ctype -- set the colour of the line
 * stg_set_ctype -- set the colour table for lines
 */
 
extern int offscreen;			/* is the current point offscreen? */

void
stg_line(x1,y1,x2,y2)
int x1,y1,			/* starting point */
    x2,y2;			/* finishing point */
{
/*
 * Transform the first point from SCREEN coords to device coords and
 * move to the transformed point, then draw the desired line
 */
   stg_reloc(x1, y1);
   stg_draw(x2, y2);
}

/**********************************************************/

void
stg_draw(x1,y1)
int x1,y1;			/* finishing point */
{
   int dx = 0, dy = 0;			/* offset from previous point;
					   initialise to make gcc happy */
   int relative = 0;			/* use a relative draw? */
/*
 * Transform point into the device window, leaving the coords
 * in registers R1 and R2 for the encoder.
 */
   x1 = (int)(x1*xscreen_to_pix + 0.5);
   y1 = (int)(y1*yscreen_to_pix + 0.5);

   if(g_xc >= 0 &&
      (*g_sg->sg_enddraw_rel != '\0' || *g_sg->sg_startdraw_rel != '\0')) {
      int adx, ady;			/* |dx|, |dy| */
      dx = x1 - g_xc;
      adx = (g_xc < x1) ? dx : -dx;
      if(adx < x1) {
	 dy = y1 - g_yc;
	 ady = (g_yc < y1) ? dy : -dy;
	 if(ady < y1) {			/* use a relative move */
	    relative = 1;
	 }
      }
   }
/*
 * Draw the line, first the startline commmand, then draw it. If it is
 * empty, don't waste the function overhead
 */
   if(relative) {
      if(*g_sg->sg_startdraw_rel != '\0') {
	 g_reg[E_IOP].i = 0;
	 if(stg_encode (g_sg->sg_startdraw_rel, g_mem, g_reg) == OK) {
	    ttwrite(g_out, g_mem, g_reg[E_IOP].i);
	 }
      }
      g_reg[E_IOP].i = 0;

      g_reg[1].i = dx;
      g_reg[2].i = dy;
/*
 * Encode the point
 */
      if(stg_encode (g_xy, g_mem, g_reg) == OK) {
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
	 g_reg[E_IOP].i = 0;
      } else {
	 msg_1d("Encode failed: draw rel %d", dx);
	 msg_1d(" %d", dy);
      }
/*
 * Output end draw sequence, if it exists
 */
      if(*g_sg->sg_enddraw_rel != '\0') {
	 g_reg[E_IOP].i = 0;
	 if(stg_encode (g_sg->sg_enddraw_rel, g_mem, g_reg) == OK) {
	    ttwrite(g_out, g_mem, g_reg[E_IOP].i);
	 }
      }
   } else {
      if(*g_sg->sg_startdraw != '\0') {
	 g_reg[E_IOP].i = 0;
	 if(stg_encode (g_sg->sg_startdraw, g_mem, g_reg) == OK) {
	    ttwrite(g_out, g_mem, g_reg[E_IOP].i);
	 }
      }
      g_reg[E_IOP].i = 0;
      
      g_reg[1].i = x1;
      g_reg[2].i = y1;
/*
 * Encode the point
 */
      if(stg_encode (g_xy, g_mem, g_reg) == OK) {
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
	 g_reg[E_IOP].i = 0;
      }
/*
 * Output end draw sequence, if it exists
 */
      if(*g_sg->sg_enddraw != '\0') {
	 g_reg[E_IOP].i = 0;
	 if(stg_encode (g_sg->sg_enddraw, g_mem, g_reg) == OK) {
	    ttwrite(g_out, g_mem, g_reg[E_IOP].i);
	 }
      }
   }

   g_xc = x1;
   g_yc = y1;
}

/**********************************************************/
/*
 * Set line type. This can confuse the terminal as to its mode (e.g. on a
 * tek4010 lookalike), so set offscreen to ensure that we generate a
 * reloc before next drawing a line.
 */
int
stg_ltype(lt)
int lt;
{
   char cltype,				/* line type as a char */
	types[11];			/* string of possible types */
   int i;

   if(ttygetb(g_tty,"ML")) {		/* can set linetype */
      offscreen = 1;			/* force a reloc */
      g_xc = -1;			/* don't lose update to rounding */
      (void)ttygets(g_tty,"lt",types,11); /* possible line types */
      cltype = lt + '0';
      for(i = 0;i < 11 && types[i] != '\0';i++) {	/* look for ltype */
	 if(types[i] == cltype) {
	    stg_ctrl1("ML", lt);
	    return(0);
	 }
      }
      stg_ctrl1("ML",0);		/* solid lines for software ltypes */
   }
   return(-1);
}

/**********************************************************/
/*
 * Try to set a line weight
 */
int
stg_lweight(lw)
double lw;				/* desired weight */
{
   if(ttygetb(g_tty,"LW")) {		/* can set lineweight */
      stg_fctrl1("LW",lw);
      return(0);
   } else {
      return(-1);
   }
}

/**********************************************************/
/*
 * Set a colour
 */
void
stg_ctype(r,g,b)
int r,g,b;				/* red, green, and blue in [0,255] */
{
   (void)stg_ctrl3("CT",r,g,b);
}


/**********************************************************/
/*
 * Set a set of line colours
 */
#define SIZE 80

int
stg_set_ctype(colors,ncol)
COLOR *colors;
int ncol;
{
   char buff[SIZE];			/* encoder commands for CS/CO */
   int i;
   
   if(ttygets(g_tty,"CS",buff,SIZE) == 0) {	/* no Colour Start string  */
      return(-1);
   }
   if(colors == NULL) {		/* just asking if CS exists */
      return(0);
   }
   g_reg[E_IOP].i = 0;
   g_reg[1].i = ncol;
   if(stg_encode(buff,g_mem,g_reg) == OK) {
      g_mem[g_reg[E_IOP].i] = '\0';
      ttyputs(g_out,g_tty,g_mem,g_reg[E_IOP].i,1);
   }

   if(ttygets(g_tty,"CO",buff,SIZE) == 0) {	/* no COlour string  */
      return(0);
   }
   for(i = 0;i < ncol;i++) {
      g_reg[E_IOP].i = 0;
      g_reg[1].i = colors[i].red;
      g_reg[2].i = colors[i].green;
      g_reg[3].i = colors[i].blue;
      if(stg_encode(buff,g_mem,g_reg) == OK) {
	 g_mem[g_reg[E_IOP].i] = '\0';
	 ttyputs(g_out,g_tty,g_mem,g_reg[E_IOP].i,1);
      }
   }
   return(0);
}


