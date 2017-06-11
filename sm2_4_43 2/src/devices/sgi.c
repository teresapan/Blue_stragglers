/*
 * This is the SM driver for an SGI
 */
#include "copyright.h"
#include "options.h"
#if defined(SGI)
#include <stdio.h>
#include <ctype.h>
#if defined(_POSIX_SOURCE)
#  undef _POSIX_SOURCE
#  include <sys/time.h>
#  define _POSIX_SOURCE
#else
#  include <sys/time.h>
#endif
#include <math.h>
#include "sm.h"
#include "devices.h"
#include "sm_declare.h"
#undef Void				/* IBM use it as a typedef */
#undef REDRAW				/* SGI use it in GL */
#include <gl.h>
#include <gl/device.h>

#define SGI_REDRAW			/* redraw the screen */ 	\
  reshapeviewport();							\
  getsize(&xsize,&ysize);						\
  aspect = (float)ysize/xsize;						\
  editobj(graph);							\
  objreplace(VIEWPORT);							\
  viewport(0,xsize,0,ysize);						\
  closeobj();								\
  callobj(graph);							\
  gflush();

#define BACKGROUND 0		/* cmap index of background colour */
#define VIEWPORT 1              /* mark for viewport command in object */
#define XYSIZE 512		/* default window size */

static short ltypes[] = {
   0xFFFF,			/* solid lines */
   0xAAAA,			/* dots */
   0xCCCC,			/* short dashes */
   0xFAFA,			/* long dashes */
   0xB9AC,			/* dot short dash */
   0xBAEB,			/* dot long dash */
   0x0000,			/* erase */
};
#define N_LTYPE (sizeof(ltypes)/sizeof(ltypes[0]))

static int current_color;		/* the current color for lines */
static int first_color;                 /* first available color in cmap */
static long gid = -1;			/* window id */
static Object graph;                    /* object describing our graph */
static long nplanes;			/* Size of colour map */
static long xoff = 1,yoff = 1;		/* initial position of window */
static long xsize = 512,ysize=512;	/* size of window */
static void _sgi_redraw();

#if defined(rs6000)
   static void foreground();
   static void gflush();
#endif

int
sgi_setup(s)
char *s;
{
   char *backcolor = NULL;		/* name of background colour */
   char *name = "SM";
   char *ptr;
   int i;
   int have_position = 0;		/* do we have a requested position? */

   if(gid < 0) {
      if(s != NULL) {
	 for(ptr = next_word(s);*ptr != '\0';ptr = next_word((char *)NULL)) {
	    if(*ptr == '-') {
	       switch (*++ptr) {
		case 'b':
		  if(!strcmp(ptr,"bg")) {
		     backcolor = next_word((char *)NULL);
		     if(*backcolor == '\0') {
			msg("You must specify a colour with \"-bg\"\n");
			backcolor = NULL;
			break;
		     }
		  } else {
		     msg_1s("Unknown option \"%s\"\n",ptr - 1);
		  }
		  break;
		case 'g':			/* -geometry */
		  if(*(ptr = next_word((char *)NULL)) == '\0') {
		     msg("You need a geometry string with \"-g[eometry]\"\n");
		     break;
		  }
		  
		  if(sscanf(ptr,"%ldx%ld%ld%ld",&xsize,&ysize,&xoff,&yoff) == 4) {
		     have_position = 1;
		     if(xsize == 0) xsize = 1;
		     if(ysize == 0) ysize = 1;
		  } else if(sscanf(ptr,"%ld%ld",&xoff,&yoff) == 2) {
		     xsize = ysize = XYSIZE;
		     have_position = 1;
		  } else {
		     msg_1s("Error parsing -geometry string, \"%s\"\n",ptr);
		     have_position = 0;
		     xsize = ysize = XYSIZE;
		     xoff = yoff = 1;		  }
		  break;
		case 'h':			/* -help */
		  msg_1s("device sgi %s\n",
"[-bg colour] [-geometry geom] [-help] [-iconic] [-name name]");
		  break;
		case 'n':			/* -name */
		  if(*(name = next_word((char *)NULL)) == '\0') {
		     msg("You need a name string with \"-n[ame]\"\n");
		     break;
		  }
		  break;
		default:
		  msg_1s("Unknown option \"%s\"\n",ptr - 1);
		  break;
	       }
	    } else {
	       msg_1s("Unknown option \"%s\"\n",ptr);
	    }
	 }
      }
/*
 * Finished parsing string
 */
      foreground();
      default_ctype("white");           /* set_dev()'ll look in .sm */
      if(have_position) {
	prefposition(xoff,xoff + xsize - 1,yoff,yoff + ysize - 1);
      } else {
	prefsize(xsize,ysize);
      }

      if((gid = winopen(name)) == -1) {
	 msg("Can't open window\n");
	 return(-1);
      }
      winconstraints();			/* actually it removes constraints */
      greset();

      if (getplanes() == 8) {
        first_color = 16;		/* 16-31 are available under X */
        multimap(); gconfig();		/* use 16 8-bit colour maps */
        setmap((short)(getmap() + 1)%16);	/* choose the next one */
      }else{
        first_color=64;      /* try not to change all the other windows too */
      }

      nplanes = 1 << (getplanes() - 1);	/* Size of colour map */
      for(i = 1;i < N_LTYPE;i++) {
	 deflinestyle(i,ltypes[i]);
      }
      if(backcolor == NULL && *(backcolor = print_var("background")) == '\0') {
	 mapcolor(BACKGROUND,0,0,0);	/* default is black */
      } else {
	 int r,g,b;
	 if(parse_color(backcolor,&r,&g,&b) < 0) {
	    msg_1s("I don't understand the colour \"%s\" -- using black\n",
		      backcolor);
	    r = g = b = 0;
         }
	 mapcolor(BACKGROUND,r,g,b);
      }
      
      qdevice(REDRAW);

      getsize(&xsize,&ysize);
      aspect = (float)ysize/xsize;
      
      makeobj(graph = genobj());
      ortho2(0.0,(float)(SCREEN_SIZE - 1),0.0,(float)(SCREEN_SIZE - 1));
      maketag(VIEWPORT);
      viewport(0,xsize,0,ysize);
      color(BACKGROUND);
      clear();
      current_color = BLACK;
      color(current_color);
      closeobj();
   }
      
   callobj(graph);

   return(0);
}

int
sgi_cursor(get_pos,x,y)
int get_pos;				/* simply return the current position */
int *x,*y;
{
   static int first = 1;
   int i;
   short key;
   long xorig,yorig;
   
   getorigin(&xorig,&yorig);

   if(first) {
      first = 0;
      curstype(CCROSS);
      defcursor(1,(unsigned short *)NULL);
   }

   if(get_pos) {
      *x = getvaluator(MOUSEX);
      *y = getvaluator(MOUSEY);
      *x = (*x - xorig)*SCREEN_SIZE/xsize;
      *y = (*y - yorig)*SCREEN_SIZE/ysize;
      
      return(0);
   }

   setcursor(1,(Colorindex)0,(Colorindex)0);

   qdevice(LEFTMOUSE);
   qdevice(MIDDLEMOUSE);
   qdevice(RIGHTMOUSE);
   qdevice(KEYBD);

   for(;;) {
      switch(i = qread(&key)) {
       case LEFTMOUSE:
       case MIDDLEMOUSE:
       case RIGHTMOUSE:
       case KEYBD:
	 if(i == LEFTMOUSE) key = 'e';
	 else if(i == MIDDLEMOUSE) key = 'p';
	 else if(i == RIGHTMOUSE) key = 'q';

	 *x = getvaluator(MOUSEX);
	 *y = getvaluator(MOUSEY);
	 *x = (*x - xorig)*SCREEN_SIZE/xsize;
	 *y = (*y - yorig)*SCREEN_SIZE/ysize;

	 unqdevice(LEFTMOUSE);
	 unqdevice(MIDDLEMOUSE);
	 unqdevice(RIGHTMOUSE);
	 unqdevice(KEYBD);
	 setcursor(0,(Colorindex)0,(Colorindex)0);
	 return(key);
       case REDRAW:
	 SGI_REDRAW;
	 break;
       default:
	 break;
      }
   }					/* NOTREACHED */
}

void
sgi_draw(x,y)
int x,y;
{
   draw2i(x,y);

   editobj(graph);
   draw2i(x,y);
   closeobj();

   xp = x;
   yp = y;
}

void
sgi_gflush()
{
   gflush();
}

void
sgi_line(x1,y1,x2,y2)
int x1,y1,x2,y2;
{
   move2i(x1,y1);
   draw2i(x2,y2);

   editobj(graph);
   move2i(x1,y1);
   draw2i(x2,y2);
   closeobj();

   xp = x2;
   yp = y2;
}

void
sgi_reloc(x,y)
int x,y;
{
   move2i(x,y);

   editobj(graph);
   move2i(x,y);
   closeobj();

   xp = x;
   yp = y;
}

int
sgi_fill_pt(n)
int n;					/* number of sides */
{
   float dtheta, theta,
         xpsize,ypsize;
   static float old_ang,		/* old values of angle */
  	        old_xp,old_yp;		/* old values of xpsize, ypsize */
   int i;
   static long num = -1;	/* number of vertices used last time */
   static Coord vlist0[MAX_POLYGON][2];
   const Coord vlist[MAX_POLYGON][2];

   xpsize = PDEF*eexpand;
   ypsize = PDEF*eexpand;
   if(aspect > 1.0) {                   /* allow for aspect ratio of screen */
      ypsize /= aspect;
   } else if(aspect < 1.0) {
      xpsize *= aspect;
   }
   
   if(n != num || aangle != old_ang || xpsize != old_xp || ypsize != old_yp) {
      if(n > MAX_POLYGON) num = MAX_POLYGON;
      else num = n;

      dtheta = 2*PI/n;
      theta = 3*PI/2 + dtheta/2 + aangle*PI/180;
      old_ang = aangle;
      old_xp = xpsize;
      old_yp = ypsize;

      for(i = 0;i < num;i++) {
	 vlist0[i][0] = xpsize*cos(theta) + 0.5;
	 vlist0[i][1] = ypsize*sin(theta) + 0.5;
	 theta += dtheta;
      }
   }
/*
 * Set vlist; the casts are to get around the SGI's C compiler having
 * trouble with passing non-const arrays to functions, and not liking
 * the required casts. In short, it's a hack to get around a compiler
 * bug. (I think. RHL)
 */
   for(i = 0;i < num;i++) {
      *(Coord *)&(vlist[i][0]) = vlist0[i][0] + xp;
      *(Coord *)&(vlist[i][1]) = vlist0[i][1] + yp;
   }

   polf2(num,vlist);

   editobj(graph);
   polf2(num,vlist);
   closeobj();

   return(0);
}

void
sgi_redraw(fd)
int fd;
{
   fd_set mask;				/* mask of descriptors to check */
   fd_set readmask;			/* the desired mask */
   struct timeval timeout;		/* how long to wait for input */

   if(fd < 0) {				/* no input on fd is possible */
      _sgi_redraw();
      return;
   }
   
   timeout.tv_sec = 0;
   timeout.tv_usec = 10000;		/* timeout after 10ms */

   FD_ZERO(&readmask); FD_SET(fd,&readmask);   
   for(;;) {				/* wait for input to be ready */
      mask = readmask;
      if(select(fd + 1,&mask,(fd_set *)NULL,(fd_set *)NULL,&timeout) != 0){
	 break;
      }
      _sgi_redraw();
   }
}

static void
_sgi_redraw()
{
   short data;                  /* unused */
   int i;

   while((i = qtest()) != 0) {
      if(i == REDRAW) {
	 SGI_REDRAW;
	 qreset();
	 return;
      } else {
         (void)qread(&data);
      }
   }
}

void
sgi_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;
   
   delobj(graph);

   makeobj(graph = genobj());
   ortho2((float)0,(float)(SCREEN_SIZE - 1),(float)0,(float)(SCREEN_SIZE - 1));
   maketag(VIEWPORT);
   viewport(0,xsize,0,ysize);
   color(BACKGROUND);
   clear();
   color(current_color);
   closeobj();

   callobj(graph);
}

int
sgi_set_ctype(colors,n)
COLOR *colors;
int n;
{
   int i;

   if(colors == NULL) return(0);	/* just an enquiry */
   
   if(n <= nplanes) {
      for(i = 0;i < n;i++) {
	 mapcolor(first_color + i,
		  colors[i].red,colors[i].green,colors[i].blue);
      }
      return(0);
   } else {
      fprintf(stderr,"Request for too many colours; complain to rhl\n");
      return(-1);
   }
}

void
sgi_ctype(r,g,b)
int r;					/* index into colour map; NOT red */
int g,b;				/* unused */
{
   current_color = first_color + r;

   color(current_color);

   editobj(graph);
   color(current_color);
   closeobj();
}

void
sgi_close()
{
 }

int
sgi_ltype(n)
int n;
{
   if(n >= 0 && n < N_LTYPE - 1) {
      setlinestyle(n);
      return(0);
   } else if(n == 10) {			/* Erase lines */
      setlinestyle(N_LTYPE - 1);
      return(0);
   } else if(n == 11) {			/* End erase line mode */
      return(0);
   } else {
      setlinestyle(0);
      return(-1);
   }
}

int
sgi_lweight(lw)
double lw;
{
   if(lw <= 0) lw = 1;
   
   linewidth((int)(lw + 0.5));
   return(0);
}

#if defined(rs6000)			/* their GL seems to omit these */

static void
foreground()
{
   ;
}

static void
gflush()
{
   ;
}
#endif
#else
/*
 * Include a symbol for picky linkers
 */
#  ifndef lint
      static void linker_func() { linker_func(); }
#  endif
#endif
