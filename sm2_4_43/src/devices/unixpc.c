/*
 * The UNIX PC device driver for SM has been split up in two parts:
 * 
 * The low level interface does the work in device coordinates
 * (0..7xx, 0..3xx), and is in a file "upcrast.c", which is directly 
 * included in this SM specific interface file. The low level interface
 * can easily be adapted to serve for another interface to a high level
 * graphics package. It opens its own window, if the kernel has space left
 * for one. It uses rasterising and talks directly to the bitmap through 
 * system ioctl() calls.
 * 
 * The rasterising routine has been taken from the GNUPLOT 3b1 device
 * driver (John Campbell)   
 * 
 * Peter Teuben - september 1989
 */
#include "copyright.h"
#include "options.h"
#ifdef UNIXPC
#include "sm.h"
#include "tty.h"
#include "stdgraph.h"  /*  globals for filename, scale factors, ...*/
#include "sm_declare.h"
#define MAX 127
#include <stdio.h>

static FILE *im_outf ;
static char *obp, obuff[4*MAX + 5] ;
static int p1, p2, old, hp1, hp2 ;
static int hold = 0 ;
     
static char endpage = 219 ;
static char eof = 255 ;
static char setpen = 232 ;
static char setc  = 1;
     
static union {
   char c[4] ;
   short s[2] ;
   int i ;
} tmp ;

static union {
   char c[2] ;
   short s ;
} num ;

static float xscale, yscale;
static void add(),doit(),sadd();

int
upc_setup(arg)
char *arg;
{
   void uPC_init();
   void uPC_clear();
   void uPC_label();
   void uPC_flush();
   int uPC_getwidth();
   int uPC_getheight();

   if(arg != NULL && *arg != NULL) {
      msg_1s("Argument %s for now ignored\n",arg);
   }
   
   uPC_init();				/* set up the thing */
   uPC_clear();				/* clear the window */
   uPC_label("SM");			/* put label on window and icon */
   uPC_flush();            /* this also gives window control back to user */
   
   xscale = (float)uPC_getwidth() / 32768.0;	/* set scale factors */
   yscale = (float)uPC_getheight() / 32768.0;
   
   ldef = 10;				/* some other variables for SM */
   return(0);
}

void
upc_enable()
{
   ;					/* dummy, not used */
}

void
upc_line(x1, y1, x2, y2)
int x1, y1, x2, y2;
{
   void uPC_move();
   void uPC_vector();

   x1 = (int) ((float)x1 * xscale);
   y1 = (int) ((float)y1 * yscale);
   x2 = (int) ((float)x2 * xscale);
   y2 = (int) ((float)y2 * yscale);
   
   uPC_move(x1,y1);
   uPC_vector(x2,y2);
#if 0
   uPC_flush();				/* control back to user ??? */
#endif
}

void
upc_reloc(x, y)
int x, y;
{
   void uPC_move();
   
   uPC_move(x,y);
}
     
void
upc_draw(x, y)
int x, y;
{
   upc_line(xp, yp, x, y);
}

int
upc_char(str,x,y)
char *str;
int x, y;
{
   return(-1);              /* use SM character set */
}

int
upc_ltype(i)
int i;
{
   void uPC_linetype();

   if(i == 10 || i == 11) {		/* no erasing of lines */
      return(-1);
   }
   uPC_linetype(i);        /* not quite the SM definitions, but ... */
   return(0);
}

int
upc_lweight(lw)
double lw;
{
   return(-1);              /* not available yet */
}

void
upc_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

   void uPC_clear();

   uPC_clear();
}

void
upc_idle()
{
   ;
}

int
upc_cursor(get_pos,x,y)
int *x, *y;
{
   int uPC_cursor();

   return(uPC_cursor(get_pos,x,y));
}

void
upc_close()
{
   void uPC_quit();
   
   uPC_quit();
}

int
upc_dot(x,y)
int x,y;
{
   return(-1);
}

int
upc_fill_pt(n)
int n;
{
   return(-1);
}

void
upc_ctype(r,g,b)
int r,g,b;
{
   ;					/* not implemented */
}

int
upc_set_ctype(col,n)
COLOR *col;
int n;
{
    return(-1);
}

void
upc_gflush()
{
   void uPC_flush();

   uPC_flush();
}
#include "upcrast.c"		/* include code directly here */
#endif
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif
