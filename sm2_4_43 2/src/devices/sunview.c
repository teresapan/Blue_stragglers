#include "copyright.h"
#include "options.h"
#ifdef SUN_VIEW
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <suntool/sunview.h>
#include <suntool/canvas.h>
#include <suntool/panel.h>
#include <suntool/walkmenu.h>
#include "sm.h"
#include "sm_declare.h"
/*
 * some declarations that sun should have provided:
 */
void pw_setcmsname(),
     pw_polyline(),
     pw_polygon_2();

#define INACTIVE_CURSOR_LEN 10	/* length of cursor crosshairs when inactive */
#define NCOLOR 127		/* number of colours, should be 2^n - 1 */
#define NPTS 100		/* size of polyline buffer */
#ifndef event_action
#  define event_action event_id	/* missing in OS 3.?, at least sometimes */
#endif

static unsigned char blue[NCOLOR + 1] =  { 255 }, /* Background colour: white */
  		     green[NCOLOR + 1] = { 255 },
	       	     red[NCOLOR + 1] =   { 255 },
		     move[NPTS];	/* Move before drawing? */
static Pr_brush brush;
static Canvas canvas;
static float xscale,yscale;		/* window -> SCREEN coordinates */
static Frame frame,
	     create_confirm();
static int get_arg(),
	   init = 1,			/* initialise frame */
	   linecolor = 1,
	   ncolor = -1,			/* number of colours requested */
	   p_ctr = 0,			/* counter in points[] */
	   sc_height,sc_width,		/* height/width of screen */
	   sunv_is_active = 0;		/* we are talking to the frame */
static Notify_value sunv_destroy();
static Pixwin *pixwin;
static unsigned short icon_image[] = {
#  define SUN_VIEW_ICON
#  include "SM_icon.h"
};
/*
 * The mpr_static macro uses a nasty cpp trick to try to concatenate
 * 2 tokens, enshrined in the macro CAT. We need our own for ANSI compilers
 */
#ifdef ANSI_CPP
#undef CAT
#define CAT(A,B) (A ## B)
#endif
mpr_static(sm_icon,SM_ICON_XSIZE,SM_ICON_YSIZE,1,icon_image);
static struct pr_pos point_arr[NPTS];	/* array for buffering lines */
static void hit_button(),
	    sunv_resize();

int
sunv_setup(disp)
char *disp;
{
   char label[80];
   char *color;
   Cursor cursor;
   Icon icon;
   int i,j,
       r,g,b,				/* colours for background */
       frame_closed = 0,
       frame_show_label = 1,
       win_x = 100,win_y = 100,
       win_width = 650,win_height = 550;
   Menu menu;
   Rect frame_closed_rect;
   void sunv_erase();
   
   if(init) {
      frame_closed_rect.r_left = 800;
      frame_closed_rect.r_top = 2;
      frame_closed_rect.r_height = SM_ICON_YSIZE;
      frame_closed_rect.r_width = SM_ICON_XSIZE;
      strcpy(label,PROGNAME);

      for(i = 0;disp[i] != '\0';i++) {
	 if(!strncmp("-W",&disp[i],2)) {
	    i += 2;
	    switch (disp[i]) {
	     case 'H':
	       msg_1s("Syntax: device sunview %s\n",
	 	"[-WH] [-Wi] [-Wl label] [-Wn] [-WP x y] [-Wp x y] [-Ws w h]");
	       break;
	     case 'i':
	       frame_closed = 1;
	       break;
	     case 'l':
	       i++;
	       while(isspace(disp[i])) i++;
	       for(j = 0;disp[i] != '\0' && !isspace(disp[i]);i++) {
		  label[j++] = disp[i];
	       }
	       i--;
	       label[j] = '\0';
	       break;
	     case 'n':
	       frame_show_label = 0;
	       break;
	     case 'P':
	       frame_closed_rect.r_left = get_arg(disp,&i);
	       frame_closed_rect.r_top = get_arg(disp,&i);
	       break;
	     case 'p':
	       win_x = get_arg(disp,&i);
	       win_y = get_arg(disp,&i);
	       break;
	     case 's':
	       win_width = get_arg(disp,&i);
	       win_height = get_arg(disp,&i);
	       break;
	     default:
	       msg_1d("Unknown option: -W%c\n",disp[i]);
	       while(disp[i] != '\0' && !isspace(disp[i])) i++;
	       break;
	    }
	 }
      }
	       
      if((frame = window_create((Window)NULL,FRAME,FRAME_NO_CONFIRM,TRUE,
	       WIN_X,win_x,WIN_Y,win_y,WIN_WIDTH,win_width,WIN_HEIGHT,win_height,
	       FRAME_CLOSED,frame_closed,FRAME_CLOSED_RECT,&frame_closed_rect,
	       FRAME_SHOW_LABEL,frame_show_label,FRAME_LABEL,label,0)) == NULL) {
         msg_1s("Can't open %s\n",disp);
         return(-1);
      }
/*
 * If the first create went OK, we are on a sun console, so assume that
 * all other calls will return OK too.
 */
      init = 0;

      icon = icon_create(ICON_IMAGE,&sm_icon,0);
      window_set(frame,FRAME_ICON,icon,0);
      
      menu = menu_create(MENU_PULLRIGHT_ITEM,"Frame",
      				(Menu)window_get(frame,WIN_MENU),
      			 MENU_ITEM,MENU_STRING,"Erase",
			 	MENU_NOTIFY_PROC,sunv_erase,0,0);
      window_set(frame,WIN_MENU,menu,0);
      

      (void)notify_interpose_destroy_func(frame,sunv_destroy);

      if(*(color = print_var("background")) != '\0') {
         if(parse_color(color,&r,&g,&b) >= 0) {
	    red[0] = r;
	    green[0] = g;
	    blue[0] = b;
         } else {
   	    msg_1s("I don't understand colour %s\n",color);
         }
      }
/*
 * We are ready to create the graphics window, at last!
 */
      canvas = window_create(frame,CANVAS,FRAME_NO_CONFIRM,TRUE,
		  CANVAS_RETAINED,FALSE,CANVAS_RESIZE_PROC,sunv_resize,0);

      pixwin = (Pixwin *)window_get(canvas,WIN_PIXWIN);
      pw_setcmsname(pixwin,PROGNAME);
      pw_putcolormap(pixwin,0,1,red,green,blue);
      pixwin = canvas_pixwin(canvas);
      pw_setcmsname(pixwin,PROGNAME);
      pw_putcolormap(pixwin,0,1,red,green,blue);

      window_set(canvas,CANVAS_RETAINED,TRUE,0);

      cursor = window_get(canvas,WIN_CURSOR);
      cursor_set(cursor,CURSOR_SHOW_CURSOR,FALSE,0);
      cursor_set(cursor,CURSOR_SHOW_CROSSHAIRS,TRUE,0);
      cursor_set(cursor,CURSOR_CROSSHAIR_OP,PIX_SRC ^ PIX_DST,0);
      cursor_set(cursor,CURSOR_CROSSHAIR_THICKNESS,1,0);
      cursor_set(cursor,CURSOR_CROSSHAIR_COLOR,1,0);
      cursor_set(cursor,CURSOR_CROSSHAIR_LENGTH,INACTIVE_CURSOR_LEN,0);
      cursor_set(cursor,CURSOR_CROSSHAIR_GAP,3,0);
      window_set(canvas,WIN_CURSOR,cursor,0);

      window_set(frame,WIN_SHOW,TRUE,0);
      notify_dispatch();			/* initial display */
      notify_do_dispatch();		/* access notifier on read/select */
   }
/*
 * Sun have their own signal handlers to deal with the SunView notifier.
 * Regretably, if I call theirs I can't pass extra stuff like the FPE
 * codes, so I'll stick to using mine. I don't trap any of the signals
 * that they care about.
 * In fact, they say that I shouldn't call system() either, but let's
 * worry about that when it breaks.
 */
   (void)signal(SIGINT,hand_C);

/* ldef and aspect are set in sunv_resize */
   sunv_resize((Canvas)0,sc_width,sc_height);
   default_ctype("black");

   return(0);
}

void
sunv_close()
{
   sunv_gflush();
}

int
sunv_char(s,x,y)
char *s;
int x,y;
{
   return(-1);
}

