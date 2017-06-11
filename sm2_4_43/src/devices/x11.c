/*
 * X11 device driver for SM.
 *
 * If you define DISABLE_BACKING to cc the code will not use
 * a backing store, even if it is supported by the server
 */
#include "copyright.h"
#include "options.h"
#if defined(X11)
#define X11_C				/* need this in sm_declare.h on suns */
#ifdef SYS_V
#  define SYSV				/* needed by X */
#endif
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#ifdef hpux
#define _STRING_INCLUDED
#endif
#include <X11/Xos.h>
#if defined(NODEVICE)			/* X on an IBM/6000 defines it */
#  undef NODEVICE
#endif
#include "sm.h"
#include "devices.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm_declare.h"

#if !defined(FORCE_BACKING_STORE) && !defined(NO_SELECT)
#  define DISABLE_BACKING
#endif

#define NARG 20
#define NVEC 200			/* size of xvec */

/*
 * This is a struct for all the SM-device dependent variables
 */
typedef struct {
   int is_complete;			/* is window creation complete? */
   char forecolor[40];			/* name of foreground colour */
   Colormap cmap;
   float cmap_sq;			/* how much cmap is compressed to fit*/
   Cursor graphics_cursor;		/* cursor for cursor */
   Display *display;
   Font font;
   GC erase_gc;				/* Graphics Context for erasing */
   GC gc;				/* GC for drawing */
   GC redraw_gc;			/* GC for redrawing */
   int cx, cy;				/* last position of cursor in wind */
   int old_sm_image_cursor;		/* old value of sm_image_cursor */
   int old_forepix;			/* old foreground colour */
   int old_height, old_width;
   int backpix;				/* pixel value for background */
   int focus_revert;			/* how should the focus revert? */
   int have_backing;			/* server supports backing store */
   int h_font,w_font;			/* height and (mean) width of chars */
   unsigned int depth;			/* depth of display */
   unsigned int height;			/* height of window */
   unsigned int width;			/* width of window */
   int xfd;				/* file descriptor for X connection */
   long event_mask;			/* which events do I want to see? */
   Pixmap backing;			/* backing for redrawing window */
   Window curswind;			/* cursor window */
   Window focus;			/* window with initial focus,
					   if we care */
   Window root;				/* root window */
   Window wind;				/* graphics window */
} SMX11;

static SMX11 *alloc_smx11();
static char *get_resource();
static int get_foreground(),		/* get the current foreground */
	   X_get_opts();
static void size_x11(),
	    usage(),
	    x_clear_pixmap(),
            X_get_opts_from_args();
static XEvent *_x11_redraw();
static int hand_XError();
static void do_configure_notify(),
            hand_XIOError();
#if defined(TK)
   static void smtk_DoOneEvent P_ARGS((Display *display));
#endif

extern float xscreen_to_pix,yscreen_to_pix;
extern int sm_verbose;			/* be chatty? */
static int nvec;                        /* number of vectors drawn */
static XSegment *xvec = NULL;		/* line-drawing buffer */

static Atom del_atom = None;		/* used to handle WM_DELETE_WINDOW */
static SMX11 **dev_x11 = NULL;		/* all of SM's X11 devices */
#define NDEV_X11 5			/* initial value of ndev_x11 */
static int ndev_x11 = NDEV_X11;		/* number of SM's X11 devices */
static int which_dev_x11 = -1;		/* index of current SMX11 */
#define XDEV (dev_x11[which_dev_x11])	/* current SMX11 */

static XrmOptionDescRec options[] = {
   {"#",		".icongeometry", XrmoptionStickyArg,	NULL},
   {"=",		".geometry",	XrmoptionStickyArg,	NULL},
   {"-borderwidth",	".borderwidth",	XrmoptionSepArg,	NULL},
   {"-bd",		".borderwidth",	XrmoptionSepArg,	NULL},
   {"-background",	".background",	XrmoptionSepArg,	NULL},
   {"-bg",		".background",	XrmoptionSepArg,	NULL},
   {"-cmap",		".colormap",	XrmoptionNoArg,		"1"},
   {"-colormap",	".colormap",	XrmoptionNoArg,		"1"},
   {"-colourmap",	".colormap",	XrmoptionNoArg,		"1"},
   {"-device",		".device",	XrmoptionSepArg,	NULL},
   {"-display",		".display",	XrmoptionSepArg,	NULL},
   {"-focus",		".kbdfocus",	XrmoptionNoArg,		"1"},
   {"-foreground",	".foreground",	XrmoptionSepArg,	NULL},
   {"-fg",		".foreground",	XrmoptionSepArg,	NULL},
   {"-fn",		".font",	XrmoptionSepArg,	NULL},
   {"-font",		".font",	XrmoptionSepArg,	NULL},
   {"-geometry",	".geometry",	XrmoptionSepArg,	NULL},
   {"-iconic",		".iconic",	XrmoptionNoArg,		"1"},
   {"-kbdfocus",	".kbdfocus",	XrmoptionNoArg,		"1"},
   {"-name",		".name",	XrmoptionSepArg,	NULL},
   {"-ndevice",		".ndevice",	XrmoptionSepArg,	NULL},
   {"-nocw",		".nocurswindow",  XrmoptionNoArg,	"1"},
   {"-nocurswindow",	".nocurswindow",  XrmoptionNoArg,	"1"},
   {"-preopened",	".preopened",	XrmoptionSepArg,	NULL},
   {"-synchronise",	".synchronise",	XrmoptionNoArg,		"1"},
   {"-title",		".title",	XrmoptionSepArg,	NULL},
};
#define NOPT (sizeof(options)/sizeof(options[0]))

int
x11_setup(s)
char *s;
{
   char *backcolor;			/* name of background colour */
   char *fontname;			/* name of font */
   char *name;				/* name of application */
   char *ptr;
   char *title;				/* title of application */
   Colormap cmap;			/* colour map for this device */
   int border_width,			/* as it says */
       i,
       iconic = 0,			/* start iconised? */
       icon_x,icon_y,			/* offset of icon */
       icon_position = 0,		/* is icon position specified? */
       root_height,			/* height of display */
       root_width,			/* width of display */
       screen,				/* screen in use */
       sync = 0,			/* Synchronise Errors */
       want_curswindow,			/* do we want a cursor window? */
       want_cmap,			/* dp we want our own colour map? */
       xoff,yoff,			/* position of window */
       x_position = 0;			/* is window position specified? */
   unsigned int xs = 512,ys = 512;	/* size of window */
   unsigned long mask,                  /* mask for attributes */
		 GCmask;		/* mask for setting GCs */
   Pixmap icon_pixmap;
   static unsigned char gray_bits[32] = {
                                  /* A 16*16 array of bits, in the pattern: */
      0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55,   /* 1010101010101010 */
      0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55,   /* 0101010101010101 */
      0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55,   /* 1010101010101010 */
      0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55};  /* 0101010101010101 */
   static char unsigned icon_bitmap[] = {
#     define X11_ICON
#     include "SM_icon.h"
   };
   XColor cdef;
   XEvent event;
   XFontStruct *font_info;
   XGCValues gc_values;
   XrmDatabase args_db = NULL;		/* resources from command line args */
   static XrmDatabase rdb = NULL;	/* resources from args _and_ xrdb */
   XSetWindowAttributes win_atts;       /* struct to g/set window attributes */
   XSizeHints hints;                    /* hints for window manager */
   XWMHints wmhints;			/* More hints for WM */
   
   if(*s != '\0' && match_pattern(s,"-h.*",(char **)NULL) != NULL) {
      usage();
      return(-1);
   }
   
   if(dev_x11 == NULL) {
      if((dev_x11 = malloc(ndev_x11*sizeof(SMX11 *))) == NULL) {
	 msg("Cannot allocate list of structures for x11 devices\n");
	 return(-1);
      }
      
      for(i = 0;i < ndev_x11;i++) {
	 if((dev_x11[i] = alloc_smx11()) == NULL) {
	    while(--i >= 0) {
	       free((char *)dev_x11[i]);
	       dev_x11[i] = NULL;
	    }
	    free((char *)dev_x11);
	    return(-1);
	 }
      }
      
      which_dev_x11 = 0;		/* start with device 0 */
   }
   XDEV->is_complete = 0;
/*
 * process command line arguments, and look for a -device argument
 */
   X_get_opts_from_args(s, &args_db);

   ptr = get_resource(args_db,"device","");
   if(isdigit(*ptr)) {
      i = atoi(ptr);
   } else if(strcmp(ptr,"+") == 0) {
      for(i = which_dev_x11 + 1;i <= which_dev_x11 + ndev_x11;i++) {
	 if(dev_x11[i%ndev_x11]->wind > 0) {
	    i %= ndev_x11;
	    break;
	 }
      }
      if(sm_verbose) {
	 msg_1d("switching to X11 device %d\n",i);
      }
   } else if(strcmp(ptr,"-") == 0) {
      for(i = which_dev_x11 + ndev_x11 - 1;i >= which_dev_x11;i--) {
	 if(dev_x11[i%ndev_x11]->wind > 0) {
	    i %= ndev_x11;
	    break;
	 }
      }
      if(sm_verbose) {
	 msg_1d("switching to X11 device %d\n",i);
      }
   } else {
      i = which_dev_x11;
   }
   
   if(i < 0 || i >= ndev_x11) {
      msg_1d("DEVICE X11: only %d devices are available\n",ndev_x11);
      return(-1);
   }
   which_dev_x11 = i;
/*
 * proceed with opening windows as needed
 */
   if(XDEV->wind == 0) {
      XSetErrorHandler(hand_XError);
      XSetIOErrorHandler((int(*)())hand_XIOError);

      if(X_get_opts(s,args_db,&rdb) < 0) {
	 return(-1);
      }
      XDEV->xfd = ConnectionNumber(XDEV->display);   

      border_width = atoi(get_resource(rdb,"borderwidth","4"));
      backcolor = get_resource(rdb,"background","");
      want_curswindow = !atoi(get_resource(rdb,"nocurswindow","0"));
      XDEV->focus = atoi(get_resource(rdb,"kbdfocus","0"));
      strncpy(XDEV->forecolor,get_resource(rdb,"foreground","white"),39);
      iconic = atoi(get_resource(rdb,"iconic","0"));
      name = get_resource(rdb,"name",PROGNAME);
      sync = atoi(get_resource(rdb,"synchronise","0"));
      title = get_resource(rdb,"title","SM");
      want_cmap = atoi(get_resource(rdb,"colormap","0"));
/*
 * Now window and icon geometry
 */
      mask = XParseGeometry(get_resource(rdb,"geometry",""),
			    &xoff,&yoff,&xs,&ys);
      if((mask & XValue) && (mask & YValue)) x_position = 1;
      if(xs == 0) xs = 1;
      if(ys == 0) ys = 1;

      if(*(ptr = get_resource(rdb,"icongeometry","")) != '\0') {
	 unsigned int h,w;	/* not used */

	 mask = XParseGeometry(ptr,&icon_x,&icon_y,&w,&h);
	 if((mask & XValue) && (mask & YValue)) {
	    icon_position = 1;
	 }
      }
/*
 * Finished dealing with user options and argument string
 */
      screen = DefaultScreen(XDEV->display);
      if(sync) {
	 XSynchronize(XDEV->display,1);
      }
      XDEV->depth = DisplayPlanes(XDEV->display,screen);
      XDEV->root = RootWindow(XDEV->display,screen);
      root_height = DisplayHeight(XDEV->display,screen);
      root_width = DisplayWidth(XDEV->display,screen);
/*
 * If they asked us to, get a new colour map
 */
      if(!want_cmap) {
	 XDEV->cmap = DefaultColormap(XDEV->display,screen);
      } else {
	 Visual *visual = DefaultVisual(XDEV->display,screen);
	 if((cmap = XCreateColormap(XDEV->display,XDEV->root,
						      visual,AllocAll)) == 0) {
	    msg("Can't allocate private PsuedoColor colour map\n");
	    cmap = DefaultColormap(XDEV->display,screen);
	 }
	 XDEV->cmap = cmap;
      }
/*
 * try to get the background colour, but don't allocate a r/w cell for it
 * unless we have our own colour map
 */
      XDEV->backpix = BlackPixel(XDEV->display,screen);
      if(*backcolor != '\0' || *(backcolor = print_var("background")) != '\0'){
	 XColor exact;			/* exact colour from RGB database */
         int i;

         if((i = XLookupColor(XDEV->display,XDEV->cmap,backcolor,&exact,&cdef))
									== 0) {
            int r,g,b;                  /* failed - do it ourselves */
            if(parse_color(backcolor,&r,&g,&b) >= 0) {
               cdef.red = (r << 8);
               cdef.green = (g << 8);
               cdef.blue = (b << 8);
               i = 1;
            } else {
	       msg_1s("I don't understand the colour \"%s\" -- using black\n",
		      backcolor);
	    }
         }
         if(i != 0) {
	    if(XDEV->cmap != DefaultColormap(XDEV->display,screen)) {
	       cdef.pixel = 0;		/* use 0 for background */
	       cdef.flags = (DoRed | DoGreen | DoBlue);
	       if(XStoreColor(XDEV->display,XDEV->cmap,&cdef)) {
		  XDEV->backpix = cdef.pixel;
	       }
	    } else {
	       if(XAllocColor(XDEV->display,XDEV->cmap,&cdef)) {
		  XDEV->backpix = cdef.pixel;
	       }
	    }
	 }
      }
#ifdef DISABLE_BACKING
      XDEV->have_backing = 0;
#else
      XDEV->have_backing =
	(DoesBackingStore(ScreenOfDisplay(XDEV->display,screen)) == Always);
#endif

      XDEV->graphics_cursor = XCreateFontCursor(XDEV->display,XC_crosshair);
      icon_pixmap = XCreateBitmapFromData(XDEV->display,XDEV->root,
			    (char *)icon_bitmap, SM_ICON_XSIZE, SM_ICON_YSIZE);

      mask = 0;
      win_atts.background_pixel = XDEV->backpix;
      mask |= CWBackPixel;
      win_atts.border_pixmap =
	XCreatePixmapFromBitmapData(XDEV->display,XDEV->root,(char *)gray_bits,
				    16,16, BlackPixel(XDEV->display,screen),
				    WhitePixel(XDEV->display,screen),
				    XDEV->depth);
      mask |= CWBorderPixmap;
      if(XDEV->have_backing) {
	 win_atts.backing_store = Always;
	 mask |= CWBackingStore;
      }
      win_atts.colormap = XDEV->cmap;
      mask |= CWColormap;
/*
 * Finally create the window!
 */
      hints.flags = 0;
      hints.width = xs;
      hints.height = ys;
      hints.flags |= PSize;
      if(x_position) {
	 if(xoff < 0) {
	    xoff += root_width - 1 - hints.width - 2*border_width;
	 }
	 if(yoff < 0) {
	    yoff += root_height - 1 - hints.height - 2*border_width;
	 }
	 hints.x = xoff;
	 hints.y = yoff;
	 hints.flags |= USPosition;
      } else {
	 hints.x = hints.y = 100;
      }
      hints.min_width = hints.min_height = 100;
      hints.flags |= PMinSize;

      if(XDEV->wind == 0 &&
	 (XDEV->wind = XCreateWindow(XDEV->display,XDEV->root,hints.x,hints.y,
		        hints.width,hints.height,border_width,XDEV->depth,
                        InputOutput,CopyFromParent,mask,&win_atts)) == 0) {
         msg_1s("Can't open display %s\n",XDisplayName(s));
         return(-1);
      }

      XSetStandardProperties(XDEV->display,XDEV->wind,name,name,None,
			                             (char **)NULL,0,&hints);

      wmhints.flags = 0;
      wmhints.initial_state = iconic ? IconicState : NormalState;
      wmhints.flags |= StateHint;
      wmhints.icon_pixmap = icon_pixmap;
      wmhints.flags |= IconPixmapHint;
      wmhints.input = True;
      wmhints.flags |= InputHint;
      if(icon_position) {
	 if(icon_x < 0) {
	    icon_x += root_width - 1 - SM_ICON_XSIZE - 2*2;
	    				/* guess that icon border width is 2 */
	 }
	 if(icon_y < 0) {
	    icon_y += root_height - 1 - SM_ICON_YSIZE - 2*2;
	 }
	 wmhints.icon_x = icon_x;
	 wmhints.icon_y = icon_y;
	 wmhints.flags |= IconPositionHint;
      }
      XSetWMHints(XDEV->display,XDEV->wind,&wmhints);
      /*
       * express interest in window manager killing this window
       */
      if(del_atom == None) {
	 del_atom = XInternAtom(XDEV->display, "WM_DELETE_WINDOW", True);
      }
      if (del_atom != None) {
	 XSetWMProtocols(XDEV->display, XDEV->wind, &del_atom, 1);
      }

      {
	 char *s_title = malloc(strlen(title) + 10);
	 if(s_title == NULL) {		/* sigh */
	    s_title = title;
	 } else {
	    sprintf(s_title, "%s:%d", title, which_dev_x11);
	 }
	 XStoreName(XDEV->display,XDEV->wind,s_title);
      }
      XDEV->event_mask = ExposureMask | StructureNotifyMask | ClientMessage;
      XSelectInput(XDEV->display,XDEV->wind,XDEV->event_mask);
/*
 * We can now finish attaching attributes to the window
 * (some we needed at its creation)
 */
#if defined(VMS)
      fontname = "fixed";
#else
      fontname = "8x13";
#endif
	
      fontname = get_resource(rdb,"font",fontname);
      if((font_info = XLoadQueryFont(XDEV->display, fontname)) == NULL) {
	 msg_1s("Can't find font %s\n",fontname);
      } else {
         XDEV->h_font = font_info->max_bounds.ascent;
         XDEV->w_font = font_info->max_bounds.width;
         XDEV->font = font_info->fid;
      }

      GCmask = 0;
      gc_values.fill_style = FillSolid;
      GCmask |= GCFillStyle;
      gc_values.fill_rule = EvenOddRule;
      GCmask |= GCFillRule;
      gc_values.foreground = WhitePixel(XDEV->display,screen);
      default_ctype(XDEV->forecolor);	/* set_dev()'ll look in .sm */
      GCmask |= GCForeground;
      gc_values.background = XDEV->backpix;
      GCmask |= GCBackground;
      gc_values.font = XDEV->font;
      GCmask |= GCFont;

      XDEV->gc = XCreateGC(XDEV->display,XDEV->root,GCmask,&gc_values);
      XDEV->redraw_gc = XCreateGC(XDEV->display,XDEV->root,GCmask,&gc_values);
      gc_values.foreground = XDEV->backpix;
      XDEV->erase_gc = XCreateGC(XDEV->display,XDEV->root,GCmask,&gc_values);
/*
 * create display list for vectors
 */
      if(xvec == NULL) {
	 if((xvec = (XSegment *)malloc(NVEC*sizeof(XSegment))) == NULL) {
	    msg("Can't allocate vectors in x11_setup\n");
	    return(-1);
	 }
	 nvec = 0;
      }
/*
 * Find where the keyboard focus is if we need to know
 */
      if(XDEV->focus) {
	 XGetInputFocus(XDEV->display,&XDEV->focus,&XDEV->focus_revert);
      }
/*
 * create a cursor window if desired
 */
      if(want_curswindow) {
	 if((XDEV->curswind =
	     XCreateWindow(XDEV->display,XDEV->wind,
			   -border_width,-border_width,
			   19*XDEV->w_font + 2,XDEV->h_font + 3,2,XDEV->depth,
			   InputOutput,CopyFromParent, mask,&win_atts)) == 0) {
	    msg("Can't create cursor window\n");
	 }
      }
/*
 * Put up window for the first time
 */
      XMapWindow(XDEV->display,XDEV->wind);
      if(!iconic) {
	 XMaskEvent(XDEV->display,
		    StructureNotifyMask,&event); /* watch creation */
	 XPutBackEvent(XDEV->display,&event); /* and reschedule it */
      }
   } else {
      default_ctype(XDEV->forecolor);
   }
/*
 * look for ndevice now that we've read the xrdb database; we have to
 * do this outside the window-creation block
 */
   ptr = get_resource(rdb,"ndevice","");

   if(*ptr != '\0') {
      i = atoi(ptr);
      
      if(i > ndev_x11) {
	 int j;
	 
	 if(sm_verbose && ndev_x11 > NDEV_X11) {
	    msg_1d("Creating %d new X11 windows\n", i - ndev_x11);
	 }
	 if((dev_x11 = realloc(dev_x11,i*sizeof(SMX11 *))) == NULL) {
	    msg("Cannot reallocate list of structures for x11 devices\n");
	    return(-1);
	 }
	 
	 for(j = ndev_x11;j < i;j++) {
	    if((dev_x11[j] = alloc_smx11()) == NULL) {
	       while(--j >= 0) {
		  free((char *)dev_x11[j]);
		  dev_x11[j] = NULL;
	       }
	       free((char *)dev_x11);
	       return(-1);
	    }
	 }
	 ndev_x11 = i;
      }
   }
/*
 * raise the window and away we go
 */
   XRaiseWindow(XDEV->display,XDEV->wind);
   size_x11();
   _x11_redraw(0, which_dev_x11);
   /*
    * set focus to initial site
    */
   if(XDEV->focus) {
      XSetInputFocus(XDEV->display,XDEV->focus,XDEV->focus_revert,CurrentTime);
   }

   return(0);
}

/****************************************************************/

int
x11_char(s,x,y)
char *s;
int x,y;
{
   int len;

   if(g_sg->sg_starttext[0] == '\0') {	/* no hardware fonts */
      return(-1);
   }
   if(s == NULL) {
      if(aangle == 0.0 && eexpand == 1.0) {
	 cheight = XDEV->h_font/yscreen_to_pix;
	 cwidth = XDEV->w_font/xscreen_to_pix;
      }
      return(0);
   }
   x = x*xscreen_to_pix + 0.5;
   y = XDEV->height - (int)(y*yscreen_to_pix + 0.5) - 1;
   len = strlen(s);

   XDrawString(XDEV->display,XDEV->wind,XDEV->gc,x,y,s,len);
   if(!XDEV->have_backing) {
      XDrawString(XDEV->display,XDEV->backing,XDEV->gc,x,y,s,len);
   }
   return(0);
}
 
/****************************************************************/

void
x11_close()
{
   ;
}
 
/****************************************************************/

int
x11_cursor(get_pos,x,y)
int get_pos;				/* simply return the current position */
int *x,*y;
{
   char buff[5];
   int focus_revert;			/* how should the focus revert? */
   int kx,ky,				/* cursor position in keyboard wind */
       rx,ry;				/* cursor position in root wind */
   extern int sm_image_cursor;		/* is this an image cursor? */
   unsigned int button_status;		/* not used */
   Window focus,			/* window with initial focus  */
          child_wind,			/* the values of these Windows */
          root_wind;			/* 	          are not used */
   XEvent event;

   if(get_pos) {
      XQueryPointer(XDEV->display,XDEV->wind,&root_wind,&child_wind,&rx,&ry,
					   &XDEV->cx,&XDEV->cy,&button_status);
      *x = XDEV->cx/xscreen_to_pix + 0.5;
      *y = (XDEV->height - XDEV->cy - 1)/yscreen_to_pix + 0.5;

      return 0;
   }

   XQueryPointer(XDEV->display,XDEV->root,&root_wind,&child_wind,&rx,&ry,
						     &kx,&ky,&button_status);
   XDefineCursor(XDEV->display,XDEV->wind,XDEV->graphics_cursor);

   if(XDEV->cx < 0) {
      XDEV->cx = XDEV->width/2;
      XDEV->cy = XDEV->height/2;
   }
   XGetInputFocus(XDEV->display,&focus,&focus_revert);
   XSetInputFocus(XDEV->display,XDEV->wind,focus_revert,CurrentTime);
   XWarpPointer(XDEV->display,None,XDEV->wind,0,0,0,0,XDEV->cx,XDEV->cy);

   while(XCheckTypedEvent(XDEV->display,ButtonPress,&event) ||
	 XCheckTypedEvent(XDEV->display,KeyPress,&event)) {
      continue;				/* throw away old key/button events */
   }
   XDEV->event_mask |= ButtonPressMask|KeyPressMask;
   if(XDEV->curswind != 0) {
      if(sm_image_cursor != XDEV->old_sm_image_cursor) {
	 int width = sm_image_cursor ?
				     30*XDEV->w_font + 2 : 19*XDEV->w_font + 2;
	 XDEV->old_sm_image_cursor = sm_image_cursor;
	 XResizeWindow(XDEV->display, XDEV->curswind, width, XDEV->h_font + 3);
      }

      XMapWindow(XDEV->display,XDEV->curswind);
      XDEV->event_mask |= PointerMotionMask;
   }
   XSelectInput(XDEV->display,XDEV->wind,XDEV->event_mask);
   for(;;) {
      event = *_x11_redraw(1, which_dev_x11); /* wait for desired events */
      if(event.type == ButtonPress || event.type == KeyPress) {
	 break;
      } else if(event.type == MotionNotify) {
	 float ux,uy;			/* user coordinates */
	 int px,py;			/* SCREEN coords of cursor */
	 char string[60];

	 px = event.xmotion.x/xscreen_to_pix + 0.5;
	 py = (XDEV->height - event.xmotion.y - 1)/yscreen_to_pix + 0.5;

	 ux = (px - ffx)/fsx;			/* SCREEN --> user coords */
	 uy = (py - ffy)/fsy;

	 if(sm_image_cursor) {
	    float val;
	    if((val = image_val(ux,uy)) < NO_VALUE) {
	       sprintf(string," %-8g %-8g %10g           ",ux,uy,val);
	    } else {
	       sprintf(string," %-8g %-8g ***           ",ux,uy);
	    }
	 } else {
	    sprintf(string," %-8g %-8g ",ux,uy);
	 }
	 XDrawImageString(XDEV->display,XDEV->curswind,XDEV->gc,0,
			  XDEV->h_font + 1,string, sizeof(string));
      }
   }
   XDEV->event_mask &= ~(ButtonPressMask|KeyPressMask|PointerMotionMask);
   XSelectInput(XDEV->display,XDEV->wind,XDEV->event_mask);

   XUndefineCursor(XDEV->display,XDEV->wind);
   XQueryPointer(XDEV->display,XDEV->wind,&root_wind,&child_wind,&rx,&ry,
					   &XDEV->cx,&XDEV->cy,&button_status);
   XSetInputFocus(XDEV->display,focus,focus_revert,CurrentTime);
   XWarpPointer(XDEV->display,None,XDEV->root,0,0,0,0,kx,ky);
   if(XDEV->curswind != 0) {
      XUnmapWindow(XDEV->display,XDEV->curswind);
   }

   XFlush(XDEV->display);			/* make the pointer move NOW */

   *x = XDEV->cx/xscreen_to_pix + 0.5;
   *y = (XDEV->height - XDEV->cy - 1)/yscreen_to_pix + 0.5;

   if(event.type == ButtonPress) {
      switch (event.xbutton.button) {
       case Button1:
	 return('e');
       case Button2:
	 return('p');
       case Button3:
	 return('q');
       default:
	 return('p');
      }
   }

   XLookupString((XKeyEvent *)&event,buff,sizeof(buff),(KeySym *)NULL,
		 				(XComposeStatus *)NULL);
   return(buff[0]);
}

/****************************************************************/

void
x11_draw(x,y)
int x,y;
{
   x11_line(xp,yp,x,y);
}

/****************************************************************/

void
x11_erase(int raise_on_erase)
{
   nvec = 0;

   XClearWindow(XDEV->display,XDEV->wind);
   if(!XDEV->have_backing) {
      x_clear_pixmap(XDEV->display,XDEV->backing);
   }

   if(raise_on_erase) {
      XMapWindow(XDEV->display,XDEV->wind);
      
      XRaiseWindow(XDEV->display,XDEV->wind);
   }
}

/****************************************************************/

int
x11_fill_pt(n)
int n;                          /* number of sides */
{
   float dtheta, theta;
   static float old_ang;	/* old values of angle */
   int i,
       xpsize,			/* scale for points */
       ypsize;
   static int num = -1,         /* number of vertices used last time */
	      old_xp,old_yp;	/* old values of xpsize, ypsize */
   XPoint vlist[MAX_POLYGON + 1];	/* corners of point */
   static XPoint vlist0[MAX_POLYGON + 1]; /* corners of point at (0,0) */

   if(n < 2) {
      x11_line(xp,yp,xp,yp);
      return(0);
   }

   dtheta = 2*PI/n;
   xpsize = ypsize = PDEF*eexpand;
   if(aspect > 1.0) {                   /* allow for aspect ratio of screen */
      ypsize /= aspect;
   } else if(aspect < 1.0) {
      xpsize *= aspect;
   }

   if(n != num || aangle != old_ang || xpsize != old_xp || ypsize != old_yp) {
      if(n > MAX_POLYGON) num = MAX_POLYGON;
      else num = n;

      theta = 3*PI/2 + dtheta/2 + aangle*PI/180;

      old_ang = aangle;
      old_xp = xpsize;
      old_yp = ypsize;

      for(i = 0;i < num;i++) {
	 vlist0[i].x = xscreen_to_pix*xpsize*cos(theta) + 0.5;
	 vlist0[i].y = yscreen_to_pix*(SCREEN_SIZE - ypsize*sin(theta)) + 0.5;
	 vlist[i].x = vlist0[i].x + xscreen_to_pix*xp + 0.5;
	 vlist[i].y = vlist0[i].y - yscreen_to_pix*yp + 0.5;
	 theta += dtheta;
      }
   } else {				/* no need for more trig. */
      for(i = 0;i < num;i++) {
	 vlist[i].x = vlist0[i].x + xscreen_to_pix*xp + 0.5;
	 vlist[i].y = vlist0[i].y - yscreen_to_pix*yp + 0.5;
      }
   }
   vlist[i].x = vlist[0].x;
   vlist[i].y = vlist[0].y;
/*
 * We need the bounding polygon as well as fill to get all of the edges
 */
   XFillPolygon(XDEV->display,XDEV->wind,XDEV->gc,vlist,num,
		Convex,CoordModeOrigin);
   XDrawLines(XDEV->display,XDEV->wind,XDEV->gc,vlist,num + 1,CoordModeOrigin);
   if(!XDEV->have_backing) {
      XFillPolygon(XDEV->display,XDEV->backing,XDEV->gc,vlist,num,
		   Convex,CoordModeOrigin);
      XDrawLines(XDEV->display,XDEV->backing,XDEV->gc,vlist,
		 num + 1,CoordModeOrigin);
   }
   return(0);
}

/*****************************************************************************/

int
x11_fill_polygon(style,x,y,n)
int style;
short *x,*y;
int n;
{
   int i;
   XPoint *vlist;			/* vertices of polygon */

   if(style != FILL_SOLID) {
      msg_1d("X11 driver doesn't support shade style %d\n",style);
      return(-1);
   }
   if(n == 0) return(0);

   if((vlist = (XPoint *)malloc((n + 1)*sizeof(XPoint))) == NULL) {
      msg("Can't allocate storage for vlist\n");
      return(-1);
   }

   for(i = 0;i < n;i++) {
      vlist[i].x = xscreen_to_pix*x[i] + 0.5;
      vlist[i].y = yscreen_to_pix*(SCREEN_SIZE - y[i]) + 0.5;
   }
   vlist[i].x = vlist[0].x;
   vlist[i].y = vlist[0].y;
   
   XFillPolygon(XDEV->display,XDEV->wind,XDEV->gc,vlist,n,
		Complex,CoordModeOrigin);
   XDrawLines(XDEV->display,XDEV->wind,XDEV->gc,vlist,n + 1,CoordModeOrigin);
   if(!XDEV->have_backing) {
      XFillPolygon(XDEV->display,XDEV->backing,XDEV->gc,vlist,n,
		   Convex,CoordModeOrigin);
      XDrawLines(XDEV->display,XDEV->backing,XDEV->gc,vlist,n + 1,
		 CoordModeOrigin);
   }

   free((char *)vlist);
   return(0);
}

/****************************************************************/

int
x11_dot(x,y)
int x,y;
{
   int x1, x2;
   int dx = llweight/(2*xscreen_to_pix);

   x1 = x + dx; x2 = x - dx;

   x1 = x1*xscreen_to_pix + 0.5;
   x2 = x2*xscreen_to_pix + 0.5;
   y = XDEV->height - (int)(y*yscreen_to_pix + 0.5) - 1;

   if(x1 == x2) x2++;

   xvec[nvec].x1 = x1;
   xvec[nvec].y1 = y;
   xvec[nvec].x2 = x2;
   xvec[nvec].y2 = y;
   nvec++;
   if(nvec > NVEC - 1) {
      XDrawSegments(XDEV->display,XDEV->wind,XDEV->gc,xvec,nvec);
      if(!XDEV->have_backing) {
	 XDrawSegments(XDEV->display,XDEV->backing,XDEV->gc,xvec,nvec);
      }
      nvec = 0;
   }
   
   return(0);
}

/****************************************************************/

void
x11_gflush()
{
   XDrawSegments(XDEV->display,XDEV->wind,XDEV->gc,xvec,nvec);
   if(!XDEV->have_backing) {
      XDrawSegments(XDEV->display,XDEV->backing,XDEV->gc,xvec,nvec);
   }
   XFlush(XDEV->display);

#if defined(TK)
   smtk_DoOneEvent(XDEV->display);
#endif

   nvec = 0;
}

/****************************************************************/

void
x11_idle()
{
   ;
}

/****************************************************************/

void
x11_line(x1,y1,x2,y2)
int x1,y1,x2,y2;
{
   x1 = x1*xscreen_to_pix + 0.5;
   x2 = x2*xscreen_to_pix + 0.5;
   y1 = XDEV->height - (int)(y1*yscreen_to_pix + 0.5) - 1;
   y2 = XDEV->height - (int)(y2*yscreen_to_pix + 0.5) - 1;
   xvec[nvec].x1 = x1;
   xvec[nvec].y1 = y1;
   xvec[nvec].x2 = x2;
   xvec[nvec].y2 = y2;
   nvec++;
   if(nvec > NVEC - 1) {
      XDrawSegments(XDEV->display,XDEV->wind,XDEV->gc,xvec,nvec);
      if(!XDEV->have_backing) {
	 XDrawSegments(XDEV->display,XDEV->backing,XDEV->gc,xvec,nvec);
      }
      nvec = 0;
   }
}

/****************************************************************/
/*
 * Only support erasing of lines (lt 10 and 11)
 */

int
x11_ltype(lt)
int lt;
{
   if(lt == 10) {
      if(get_foreground(XDEV->gc) != XDEV->backpix) {
	 XDEV->old_forepix = get_foreground(XDEV->gc);
      }
      XSetForeground(XDEV->display,XDEV->gc,XDEV->backpix);
      return(0);
   } else if(lt == 11) {
      XSetForeground(XDEV->display,XDEV->gc,XDEV->old_forepix);
      return(0);
   }
   return(-1);
}

/****************************************************************/

int
x11_lweight(lw)
double lw;
{
   XGCValues gc_values;
   
   if(lw > 1.0) {
      lw--;				/* lw == 0 is special */
   }

   gc_values.line_width = lw + 0.5;
   XChangeGC(XDEV->display,XDEV->gc,GCLineWidth,&gc_values);
   return(0);
}

/****************************************************************/

void
x11_page()
{
   XMapWindow(XDEV->display,XDEV->wind);
   XRaiseWindow(XDEV->display,XDEV->wind);
   size_x11();
   _x11_redraw(0, which_dev_x11);
}

/****************************************************************/

void
x11_redraw(fd)
int fd;					/* f.d. that get1char is waiting on */
{
#if defined(NO_SELECT)
   _x11_redraw(0, which_dev_x11);
#else
   fd_set mask;				/* mask of descriptors to check */
   int ncheck;				/* number of descriptors to check */
   int i;
   fd_set readmask;			/* the desired mask */

   if(fd < 0) {				/* no input on fd is possible */
      _x11_redraw(0, which_dev_x11);
      return;
   }

   FD_ZERO(&readmask);
   FD_SET(fd,&readmask);
   ncheck = fd;
   for(i = 0;i < ndev_x11;i++) {
      if(dev_x11[i]->xfd >= 0) {
	 FD_SET(dev_x11[i]->xfd,&readmask);
	 if(dev_x11[i]->xfd > ncheck) {
	    ncheck = dev_x11[i]->xfd;
	 }
      }
   }
   ncheck++;				/* as an fd of 0 => check 1 fd */
   
   for(;;) {				/* wait for input to be ready */
      mask = readmask;
      if(select(ncheck,&mask,(fd_set *)NULL,(fd_set *)NULL,(void *)NULL) != 0){
	 for(i = 0;i < ndev_x11;i++) {
	    if(dev_x11[i]->xfd >= 0 && FD_ISSET(dev_x11[i]->xfd,&mask)) {
	       _x11_redraw(0, i);
	    }
	 }
	 if(FD_ISSET(fd,&mask)) {
	    return;
	 }
      }
   }
#endif
}

static XEvent *
_x11_redraw(block, which)
int block;
int which;				/* which SMX11 to process */
{
   long reduced_mask;			/* mask without button/key events */
   Region region;			/* Region to re-draw */
   static XEvent event;
   XRectangle rect;
   int save_which_dev_x11 = which_dev_x11;

   which_dev_x11 = which;
/*
 * we want to process all expose events before even thinking about
 * button/key masks. Only when we are up-to-date does the block
 * flag become relevent
 */
   reduced_mask = XDEV->event_mask &
			     ~(ButtonPressMask|KeyPressMask|PointerMotionMask);

   while(XCheckMaskEvent(XDEV->display,reduced_mask,&event) ||
	 XCheckMaskEvent(XDEV->display,XDEV->event_mask,&event) ||	 
	 (block && (XNextEvent(XDEV->display,&event),1))) {
      switch (event.type) {
       case NoExpose:
       case GraphicsExpose:
	 break;
       case Expose:
	 if(XDEV->have_backing) {	/* contents lost (bit_gravity
					   may not be implemented) */
	    continue;
	 }
	 region = XCreateRegion();
	 do {
	    rect.x = event.xexpose.x;
	    rect.y = event.xexpose.y;
	    rect.height = event.xexpose.height;
	    rect.width = event.xexpose.width;
	    XUnionRectWithRegion(&rect,region,region);
	 } while(XCheckTypedEvent(XDEV->display,Expose,&event)) ;
	 XSetRegion(XDEV->display,XDEV->redraw_gc,region);
	 
	 XCopyArea(XDEV->display,XDEV->backing,XDEV->wind,XDEV->redraw_gc,0,0,
		   XDEV->width,XDEV->height,0,0);
	 XDestroyRegion(region);
	 break;
       case ConfigureNotify:
	 do_configure_notify();
	 break;
       case MotionNotify:		/* only in cursor mode + curswind */
       case ButtonPress:		/* only in mask if in cursor mode */
       case KeyPress:
	 which_dev_x11 = save_which_dev_x11;
	 return(&event);
      case ClientMessage:
	if(event.xclient.data.l[0] == del_atom) {
	   exit(0);
	}
	break;
      default:
	 continue;
      }
   }
   XFlush(XDEV->display);

   which_dev_x11 = save_which_dev_x11;
   return(&event);
}

/****************************************************************/

void
x11_reloc(x,y)
int x,y;
{
   ;
}

/****************************************************************/

int
x11_set_ctype(colors,ncolors)
COLOR *colors;
int ncolors;
{
   XColor cdef;
   int i, j;
   int screen = DefaultScreen(XDEV->display);
   
   if(XDEV->cmap != DefaultColormap(XDEV->display,screen)) {
      int cmap_size;			/* size of colour map */

      if(colors == NULL) {
	 return(0);
      }
      
      cdef.flags = (DoRed | DoGreen | DoBlue);

      cmap_size = DisplayCells(XDEV->display, screen);
      if(ncolors >= cmap_size) {
	 float f;			/* interpolation weight */
	 float sq;			/* factor to squash by */
	 int r, g, b;			/* an interpolated colour */
	 
	 XDEV->cmap_sq = sq = (float)(cmap_size - 1)/(ncolors - 1)*0.999999;
	 for(i = 0;i < cmap_size - 1;i++) {
	    j = i/sq;
	    f = i/sq - j;
	    r = colors[j].red*(1 - f) + colors[j+1].red*f;
	    g = colors[j].green*(1 - f) + colors[j+1].green*f;
	    b = colors[j].blue*(1 - f) + colors[j+1].blue*f;

	    cdef.pixel = i + 1;
	    cdef.red = r << 8;
	    cdef.green = g << 8;
	    cdef.blue = b << 8;
	    
	    XStoreColor(XDEV->display,XDEV->cmap,&cdef);
	 }
      } else {
	 XDEV->cmap_sq = 1;
	 for(i = 0;i < ncolors;i++) {
	    cdef.pixel = i + 1;
	    cdef.red = colors[i].red << 8;
	    cdef.green = colors[i].green << 8;
	    cdef.blue = colors[i].blue << 8;
	    
	    XStoreColor(XDEV->display,XDEV->cmap,&cdef);
	 }
      }
      return(0);
   } else {
      return(-1);
   }
}

/****************************************************************/

void
x11_ctype(r,g,b)
int r,g,b;
{
   int screen = DefaultScreen(XDEV->display);
   XColor cdef;
   
   cdef.flags = (DoRed | DoGreen | DoBlue);
   if(XDEV->cmap != DefaultColormap(XDEV->display,screen)) {
      cdef.pixel = XDEV->cmap_sq*r + 1;
   } else {
      cdef.red = r << 8;
      cdef.green = g << 8;
      cdef.blue = b << 8;
      
      if(XAllocColor(XDEV->display,XDEV->cmap,&cdef) == 0) {
	 msg_1d("Can't allocate colour (r,g,b) = (%d,",r);
	 msg_1d("%d,",g);
	 msg_1d("%d)\n",b);
      }
   }
   
   x11_gflush();
   XSetForeground(XDEV->display,XDEV->gc,cdef.pixel);
}

/****************************************************************/
/*
 * Static utilities:
 */
static SMX11 *
alloc_smx11()
{
   SMX11 *newdev;
   
   if((newdev = malloc(sizeof(SMX11))) == NULL) {
      msg("Cannot allocate structure for device x11\n");
      return(NULL);
   }
   newdev->display = NULL;
   newdev->xfd = -1;
   newdev->font = (Font)(-1);
   newdev->cx = -1;
   newdev->old_forepix = -1;
   newdev->old_height = -1;
   newdev->height = 0;
   newdev->curswind = 0;
   newdev->focus = 0;
   newdev->wind = 0;

   return(newdev);
}

static void
do_configure_notify()
{
   Pixmap new_backing;

   size_x11();
   if(XDEV->old_height != XDEV->height || XDEV->old_width != XDEV->width) {
      if(!XDEV->have_backing) {
	 new_backing = XCreatePixmap(XDEV->display,XDEV->wind,
				     XDEV->width,XDEV->height,
				     XDEV->depth);
	 x_clear_pixmap(XDEV->display,new_backing);
	 if(XDEV->old_height >= 0) {
	    int dest_x,dest_y,
	    src_x,src_y;
	    unsigned int wwidth,hheight;
	    
	    if(XDEV->width > XDEV->old_width) {
	       wwidth = XDEV->old_width;
	       src_x = 0;
	       dest_x = (XDEV->width - XDEV->old_width)/2;
	    } else {
	       wwidth = XDEV->width;
	       src_x = (XDEV->old_width - XDEV->width)/2;
	       dest_x = 0;
	    }
	    if(XDEV->height > XDEV->old_height) {
	       hheight = XDEV->old_height;
	       src_y = 0;
	       dest_y = (XDEV->height - XDEV->old_height)/2;
	    } else {
	       hheight = XDEV->height;
	       src_y = (XDEV->old_height - XDEV->height)/2;
	       dest_y = 0;
	    }
	    XCopyArea(XDEV->display,XDEV->backing,new_backing,XDEV->gc,
		      src_x,src_y,wwidth,hheight,dest_x,dest_y);
	    XFreePixmap(XDEV->display,XDEV->backing);
	 }
	 XDEV->backing = new_backing;
      }
      XDEV->old_height = XDEV->height;
      XDEV->old_width = XDEV->width;
   }
}

static int
get_foreground(g)
GC g;					/* the GC */
{
   XGCValues gcv;
   
   (void)XGetGCValues(XDEV->display,g,GCForeground,&gcv);
   return(gcv.foreground);
}

static void
X_get_opts_from_args(str, args_db)
char *str;
XrmDatabase *args_db;			/* resources from command line */
{
   int argc;
   char *argv[1 + NARG];		/* leave room for programme's name */
   
   argc = NARG;				/* max number of arguments */
   (void)str_to_argv(str,&argv[1],&argc);
   argv[0] = PROGNAME; argc++;
   XrmInitialize();
   
   XrmParseCommand(args_db,options,NOPT,PROGNAME,&argc,argv);
   if(argc != 1) {
      int i;

      msg("Unknown arguments to DEVICE command:");
      for(i = 1;i < argc;i++) {
	 msg_1s(" %s",argv[i]);
      }
      msg("\n");
      usage();
   }
}

#define WORKAROUND_XOPENDISPLAY_ABORT 1

static jmp_buf env;			/* for longjmp; only used if WORKAROUND_XOPENDISPLAY_ABORT is true */

#if WORKAROUND_XOPENDISPLAY_ABORT
static void hand_XIOErrorLongjmp(int sig) {
   assert (sig >= 0);			/* keep compiler happy */
   longjmp(env,1);
}
#endif

static int
X_get_opts(str, args_db, rdb)
char *str;
XrmDatabase args_db;			/* resources from command line */
XrmDatabase *rdb;			/* merged database */
{
   char *ptr;
   XrmDatabase xrdb_db;			/* and those managed by xrdb */
   
   if(*(ptr = get_resource(args_db,"preopened","")) != '\0') {
      if(str_scanf(ptr,"%d:%d",&XDEV->display,&XDEV->wind) != 2) {
	 XDEV->wind = 0;
	 XDEV->display = NULL;
	 msg("You need display:window with \"-p[reopened]\"\n");
      }
   }
   
   if(XDEV->display == NULL) {
#if WORKAROUND_XOPENDISPLAY_ABORT
      XSetIOErrorHandler((int(*)())hand_XIOErrorLongjmp);
#endif
      int jmpVal;
      ptr = get_resource(args_db,"display",(char *)NULL);
      
      if ((jmpVal = setjmp(env)) == 0) { /* 0 => preparing for longjmp */
	 XDEV->display = XOpenDisplay(ptr);
      }

#if WORKAROUND_XOPENDISPLAY_ABORT
      XSetIOErrorHandler((int(*)())hand_XIOError);
#endif

      if(jmpVal > 0 || XDEV->display == NULL) {
	 msg_1s("Could not open display \"%s\"\n", XDisplayName(ptr));
	 return(-1);
      }
   }
   
   if((ptr = XResourceManagerString(XDEV->display)) != NULL) {
      xrdb_db = XrmGetStringDatabase(ptr);
      XrmMergeDatabases(xrdb_db,rdb);
   }
   XrmMergeDatabases(args_db,rdb);
      
   return(0);
}

static char *
get_resource(db,name,def_val)
XrmDatabase db;
char *name;
char *def_val;				/* default value */
{
   char fullname[50];
   char *option_type;			/* the type of the resource */
   static XrmValue value;

   sprintf(fullname,"%s.%s",PROGNAME,name);
   if(XrmGetResource(db,fullname,"",&option_type,&value) == True) {
      return(value.addr);
   } else {
      return(def_val);
   }
}

static void
usage()
{
   int i;

   msg("Legal options are:\n");
   msg("\t-help\n");
   for(i = 0;i < NOPT;i++) {
      msg_2s("\t%-40s (xrdb: %s)\n",options[i].option,options[i].specifier);
   }
}

static int
hand_XError(disp,event)
Display *disp;
XErrorEvent *event;
{
   char str[80];

   XGetErrorText(disp,event->error_code,str,80);
   msg_1s("X Error: %s\n",str);
   return(0);
}

/****************************************************************/

static void
hand_XIOError(disp)
Display *disp;
{
#ifdef lint
   *disp++;
#endif
   XDEV->wind = 0;
   devnum = 0;
   set_dev();
   msg("Fatal X Error: closing device\n");
   hand_q(0);
}

/****************************************************************/

static void
size_x11()
{
   int x,y;
   unsigned int b_width;
   Window root_w;

   XGetGeometry(XDEV->display,XDEV->wind,&root_w,&x,&y,
		&XDEV->width,&XDEV->height,&b_width,&XDEV->depth);
   xscreen_to_pix = XDEV->width/(float)SCREEN_SIZE;
   yscreen_to_pix = XDEV->height/(float)SCREEN_SIZE;
   aspect = (float)(XDEV->height)/XDEV->width;
   ldef = 1/xscreen_to_pix;
}

/****************************************************************/

static void
x_clear_pixmap(disp,pixmap)
Display *disp;
Pixmap pixmap;
{
   XFillRectangle(disp,pixmap,XDEV->erase_gc,0,0,
		  XDEV->width + 1,XDEV->height + 1);
}

/*****************************************************************************/
/*
 * Now the TK support for SM
 */
#if defined(TK)
#include "tk.h"

/*
 * A data structure of the following type is kept for each square
 * widget managed by this file:
 */
typedef struct {
   Tk_Window tkwin;			/* Tk window for plot. NULL means
					   window has been deleted but widget
					   record hasn't been freed */
   Display *display;			/* X display. */
   Tcl_Interp *interp;			/* widget's interpreter */

   int dev;				/* device number for this SM widget */
   int xsize, ysize;			/* configured size of widget */

   int borderWidth;			/* Width of 3-D border around widget */
   Tk_3DBorder bgBorder;		/* Used for drawing background */
   int relief;				/* Indicates whether window as a whole
					   is raised, sunken, or flat. */
} SM_record;

/*
 * Information used for argv parsing.
 */
static Tk_ConfigSpec configSpecs[] = {
   {TK_CONFIG_PIXELS, "-xsize", "xsize", "Xsize",
      "512", Tk_Offset(SM_record, xsize), 0},
   {TK_CONFIG_SYNONYM, "-xs", "xsize", (char *) NULL,
      (char *) NULL, 0, 0},
   {TK_CONFIG_PIXELS, "-ysize", "ySize", "Ysize",
      "512", Tk_Offset(SM_record, ysize), 0},
   {TK_CONFIG_SYNONYM, "-ys", "ysize", (char *) NULL,
      (char *) NULL, 0, 0},
#if 0
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	"#cdb79e", Tk_Offset(SM_record, bgBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	"white", Tk_Offset(SM_record, bgBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	"2", Tk_Offset(SM_record, borderWidth), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	"raised", Tk_Offset(SM_record, relief), 0},
#endif
   {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *)NULL,
      (char *)NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		smtk_CmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		smtk_Configure _ANSI_ARGS_((Tcl_Interp *interp,
			    SM_record *tk_data, int ac, char **av,
			    int flags));
static void		smtk_Destroy _ANSI_ARGS_((char *clientData));
static void		smtk_Display _ANSI_ARGS_((ClientData clientData));
static void		smtk_EventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		smtk_WidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *, int ac, char **av));

/*
 *
 * This procedure is invoked to process the SM Tcl command, creating a new
 * widget.
 *
 * Returns TCL_OK/TCL_ERROR
 */
int
sm_create_tk(ClientData clientData,	/* interpreter's main window */
	     Tcl_Interp *interp,	/* Current interpreter */
	     int ac,			/* number of arguments */
	     char **av)			/* Argument strings */
{
    Tk_Window mainw = (Tk_Window) clientData;
    static int nwidget = 0;			/* number of SM widgets */
    SM_record *tk_data;
    Tk_Window tkwin;

    if (ac < 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		av[0], " pathName ?options?\"", (char *) NULL);
	return(TCL_ERROR);
    }

    tkwin = Tk_CreateWindowFromPath(interp, mainw, av[1], (char *) NULL);
    if (tkwin == NULL) {
	return(TCL_ERROR);
    }
    Tk_SetClass(tkwin, "SM");

    /*
     * Allocate and initialize the widget record.
     */

    tk_data = (SM_record *)malloc(sizeof(SM_record));
    tk_data->tkwin = tkwin;
    tk_data->display = Tk_Display(tkwin);
    tk_data->interp = interp;
    Tcl_CreateCommand(interp, Tk_PathName(tk_data->tkwin), smtk_WidgetCmd,
			(ClientData)tk_data, smtk_CmdDeletedProc);
    tk_data->borderWidth = 0;
    tk_data->bgBorder = NULL;
    tk_data->relief = TK_RELIEF_FLAT;
    tk_data->dev = -1;

    if (smtk_Configure(interp, tk_data, ac-2, av+2, 0) != TCL_OK) {
	Tk_DestroyWindow(tk_data->tkwin);
	return TCL_ERROR;
    }
/*
 * register handlers etc.
 */
    Tk_CreateEventHandler(tk_data->tkwin, ExposureMask|StructureNotifyMask,
			  smtk_EventProc, (ClientData)tk_data);
/*
 * actually create the SM device
 */
    Tk_MakeWindowExist(tkwin);

    {
       char buff[100];
       
       sprintf(buff,"x11 -preopened %ld:%ld -device %d",
	       (long)Tk_Display(tkwin),Tk_WindowId(tkwin), nwidget);

       if(set_device(buff) != 0) {
	  Tcl_AppendResult(interp, "failed to open SM widget", (char *) NULL);
	  smtk_Destroy((char *)tk_data);
	  return(TCL_ERROR);
       }
       ENABLE(); set_dev(); IDLE();

       tk_data->dev = nwidget++;
       do_configure_notify();
    }

    XDEV->is_complete = 1;

    interp->result = Tk_PathName(tk_data->tkwin);
    return TCL_OK;
}

/*****************************************************************************/
/*
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 */
static int
smtk_WidgetCmd(ClientData clientData,	/* Information about SM widget */
	       Tcl_Interp *interp,	/* Current interpreter. */
	       int ac,			/* Number of arguments. */
	       char **av)		/* Argument strings. */
{
    SM_record *tk_data = (SM_record *) clientData;
    int result = TCL_OK;
    int length;

    if (ac < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		av[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tk_Preserve((ClientData) tk_data);
/*
 * if needs be, switch SM devices
 */
    if(tk_data->dev != which_dev_x11) {
       char buff[40];
       sprintf(buff,"-device %d", tk_data->dev);
       x11_setup(buff);
    }

    length = strlen(av[1]);
    if(strncmp(av[1], "cget", length) == 0 && length >= 2) {
       if(ac != 3) {
	  Tcl_AppendResult(interp, "wrong # args: should be \"",
			   av[0], " cget option\"", (char *) NULL);
	  Tk_Release((ClientData)tk_data);
	  return(TCL_ERROR);
       }
       result = Tk_ConfigureValue(interp, tk_data->tkwin, configSpecs,
				  (char *)tk_data, av[2], 0);
    } else if (strncmp(av[1], "configure", length) == 0 && length >= 3) {
	if (ac == 2) {
	    result = Tk_ConfigureInfo(interp, tk_data->tkwin, configSpecs,
		    (char *) tk_data, (char *) NULL, 0);
	} else if (ac == 3) {
	    result = Tk_ConfigureInfo(interp, tk_data->tkwin, configSpecs,
		    (char *) tk_data, av[2], 0);
	} else {
	    result = smtk_Configure(interp, tk_data, ac-2, av+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if(strncmp(av[1], "select", length) == 0 && length >= 3) {
       size_x11();
    } else {
	Tcl_AppendResult(interp, "bad option \"", av[1],
			 "\" for sm widget", (char *) NULL);
	Tk_Release((ClientData) tk_data);
	return TCL_ERROR;
    }
    Tk_DoWhenIdle(smtk_Display, (ClientData) tk_data);

    Tk_Release((ClientData) tk_data);
    return result;
}

/*****************************************************************************/
/*
 *	This procedure is called to process an av/ac list in
 *	conjunction with the Tk option database to configure (or
 *	reconfigure) a square widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for tk_data;  old resources get freed,
 *	if there were any.
 *
 */
static int
smtk_Configure(Tcl_Interp *interp,	/* Used for error reporting */
	     SM_record *tk_data,	/* Information about widget */
	     int ac,			/* Number of valid entries in av. */
	     char **av,			/* Arguments */
	     int flags)			/* flags for Tk_ConfigureWidget */
{
   if(Tk_ConfigureWidget(interp, tk_data->tkwin, configSpecs,
			 ac, av, (char *) tk_data, flags) != TCL_OK) {
      return TCL_ERROR;
   }
/*
 * Register the desired geometry for the window.  Then arrange for
 * the window to be redisplayed.
 */
   Tk_GeometryRequest(tk_data->tkwin, tk_data->xsize, tk_data->ysize);
   Tk_DoWhenIdle(smtk_Display, (ClientData) tk_data);
   
   return TCL_OK;
}

/*****************************************************************************/
/*
 * This procedure is invoked by the Tk dispatcher for events on SM widgets.
 */
static void
smtk_EventProc(ClientData clientData,	/* Information about window */
	       XEvent *eventPtr)	/* Information about event */
{
   SM_record *tk_data = (SM_record *)clientData;
   XEvent event = *eventPtr;
   int save_which_dev_x11 = which_dev_x11;

   if(tk_data->dev < 0) {
      return;
   }

   which_dev_x11 = tk_data->dev;	/* the window of interest */

   switch (event.type) {
    case Expose:
      if(XDEV->have_backing) {		/* contents lost (bit_gravity
					   may not be implemented) */
	 break;
      }
      Tk_DoWhenIdle(smtk_Display, (ClientData) tk_data);
      break;
    case ConfigureNotify:
      do_configure_notify();
      break;
    case DestroyNotify:
      if (tk_data->tkwin != NULL) {
	 char *name = Tk_PathName(tk_data->tkwin);
	 tk_data->tkwin = NULL;
	 Tcl_DeleteCommand(tk_data->interp, name);
      }
      Tk_CancelIdleCall(smtk_Display, (ClientData) tk_data);
      
      Tk_EventuallyFree((ClientData) tk_data, (Tk_FreeProc *)smtk_Destroy);
      break;
   }

   which_dev_x11 = save_which_dev_x11;
}

/*****************************************************************************/
/*
 * This procedure is invoked when a widget command is deleted.
 * If the widget isn't already in the process of being destroyed,
 * this command destroys it.
 */
static void
smtk_CmdDeletedProc(ClientData clientData) /* Pointer to widget record */
{
    SM_record *tk_data = (SM_record *) clientData;
    Tk_Window tkwin = tk_data->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	tk_data->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*****************************************************************************/
/*
 *	This procedure redraws the contents of an SM window.
 *	It is invoked as a do-when-idle handler, so it only runs
 *	when there's nothing else for the application to do.
 */
static void
smtk_Display(ClientData clientData)	/* Information about window. */
{
    SM_record *tk_data = (SM_record *) clientData;
    Tk_Window tkwin = tk_data->tkwin;

    if (!Tk_IsMapped(tkwin)) {
	return;
    }

    x11_gflush();
/*
 * and redisplay the plot
 */
    XCopyArea(XDEV->display, XDEV->backing, XDEV->wind, XDEV->gc, 0, 0,
	      XDEV->width, XDEV->height, 0, 0);
}

/*****************************************************************************/
/*
 * This procedure is invoked by Tk_EventuallyFree or Tk_Release to clean up
 * the internal structure of a square at a safe time (when no-one is using
 * it anymore)
 */
static void
smtk_Destroy(char *clientData)	/* Info about SM widget */
{
    SM_record *tk_data = (SM_record *)clientData;

    Tk_FreeOptions(configSpecs, (char *)tk_data, tk_data->display, 0);
#if 0
    if (tk_data->gc != None) {
	Tk_FreeGC(tk_data->display, tk_data->gc);
    }
#endif
    free((char *)tk_data);
}

/*****************************************************************************/
/*
 * Make tcl/tk dispatch events
 */
static void
smtk_DoOneEvent(Display *display)
{
   if(!XDEV->is_complete) {
      return;
   }
	
   for(;;) {
      while(Tk_DoOneEvent(TCL_DONT_WAIT) != 0) continue;
      
      XSync(XDEV->display, 0);
      
      if(Tk_DoOneEvent(TCL_DONT_WAIT) == 0) {
	 break;
      }
   }
}

#endif

#endif
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif

