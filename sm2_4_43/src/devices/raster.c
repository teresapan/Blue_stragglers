#include "copyright.h"
#include <stdio.h>
#include "options.h"
/*
 * Support a raster device. The vectors to be drawn are written out
 * to the output file.
 *
 * To use device raster, you must also provide a graphcap entry, with at least
 * xr, yr, SY, and OF fields. See the documentation of rasterise for what other
 * capabilities are used to actually rasterise the plot. If the device is
 * specified as DEVICE raster name, `name' is the name looked for in graphcap.
 * We presume that SY will run the SM rasteriser on the OF file.
 * 
 * The format of the output file is the vectors as groups of 4 shorts,
 * x1 y1 x2 y2. End of file is marked by a group of 4 -1's. (This is
 * needed as we may have been forced to erase by seeking to the start
 * of the file)
 */
#include <stdio.h>
#include "devices.h"
#include "sm.h"
#include "vectors.h"
#include "tty.h"
#include "stdgraph.h"		/* globals for filename, scale factors, ... */
#include "raster.h"
#include "sm_declare.h"

void
ras_close()
{
   short pos[4];

   pos[0] = RASTER_EOF;			/* mark end of file. (We need */
   pos[1] = RASTER_EOF;			/* this if we erased by seeking) */
   pos[2] = RASTER_EOF;
   pos[3] = RASTER_EOF;
   ttwrite(g_out,(char *)pos,4*sizeof(short));
}

void
ras_ctype(ind,g,b)
int ind,g,b;
{
   short buf[4];

   buf[0] = RASTER_CTYPE;
   buf[1] = ind;
   buf[2] = 0;
   buf[3] = 0;
   ttwrite(g_out,(char *)buf,4*sizeof(short));
}

int
ras_set_ctype(colors,ncol)
COLOR *colors;
int ncol;
{
   int i;
   union {
      char c[2];
      short s;
   } buf[4];
   
   if(colors == NULL) {			/* Do I exist? */
      return(0);
   }

   buf[0].s = RASTER_SET_CTYPE;
   for(i = 0;i < ncol;i++) {
      buf[1].s = i;
      buf[2].c[0] = colors[i].red;
      buf[2].c[1] = colors[i].green;
      buf[3].c[0] = colors[i].blue;
      ttwrite(g_out,(char *)buf,4*sizeof(short));
   }

   return(0);
}

void
ras_draw(x, y)
int x, y;
{
   ras_line(xp, yp, x, y);
}

void
ras_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;
   ttflush(g_out);
   (void)ftruncate(g_out,0);
}

void
ras_line(x1, y1, x2, y2)
int x1, y1, x2, y2;
{
   short pos[4];

   pos[0] = x1*xscreen_to_pix;
   pos[1] = y1*yscreen_to_pix;
   pos[2] = x2*xscreen_to_pix;
   pos[3] = y2*yscreen_to_pix;
   ttwrite(g_out,(char *)pos,4*sizeof(short));
}

int
ras_setup(str)
char *str;
{
   char buff[80];
   
   ttflush(g_out);
   (void)lseek(g_out,0L,0);			/* back over init stuff */
/*
 * set the default-default colour to be black
 */
   if(ttygets(g_tty,"DC",buff,80) > 0) { /* set default colour */
      default_ctype(buff);
   } else {
      default_ctype("black");
   }
   
   return(0);
}
