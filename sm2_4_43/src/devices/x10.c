#include "options.h"
#ifdef XWINDOWS
#include <math.h>
#include "sm.h"
#include "sm_declare.h"

#define START_NVEC 200		/* starting value for max_nvec */
/*
 * X - window driver for SM
 *
 * Written by trq on 2/28/87 but if you ask me I'll deny everything.
 *
 * Modified rhl, but if you ask me I'll implicate trq.
 */
#include <X/Xlib.h>
#include <stdio.h>
#include <ctype.h>
#include "cursor.h"			/* define the cursor to use */

#if defined(ANSI_CPP)
#define XCREATECURSOR(S,FORE,BACK,FUNC)	/* create a cursor */\
	XCreateCursor(S ## _width,S ## _height,S ## _bits,S ## _mask_bits,    \
					S ## _x_hot,S ## _y_hot,FORE,BACK,FUNC)
#else
#define XCREATECURSOR(S,FORE,BACK,FUNC)	/* create a cursor,		      \
					   using the cpp to concat. strings */\
	XCreateCursor(S/**/_width,S/**/_height,S/**/_bits,S/**/_mask_bits,    \
					S/**/_x_hot,S/**/_y_hot,FORE,BACK,FUNC)
#endif
/*
 * X function declarations
 */
int XPending();
void XClear(),
     XDefineCursor(),
     XFlush(),
     XLine(),
     XMapWindow(),
     XNextEvent(),
     XSelectInput(),
     XSync(),
     XText(),
     XUndefineCursor(),
     XUnmapWindow(),
     XWarpMouse(),
     XWindowEvent();

static Cursor curs;			/* graphics cursor to use */
static Display *display;
static Font font;
static int height;			/* height of window */
static struct xvector {
    int x1, y1, x2, y2;
} *xvec;
static int backpix,			/* pixel value for background */
	   linepix,			/* pixel value for lines */ 
	   h_font,w_font,		/* height and (mean) width of chars */
	   nvec,			/* number of vectors drawn */
	   max_nvec;			/* max number of vectors allocated */
static void size_window(),
	    _x_redraw();
static Window wind = 0;

int
x_setup(s)
char *s;
{
   char name[20];			/* possible display name */
   char *color;				/* name of a colour */
   Color cdef;
   FontInfo font_info;			/* query font */
   int i;
   OpaqueFrame frame;			/* border/background for windows */
   static short gray_bits[16] = {  /* A 16*16 array of bits, in the pattern: */
      0xaaaa, 0x5555, 0xaaaa, 0x5555,	/* 1010101010101010 */
      0xaaaa, 0x5555, 0xaaaa, 0x5555,	/* 0101010101010101 */
      0xaaaa, 0x5555, 0xaaaa, 0x5555,	/* 1010101010101010 */
      0xaaaa, 0x5555, 0xaaaa, 0x5555};	/* 0101010101010101 */
   Window premade;			/* non-zero if window premade */
      
   if(wind == 0) {
      while(*s != '\0' && isspace(*s)) s++;
      if(s != NULL) {
	 if(str_scanf(s,"DISPLAY %s ID %d",name,&wind) == 2) {
	    s = name;
	 }
      }
      if(s[0] == '\0') {
	 s = NULL;
      }
      premade = wind;
      if(s == NULL || strcmp(s,"preopened") != 0) {
         if((display = XOpenDisplay(s)) == NULL) {
            msg("Could not open Display\n");
            return(-1);
         }
      }
      frame.bdrwidth = 4;
      frame.border = XMakePixmap(XStoreBitmap(16,16,gray_bits),
							BlackPixel,WhitePixel);
      backpix = BlackPixel;
      if(*(color = print_var("background")) != '\0') {
	 if((i = XParseColor(color,&cdef)) == 0) {	/* failed */
	    int r,g,b;
	    if(parse_color(color,&r,&g,&b) >= 0) {
	       cdef.red = (r << 8);
	       cdef.green = (g << 8);
	       cdef.blue = (b << 8);
	       i = 1;
	    }
	 }
	 if(i != 0 && XGetHardwareColor(&cdef)) {
	    backpix = cdef.pixel;
	 }
      }
      frame.background = XMakeTile(backpix);

      if(!premade &&
          (wind = XCreate(PROGNAME,"",NULL,"512x512+100+100",&frame,200,200))
									== 0) {
         msg("Can't create display\n");
         return(-1);
      }

      curs = XCREATECURSOR(cursor,BlackPixel,WhitePixel,GXcopy);

      font = XGetFont("vtsingle");
      if(XQueryFont(font,&font_info) == 0) {
	 h_font = 13;			/* correct for vtsingle */
	 w_font = 8;
      } else {
	 h_font = font_info.height;
	 w_font = font_info.width;
      }
      XSelectInput(wind, ExposeRegion|ExposeWindow);
      max_nvec = START_NVEC;
      if((xvec = (struct xvector *)malloc((unsigned)max_nvec*
					     sizeof(struct xvector))) == NULL){
	 msg("Can't allocate vectors in x_setup\n");
 	 max_nvec = 0;
	 return(-1);
      }
      nvec = 0;
/*
 * No need to look at .sm for linepix, set_dev()'ll do it for us.
 */
      default_ctype("white");
      linepix = WhitePixel;
   } else {
      XUnmapWindow(wind);
   }
   if(premade) XUnmapWindow(wind);
   size_window();                        /* get size of window and scale */
   XMapWindow(wind);
/* finally, set some variables for SM */
   ldef = 32;
   return(0); 
}

/*
 * Only support erasing of lines (lt 10 and 11)
 */
int
x_ltype(lt)
int lt;
{
   static int old_linepix = -1;
   
   if(lt == 10) {
      if(linepix != backpix) {
	 old_linepix = linepix;
      }
      linepix = backpix;
      return(0);
   } else if(lt == 11) {
      if(old_linepix >= 0) {
	 linepix = old_linepix;
      }
      return(0);
   }
   return(-1);
}

void
x_line(x1, y1, x2, y2)
int x1, y1, x2, y2;
{
    char *malloc(), *realloc();

    xp = x2;
    yp = y2;
    x1 = x1*xscreen_to_pix;
    x2 = x2*xscreen_to_pix;
    y1 = height - y1*yscreen_to_pix;
    y2 = height - y2*yscreen_to_pix;
    XLine(wind, x1, y1, x2, y2, 1, 1, linepix, GXcopy, AllPlanes);
    if(nvec >= max_nvec) {
	max_nvec *= 2;
	if((xvec = (struct xvector *)realloc((char *)xvec,
		   (unsigned)(max_nvec*sizeof(struct xvector)))) == NULL) {
	    msg("Can't reallocate vectors in x_line\n");
	    max_nvec = 0;
	}
    }
    xvec[nvec].x1 = x1;
    xvec[nvec].y1 = y1;
    xvec[nvec].x2 = x2;
    xvec[nvec].y2 = y2;
    nvec++;
}

void
x_reloc(x, y)
int x, y;
{
    xp = x;
    yp = y;
}

void
x_draw(x, y)
int x, y;
{
    x_line(xp, yp, x, y);
}

int
x_char(s, x, y)
char *s;
int x, y;
{
    if(s == NULL) {
	if(aangle == 0.0 && eexpand == 1.0) {
	    cheight = h_font/yscreen_to_pix;
	    cwidth = w_font/xscreen_to_pix;
	}
	return(0);		/* we do hard characters */
    }

    XText(wind,(int)(x*xscreen_to_pix),
	  height - (int)(0.5*h_font + y*yscreen_to_pix),
					s,strlen(s),font,linepix,backpix);
    return(0);
}

void
x_erase(int raise_on_erase)
{
    volatile int roe = raise_on_erase; roe++;

    XClear(wind);
    nvec = 0;
}


/*
 * Set colour of line 
 */
void
x_ctype(r,g,b)
int r,g,b;				/* red, green, blue in [0,255] */
{
   char *realloc();
   Color cdef;

   cdef.red = r*257;			/* scale to [0,65535] */
   cdef.green = g*257;
   cdef.blue = b*257;
   if(XGetHardwareColor(&cdef)) {
      linepix = cdef.pixel;
   }
/*
 * Put line colour change on vector list as a pseudo-line (-ve x1)
 */
   if(nvec >= max_nvec) {
      max_nvec *= 2;
      if((xvec = (struct xvector *)realloc((char *)xvec,
		   (unsigned)(max_nvec*sizeof(struct xvector)))) == NULL) {
	 msg("Can't reallocate vectors in x_ctype\n");
	 max_nvec = 0;
      }
   }
   xvec[nvec++].x1 = -linepix;
}

void
x_idle()
{
   XFlush();
   if(XPending() > 0) {
      size_window();
      _x_redraw();
   }
}

void
x_gflush()
{
   XFlush();
}

int
x_fill_pt(n)
int n;                          /* number of sides */
{
#if !defined(sun)
   float dtheta, theta;
   static float old_ang;	/* old values of angle */
   int i,
       xpsize,			/* scale for points */
       ypsize;
   static int num = -1,         /* number of vertices used last time */
	      old_xp,old_yp,	/* old values of xpsize, ypsize */
	      x0,y0;		/* constant part of vertex[0].x, .y */
   static Vertex vlist[MAX_POLYGON + 1];  /* vertices describing point */
#endif

   if(n < 2) {
      x_line(xp,yp,xp,yp);
      return(0);
#ifdef sun
   } else {
      return(-1);
   }
#else
   }
   
   dtheta = 2*PI/n;
   xpsize = 2*PDEF*sin(dtheta/2)*eexpand*xscreen_to_pix;
   ypsize = 2*PDEF*sin(dtheta/2)*eexpand*yscreen_to_pix;
   if(n != num || aangle != old_ang || xpsize != old_xp || ypsize != old_yp) {
      if(n > MAX_POLYGON) num = MAX_POLYGON;
      else num = n;

      theta = 3*PI/2 + dtheta/2 + aangle*PI/180;

      old_ang = aangle;
      old_xp = xpsize;
      old_yp = ypsize;

      x0 = PDEF*xscreen_to_pix*eexpand*cos(theta);
      y0 = height - PDEF*yscreen_to_pix*eexpand*sin(theta);
      vlist[0].flags = !VertexRelative | VertexDontDraw;

      theta += dtheta/2;
      for(i = 1;i <= num + 1;i++) {
	 vlist[i].x = -xpsize*sin(theta);
	 vlist[i].y = -ypsize*cos(theta);	/* screen is upside down */
	 vlist[i].flags = VertexRelative;
	 theta += dtheta;
      }
   }
   vlist[0].x = xscreen_to_pix*xp + x0;
   vlist[0].y = y0 - yscreen_to_pix*yp;
   XDrawFilled(wind, vlist, num+1, linepix, GXcopy, AllPlanes);

   return(0);
#endif
}

