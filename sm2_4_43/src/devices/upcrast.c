/*
 *   This UNIXPC graphics interface consists of two layers:
 *
 *   This file contains all lower level routines, which deal
 *   device specific coordinates. It is included in the unixpc.c
 *   file.
 *
 *   When uPC_init() is called, a new window is opened (/dev/window)
 *   with a default size of about 8 by 8 cm (304 by 192 pixels), but
 *   the environment variables YAPP, YAPP_X and YAPP_Y can be
 *   used to communicate to these routine that they should use
 *   a different device (e.g. /dev/wN) and or a different size.
 *   Remember that the YAPP_X must be a multiple of 16.
 *
 *   The rasterizing routines here have been adapted from gnuplot
 *   (C)  John Cambell
 *
 */

#include <stdio.h>
#include <sys/window.h>
#include <sys/signal.h>
#include <fcntl.h>	
#include <sys/types.h>	
#include <sys/stat.h>
#include <errno.h>

#define uPC_HIGH_BIT    (0x8000)

typedef unsigned short Scr_type;
typedef unsigned char Scr_kluge;

#define uPC_XMAX 304
#define uPC_YMAX 192

#define uPC_XSIZE uPC_XMAX/16
#define uPC_YSIZE uPC_YMAX

static Scr_type       *uPC_display;    /* array will be allocated dynamically */
static int            uPC_width = 2*uPC_XSIZE;
static int            uPC_sx=0, uPC_sy=0;
static int            uPC_cur_linetype=0;
static unsigned short uPC_raster_count=0;

#define uPC_XLAST (uPC_XMAX - 1)
#define uPC_YLAST (uPC_YMAX - 1)

#define uPC_VCHAR 12
#define uPC_HCHAR  9
#define uPC_VTIC   8
#define uPC_HTIC  12

extern errno, sys_nerr;
extern char *sys_errlist[];

static struct urdata uPC_ur;


static int   wid;                           /* fd for  plotting window */
static char *defname = "/dev/window";       /* default window : generated */
static int   wwidth, wheight;               /* actual size of window */

#define IfErrOut(e1,e2,s1,s2) if (e1 e2) {\
fprintf(stderr, "%s:: %s %s\n", sys_errlist[errno], s1, s2);\
uPC_fixwind(0);\
close(wid);\
exit(-1);}

uPC_init()
{
   int i;
   struct uwdata uw;
   int uPC_fixwind();
   short gw;
   char *getenv();
   char *ep, *cp, *malloc();
   Scr_type *ip, *jp;

   ep = getenv("YAPP");
   if (*ep==NULL)
      ep = defname;         /* fill in a default name */
   printf("Opening window %s\n",ep);
   wid = open(ep,O_RDWR);
   if (wid<0) {
       perror("Couldn't open graphics window YAPP\n");
       exit(-1);
   }

/*  Set the screen size */   

   uw.uw_x = 0;                     /* location of upper left corner */
   uw.uw_y = 12; 

   ep = getenv("YAPP_X");           /* width */
   if (*ep==NULL)
       uw.uw_width = 304;
   else
       uw.uw_width = atoi(ep);
   ep = getenv("YAPP_Y");           /* height */
   if (*ep==NULL)
       uw.uw_height = 192;
   else
       uw.uw_height = atoi(ep);

   fprintf (stderr,"Window will have size %d x %d\n", 
            uw.uw_width, uw.uw_height);
   uw.uw_uflags = 256;         /* created with border */

   IfErrOut (ioctl(wid, WIOCSETD, &uw), <0, "ioctl failed on", "WIOCSETD");

   wwidth = uw.uw_width;        /* must be divisible by 16 */
   wheight = uw.uw_height;

    if (wwidth % 16) {
        printf("Error: width of window MUST be a multiple of 16\n");
        exit(-1);
    }
    ip = (Scr_type *) malloc(wwidth * wheight / 8 );  
    printf ("ip @ %d\n",ip);

    uPC_display = (Scr_type *) ip;
    cp = malloc(1000);        /* safeguard junk tail */
    printf("uPC_display @ %d\n",uPC_display);
    printf ("Final: ip=%d uPD_ = %d  cp=%d\n",ip,uPC_display,cp);

    uPC_ur.ur_srcbase  = (unsigned short *) uPC_display;
    uPC_ur.ur_srcwidth = wwidth/8;
    uPC_ur.ur_dstbase  = 0;
    uPC_ur.ur_dstwidth = 0;
    uPC_ur.ur_srcx     = 0;
    uPC_ur.ur_srcy     = 0;
    uPC_ur.ur_dstx     = 0;
    uPC_ur.ur_dsty     = 0;
    uPC_ur.ur_width    = wwidth;
    uPC_ur.ur_height   = wheight;
    uPC_ur.ur_srcop    = SRCSRC;
    uPC_ur.ur_dstop    = DSTOR;
    uPC_ur.ur_pattern  = 0;
    printf("init done ... \n");
}


uPC_getwidth()
{
    return wwidth;
}

uPC_getheight()
{
    return wheight;
}

uPC_clear()
{
/* This routine will clear the uPC_display buffer and window. */
   register Scr_type *j;
   register int i;

   j = (Scr_type *)uPC_display;
   i = uPC_YSIZE*uPC_XSIZE + 1;

   while (--i)
      *j++ = 0;

   printf("Raster will be cleared now ... \n");
   uPC_ur.ur_dstop = DSTSRC;   /* replace (clear screen). */
   IfErrOut (ioctl(wid, WIOCRASTOP, &uPC_ur), <0,
      "ioctl failed", "WIOCRASTOP");
   uPC_ur.ur_dstop = DSTOR;   /* Or in (show text) */
}

uPC_label(s)
char *s;
{		/* put labels in border and icon */
    struct utdata ut;

    strncpy(ut.ut_text, s, WTXTLEN);
    ut.ut_text[strlen(s)] = '\0';
    ut.ut_num = WTXTUSER;
    ioctl(wid, WIOCSETTEXT, &ut);
    ut.ut_num = WTXTLABEL;
    ioctl(wid, WIOCSETTEXT, &ut);    
}


uPC_border()
{
/* This routine will draw a border around the graphics frame */
    uPC_move(0,         0);
    uPC_move(uPC_XLAST, 0);
    uPC_move(uPC_XLAST, uPC_YLAST);
    uPC_move(0,         uPC_YLAST);
    uPC_move(0,         0);
}

uPC_flush()
{
/* This routine will flush the display. */

   IfErrOut (ioctl(wid, WIOCRASTOP, &uPC_ur), <0,
      "ioctl failed", "WIOCRASTOP");
   ioctl(0,WIOCSELECT);     /* return active window to original owner */
}


uPC_linetype(linetype)
int linetype;
{
/* This routine records the current linetype. */
   if (uPC_cur_linetype != linetype) {
      uPC_raster_count = 0;
      uPC_cur_linetype = linetype;
   }
}


uPC_move(x,y)
unsigned int x,y;
{
/* This routine just records x and y in uPC_sx, uPC_sy */
   uPC_sx = x;
   uPC_sy = y;
}


/* Was just (*(a)|=(b)) */
#define uPC_PLOT(a,b)   (uPC_cur_linetype != 0 ? uPC_plot_word (a,b) :\
                                *(a)|=(b))

uPC_plot_word(a,b)
Scr_type *a, b;
/*
   Weak attempt to make line styles.  The real problem is the aspect
   ratio.  This routine is called only when a bit is to be turned on in
   a horizontal word.  A better line style routine would know something
   about the slope of the line around the current point (in order to
   change weighting).

   This yields 3 working linetypes plus a usable axis line type.
*/
{
/* Various line types */
   switch (uPC_cur_linetype) {
   case -1:
   /* Distinguish between horizontal and vertical axis. */
      if (uPC_sx > uPC_XMAX/8 && uPC_sx < 7*uPC_XMAX/8) {
      /* Fuzzy tolerance because we don't know exactly where the y axis is */
         if (++uPC_raster_count % 2 == 0) *(a) |= b;
      }
      else {
      /* Due to aspect ratio, take every other y pixel and every third x. */
         *(a) |= (b & 0x9999);
      }
   break;
   case 1:
   case 5:
   /* Make a |    |----|    |----| type of line. */
      if ((1<<uPC_raster_count) & 0xF0F0) *(a) |= b;
      if (++uPC_raster_count > 15) uPC_raster_count = 0;
   break;
   case 2:
   case 6:
   /* Make a |----|----|----|--- |    | type of line. */
      if ((1<<uPC_raster_count) & 0x0EFFF) *(a) |= b;
      if (++uPC_raster_count > 19) uPC_raster_count = 0;
   break;
   case 3:
   case 7:
   /* Make a | -  | -  | -  | -  | type of line. */
      if ((1<<uPC_raster_count) & 0x4444) *(a) |= b;
      if (++uPC_raster_count > 15) uPC_raster_count = 0;
   break;
   case 4:
   case 8:
   default:
      *(a) |= b;
   break;
   }
}

uPC_vector(x,y)
unsigned int x,y;
{
/* This routine calls line with x,y */
   int x1 = uPC_sx, y1=uPC_sy, x2 = x, y2 = y;
   register int  c, e, dx, dy, width;
   register Scr_type mask, *a;
   static Scr_type lookup[] = {
      0x0001, 0x0002, 0x0004, 0x0008,
      0x0010, 0x0020, 0x0040, 0x0080,
      0x0100, 0x0200, 0x0400, 0x0800,
      0x1000, 0x2000, 0x4000, 0x8000,
   };

/* Record new sx, sy for next call to the vector routine. */
   uPC_sx = x2;
   uPC_sy = y2;

   a = &uPC_display[(x1>>4) + (uPC_YSIZE-1-y1)*(wwidth/16)];

/*   a = &uPC_display[(uPC_YSIZE - 1) - y1][x1 >> 4];	*/
   mask = lookup[x1 & 0x0f];
   width = uPC_width;

   if ((dx = x2 - x1) > 0) {
      if ((dy = y2 - y1) > 0) {
         if (dx > dy) {         /* dx > 0, dy > 0, dx > dy */
            dy <<= 1;
            e = dy - dx;
            c = dx + 2;
            dx <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  (Scr_kluge *)a -= width;
                  e -= dx;
               }
               if (mask & uPC_HIGH_BIT) {
                  mask = 1;
                  a++;
               } else
                  mask <<= 1;
               e += dy;
            }
         } else {            /* dx > 0, dy > 0, dx <= dy */
            dx <<= 1;
            e = dx - dy;
            c = dy + 2;
            dy <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  if (mask & uPC_HIGH_BIT) {
                     mask = 1;
                     a++;
                  } else
                     mask <<= 1;
                  e -= dy;
               }
               (Scr_kluge *)a -= width;
               e += dx;
            }
         }
      } else {
         dy = -dy;
         if (dx > dy) {         /* dx > 0, dy <= 0, dx > dy */
            dy <<= 1;
            e = dy - dx;
            c = dx + 2;
            dx <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  (Scr_kluge *)a += width;
                  e -= dx;
               }
               if (mask & uPC_HIGH_BIT) {
                  mask = 1;
                  a++;
               } else
                  mask <<= 1;
               e += dy;
            }
         } else {            /* dx > 0, dy <= 0, dx <= dy */
            dx <<= 1;
            e = dx - dy;
            c = dy + 2;
            dy <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  if (mask & uPC_HIGH_BIT) {
                     mask = 1;
                     a++;
                  } else
                     mask <<= 1;
                  e -= dy;
               }
               (Scr_kluge *)a += width;
               e += dx;
            }
         }
      }
   } else {
      dx = -dx;
      if ((dy = y2 - y1) > 0) {
         if (dx > dy) {         /* dx <= 0, dy > 0, dx > dy */
            dy <<= 1;
            e = dy - dx;
            c = dx + 2;
            dx <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  (Scr_kluge *)a -= width;
                  e -= dx;
               }
               if (mask & 1) {
                  mask = uPC_HIGH_BIT;
                  a--;
               } else
                  mask >>= 1;
               e += dy;
            }
         } else {            /* dx <= 0, dy > 0, dx <= dy */
            dx <<= 1;
            e = dx - dy;
            c = dy + 2;
            dy <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  if (mask & 1) {
                     mask = uPC_HIGH_BIT;
                     a--;
                  } else
                     mask >>= 1;
                  e -= dy;
               }
               (Scr_kluge *)a -= width;
               e += dx;
            }
         }
      } else {
         dy = -dy;
         if (dx > dy) {         /* dx <= 0, dy <= 0, dx > dy */
            dy <<= 1;
            e = dy - dx;
            c = dx + 2;
            dx <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  (Scr_kluge *)a += width;
                  e -= dx;
               }
               if (mask & 1) {
                  mask = uPC_HIGH_BIT;
                  a--;
               } else
                  mask >>= 1;
               e += dy;
            }
         } else {            /* dx <= 0, dy <= 0, dx <= dy */
            dx <<= 1;
            e = dx - dy;
            c = dy + 2;
            dy <<= 1;

            while (--c) {
               uPC_PLOT(a, mask);
               if (e >= 0) {
                  if (mask & 1) {
                     mask = uPC_HIGH_BIT;
                     a--;
                  } else
                     mask >>= 1;
                  e -= dy;
               }
               (Scr_kluge *)a += width;
               e += dx;
            }
         }
      }
   }
}


uPC_lrput_text(row,str)
unsigned int row;
char str[];
{
   int col = 80-strlen(str), num, i;
   struct utdata ut;
   char *txt=ut.ut_text;

/* Fill in the pad. */
   for (i = 0; i < col; i++)
      txt[i] = ' ';
/* Then stick in the text. */
   txt[i] = '\0';
   strcat (txt, str);

   if (row > 2)
      puts (txt);
   else {
   /* This will fit on the 2 bottom "non-graphic" lines. */
      switch (row) {
      case 0: ut.ut_num =  WTXTSLK1; break;
      case 1: ut.ut_num =  WTXTSLK2; break;
      }
      ioctl (wid, WIOCSETTEXT, &ut);
   }
#if 0
   wgoto (1, 24, 0);
#endif
}

uPC_ulput_text(row,str)
unsigned int row;
char str[];
{
/* This routine puts the text in the upper left corner. */

/* Just use the ANSI escape sequence CUP (iswind said that was ok!) */
   printf ("\033[%d;%dH%s\033[25;1H", row+2, 2, str); /* +1 +2 ? */
   fflush (stdout);
}


uPC_reset()
{
/* Reset window to normal size. */
   uPC_fixwind (0);
}



uPC_fixwind(signo)
int signo;
{
   static struct uwdata wreset = { 0, 12, 304, 192, 0x1};
   struct utdata ut;

/* Reset the window to the right size. */
   ioctl(wid, WIOCSETD, &wreset);   /* 0, not wncur here! */

/* Clear the lines affected by an _lrput_text. */
   ut.ut_text[0] = '\0';
   ut.ut_num =  WTXTSLK1;
   ioctl(wid, WIOCSETTEXT, &ut);
   ut.ut_num =  WTXTSLK2;
   ioctl(wid, WIOCSETTEXT, &ut);
/* Scroll the screen once. (avoids typing over the same line) */
   fprintf (stderr, "\n");

   if (signo) {
      if (signo == SIGILL || signo == SIGTRAP || signo == SIGPWR)
         signal (signo, SIG_DFL);
      kill (0,signo);  /* Redo the signal (as if we never trapped it). */
   }
}


uPC_quit()
{
	close(wid);
}

uPC_cursor(get_pos,x,y)
int get_pos;				/* simply return the current position */
int *x, *y;
   /* get mouse position, returns 1,2,3 if left, middle, right button pushed */
{
    struct umdata um ;

    um.um_flags = MSUP | MSIN;
    um.um_x = 0;
    um.um_y = 0;
    um.um_w = wwidth;
    um.um_h = wheight;
    um.um_icon = NULL;

    ioctl (wid, WIOCGETMOUSE, &um);
}