void
sunv_ctype(r,g,b)
int r,g,b;				/* only r used, in set_line_index */
{
   if(ncolor > NCOLOR) {		/* must scale to 0:NCOLOR - 1 */
      linecolor = r*(float)(NCOLOR - 1)/(ncolor - 1);
   } else {
      linecolor = r;
   }
   linecolor++;				/* [0] is the background */
}

int
sunv_set_ctype(colors,ncol)
COLOR *colors;
int ncol;
{
   char cmapname[40];			/* SunView wants a new name for cmap */
   unsigned char r,g,b;			/* a new colour */
   float f,				/* interpolation weight */
	 sq;				/* factor to squash by */
   int cmap_size,			/* size of colour map */
       i,j,
       new = 0;				/* is it really a new colour map? */
   static int nn = 0;			/* make a new name for colour map */
   Pixwin *pix;

   if(colors == NULL) {			/* Do I exist? */
      return(0);
   }

   if(ncol < NCOLOR) {
      for(cmap_size = 1;cmap_size < ncol + 1;cmap_size *= 2) ;
      for(i = 0;i < ncol;i++) {
	 r = colors[i].red;
	 g = colors[i].green;
	 b = colors[i].blue;
	 if(red[i + 1] != r || green[i + 1] != g || blue[i + 1] != b) {
	    new = 1;			/* a new colour map */
	 }
	 red[i + 1] = r;
	 green[i + 1] = g;
	 blue[i + 1] = b;
      }
   } else {			/* we'll have to squash them in */
      cmap_size = NCOLOR + 1;
      sq = (float)(NCOLOR - 1)/(ncol - 1)*0.999999;
      for(i = 0;i < NCOLOR;i++) {
	 j = i/sq;
	 f = i/sq - j;
	 r = colors[j].red*(1 - f) + colors[j+1].red*f;
	 g = colors[j].green*(1 - f) + colors[j+1].green*f;
	 b = colors[j].blue*(1 - f) + colors[j+1].blue*f;
	 if(red[i + 1] != r || green[i + 1] != g || blue[i + 1] != b) {
	    new = 1;			/* a new colour map */
	 }
	 red[i + 1] = r;
	 green[i + 1] = g;
	 blue[i + 1] = b;
      }
   }

   if(ncolor < 0 || new) {
      sprintf(cmapname,"%s%d",PROGNAME,nn++);
      pix = (Pixwin *)window_get(canvas,WIN_PIXWIN);
      pw_setcmsname(pix,cmapname);
      pw_putcolormap(pix,0,cmap_size,red,green,blue);
      pw_setcmsname(pixwin,cmapname);
      pw_putcolormap(pixwin,0,cmap_size,red,green,blue);
      ncolor = ncol;
      sunv_ctype(0,0,0);
   }
   return(0);
}

/*
 * For some reason, we only see every other keyboard event
 */
int
sunv_cursor(get_pos,x,y)
int get_pos;				/* simply return the current position */
int *x,*y;
{
   Cursor cursor;
   Event event;
   int type;			/* type of event */

   cursor = window_get(canvas,WIN_CURSOR);
   cursor_set(cursor,CURSOR_CROSSHAIR_LENGTH,CURSOR_TO_EDGE,0);
   window_set(canvas,WIN_CURSOR,cursor,0);
   window_set(canvas,WIN_CONSUME_KBD_EVENTS,WIN_NO_EVENTS,WIN_ASCII_EVENTS,0,0);

   for(;;) {
      if(window_read_event(canvas,&event) < 0) {
         perror("read event");
      }
      (void)canvas_event(canvas,&event);
      type = event_action(&event);

      if(type == MS_LEFT) {
	 type = 'e';
      } else if(type == MS_MIDDLE) {
	 type = 'p';
      } else if(type == MS_RIGHT) {
	 type = 'q';
      }

      if(isascii(type)) {
	 *x = event_x(&event);
	 *y = sc_height - event_y(&event);
	 *x = *x/xscale + 0.5;
	 *y = *y/yscale + 0.5;
	 cursor_set(cursor,CURSOR_CROSSHAIR_LENGTH,INACTIVE_CURSOR_LEN,0);
	 window_set(canvas,WIN_CURSOR,cursor,0);
	 window_set(canvas,WIN_IGNORE_KBD_EVENT,WIN_ASCII_EVENTS,0);
	 return(type);
      }
   }
}

void
sunv_draw(x,y)
int x,y;
{
   x *= xscale;
   y *= yscale;
   y = sc_height - y;
   
   point_arr[p_ctr].x = x;
   point_arr[p_ctr].y = y;
   move[p_ctr++] = 0;
   if(p_ctr >= NPTS - 1) {
      pw_polyline(pixwin,0,0,p_ctr,point_arr,move,&brush,
     		(struct pr_texture *)NULL,PIX_SRC | PIX_COLOR(linecolor));
      p_ctr = 0;
      point_arr[p_ctr].x = x;
      point_arr[p_ctr].y = y;
      move[p_ctr++] = 0;
   }
}

void
sunv_enable()
{
   sunv_is_active = 1;
}

void
sunv_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

   pw_writebackground(pixwin,0,0,sc_width,sc_height,PIX_SRC);
   window_set(frame,WIN_SHOW,TRUE,0);
}

void
sunv_idle()
{
   sunv_is_active = 0;
}

void
sunv_gflush()
{
   notify_dispatch();
   if(p_ctr > 0) {
      pw_polyline(pixwin,0,0,p_ctr,point_arr,move,&brush,
      		(struct pr_texture *)NULL,PIX_SRC | PIX_COLOR(linecolor));
      point_arr[0].x = point_arr[p_ctr - 1].x;
      point_arr[0].y = point_arr[p_ctr - 1].y;
      move[0] = 0;
      p_ctr = 1;
   }
}

/*
 * Only support erasing of lines (lt 10 and 11)
 */
int
sunv_ltype(lt)
int lt;
{
   static int old_linecolor = 1;
   
   if(lt == 10) {
      if(linecolor != 0) {
	 old_linecolor = linecolor;
      }
      linecolor = 0;
      return(0);
   } else if(lt == 11) {
      linecolor = old_linecolor;
      return(0);
   }
   return(-1);
}

void
sunv_line(x_1,y_1,x_2,y_2)
int x_1,y_1,x_2,y_2;
{
   x_1 *= xscale;
   x_2 *= xscale;
   y_1 *= yscale;
   y_2 *= yscale;
   y_1 = sc_height - y_1;
   y_2 = sc_height - y_2;

   point_arr[p_ctr].x = x_1;
   point_arr[p_ctr].y = y_1;
   move[p_ctr] = p_ctr == 0 ? 0 : 1;
   p_ctr++;
   point_arr[p_ctr].x = x_2;
   point_arr[p_ctr].y = y_2;
   move[p_ctr++] = 0;
   if(p_ctr >= NPTS - 1) {
      pw_polyline(pixwin,0,0,p_ctr,point_arr,move,&brush,
      		(struct pr_texture *)NULL,PIX_SRC | PIX_COLOR(linecolor));
      p_ctr = 0;
      point_arr[p_ctr].x = x_2;
      point_arr[p_ctr].y = y_2;
      move[p_ctr++] = 0;
   }
}

int
sunv_lweight(lw)
double lw;
{
   brush.width = 1 + lw/3;			/* 3 seems about right */
   return(0);
}

void
sunv_page()
{
   window_set(frame,WIN_SHOW,TRUE,0);
}

void
sunv_reloc(x,y)
int x,y;
{
   x *= xscale;
   y *= yscale;
   y = sc_height - y;
   
   point_arr[p_ctr].x = x;
   point_arr[p_ctr].y = y;
   move[p_ctr++] = 1;
   if(p_ctr >= NPTS - 1) {
      pw_polyline(pixwin,0,0,p_ctr,point_arr,move,&brush,
     		(struct pr_texture *)NULL,PIX_SRC | PIX_COLOR(linecolor));
      p_ctr = 0;
      point_arr[p_ctr].x = x;
      point_arr[p_ctr].y = y;
      move[p_ctr++] = 0;
   }
}

int
sunv_fill_pt(n)
int n;					/* number of sides */
{
   float dtheta, theta,
         xpsize,ypsize;
   static float old_ang,		/* old values of angle */
  	        old_xp,old_yp;		/* old values of xpsize, ypsize */
   int i,
       x,y;
   static int num = -1;			/* number of vertices used last time */
   static struct pr_pos vlist[MAX_POLYGON];

   xpsize = PDEF*eexpand*xscale;
   ypsize = PDEF*eexpand*yscale;
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
	 vlist[i].x = 1000 + xpsize*cos(theta) + 0.5;
	 vlist[i].y = 1000 - ypsize*sin(theta) + 0.5;
	 theta += dtheta;
      }
   }

   x = (int)(xp*xscale + 0.5) - 1000;
   y = (int)(sc_height - yp*yscale + 0.5) - 1000;

   pw_polygon_2(pixwin,x,y,1,&num,vlist,PIX_SRC | PIX_COLOR(linecolor),
			   				(Pixrect *)NULL,0,0);
   return(0);
}

/************************************************************************/
/*
 * Various static utilities
 */
static int
get_arg(str,i)
char *str;
int *i;
{
   int val;

   (*i)++;				/* skip letter we switched on */
   val = atoi(&str[*i]);

   while(isspace(str[*i])) (*i)++;
   while(isdigit(str[*i])) (*i)++;
   (*i)--;				/* one too far */

   return(val);
}


static Notify_value
sunv_destroy(frame,status)
Frame frame;
Destroy_status status;
{
   Frame confirm;
   
   if(status == DESTROY_CHECKING) {
      confirm = create_confirm();

      if((char)(long)window_loop(confirm) == 'n') {
         window_destroy(confirm);
         (void)notify_veto_destroy(frame);
	 return(NOTIFY_DONE);
      }
   } else {
      init = 1;			/* recreate window if asked for again */
      ncolor = -1;			/* need a new colour map */
   }
   return(notify_next_destroy_func(frame,status));
}


static Frame
create_confirm()
{
   Frame confirm;
   int left,top;
   Panel panel;
   Panel_item warning;
   Rect *r;

   confirm = window_create((Window)NULL,FRAME,FRAME_SHOW_LABEL,FALSE,
			   			FRAME_NO_CONFIRM,TRUE,0);
   panel = window_create(confirm,PANEL,0);
   (void)panel_create_item(panel,PANEL_MESSAGE,PANEL_LABEL_STRING,
      							"Destroy Window?",0);
   warning = panel_create_item(panel,PANEL_MESSAGE,PANEL_LABEL_STRING,
					"Beware: I think that I am active",0);
   (void)panel_create_item(panel,PANEL_BUTTON,PANEL_NOTIFY_PROC,hit_button,
	PANEL_LABEL_IMAGE,panel_button_image(panel,"Cancel",(Pixfont *)NULL,0),
						PANEL_CLIENT_DATA,'n',0);
   (void)panel_create_item(panel,PANEL_BUTTON,PANEL_NOTIFY_PROC,hit_button,
	 PANEL_LABEL_IMAGE,panel_button_image(panel,"Confirm",(Pixfont *)NULL,0),
						PANEL_CLIENT_DATA,'y',0);
   window_fit(panel);
   window_fit(confirm);

   if(sunv_is_active) {
      panel_set(warning,PANEL_SHOW_ITEM,TRUE,0);
   } else {
      panel_set(warning,PANEL_SHOW_ITEM,FALSE,0);
   }
/*
 * put confirmer in middle of screen
 */
   r = (Rect *)window_get(confirm,WIN_SCREEN_RECT);
   left = (r->r_width - (int)window_get(confirm,WIN_WIDTH))/2;
   top = (r->r_height - (int)window_get(confirm,WIN_HEIGHT))/2;
   if(left < 0) left = 0;
   if(top < 0) top = 0;
   window_set(confirm,WIN_X,left,WIN_Y,top,0);
   
   return(confirm);
}

static void
sunv_resize(canv,width,height)
Canvas canv;				/* canv is not used */
int width,
    height;
{
   aspect = (float)height/width;
   sc_height = height;
   sc_width = width;
   xscale = (float)sc_width/SCREEN_SIZE;
   yscale = (float)sc_height/SCREEN_SIZE;
   
   ldef = 1/xscale;
}


static void
hit_button(item,event)
Panel_item item;
Event *event;
{
   window_return(panel_get(item,PANEL_CLIENT_DATA,0));
}
#else
/*
 * Include a symbol for picky linkers
 */
#  ifndef lint
      static void linker_func() { linker_func(); }
#  endif
#endif