int
x_cursor(get_pos, x, y)
int get_pos;				/* simply return the current position */
int *x, *y;
{
   int i,
       kx,ky;			/* position of cursor in text window */
   static int cx = -1,cy = -1;	/* position of cursor in graphics window */
   XKeyOrButtonEvent rep;
   Window subw;			/* we don't actually care about subw */
   WindowInfo winfo;			/* information about a Window */

   XUpdateMouse(RootWindow,&kx,&ky,&subw);

   XDefineCursor(wind,curs);		/* define graphics cursor */
   XSelectInput(wind,ExposeRegion|ExposeWindow|KeyPressed|ButtonReleased);
   if(cx >= 0) {
      XWarpMouse(wind,cx,cy);		/* return cursor to old position */
   } else {
      XQueryWindow(wind,&winfo);
      XWarpMouse(wind,winfo.width/2,winfo.height/2);
   }

   XWindowEvent(wind,KeyPressed|ButtonReleased,&rep); /* wait for something */

   XSelectInput(wind,ExposeRegion|ExposeWindow);
   XUpdateMouse(wind,&cx,&cy,&subw);		/* current position */
   *x = cx/xscreen_to_pix;
   *y = (height - cy)/yscreen_to_pix;
   XUndefineCursor(wind);
   XSync(1);					/* we do need this, but why? */
   XWarpMouse(RootWindow,kx,ky);		/* return to GO */

   if(rep.type == ButtonReleased) {		/* Mouse */
      if((rep.detail & 0xff) == LeftButton) {
	 return('e');
      } else if((rep.detail & 0xff) == MiddleButton) {
	 return('p');
      } else if((rep.detail & 0xff) == RightButton) {
	 return('q');
      } else {
	 return('p');			/* unknown button */
      }
   }
/*
 * must be keyboard
 */
   return(*XLookupMapping(&rep,&i));
}

void
x_close()
{
   ;
}

void
x_redraw(fd)
int fd;					/*NOTUSED*/
{
   _x_redraw();
}

static void
_x_redraw()
{
    XEvent rep;
    struct xvector *vec;

    XPending();
    while(QLength() > 0) {
	XNextEvent(&rep);
    }
    for(vec = xvec; vec < xvec + nvec; vec++) {
        if(vec->x1 < 0) {
	    linepix = -vec->x1;
	    continue;
        }
	XLine(wind, vec->x1, vec->y1, vec->x2, vec->y2, 1, 1, linepix, GXcopy,
	  AllPlanes);
    }
    XFlush();
}

static void
size_window()
{
   WindowInfo winfo;			/* information about a Window */

   XQueryWindow(wind,&winfo);
   xscreen_to_pix = winfo.width/(float)SCREEN_SIZE;
   height = winfo.height;
   yscreen_to_pix = winfo.height/(float)SCREEN_SIZE;
   aspect = yscreen_to_pix/xscreen_to_pix;
}
#endif
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif
