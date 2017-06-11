#include "copyright.h"
#include "options.h"
#ifdef SUNWINDOWS
#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <usercore.h>
#include "sm.h"
#include "sm_declare.h"
/*
 * Some Sun functions that need to be declared:
 */
void await_any_button_get_locator_2();
void await_keyboard();
void create_retained_segment();
void define_color_indices();
void delete_retained_segment();
int get_view_surface();
void initialize_core();
void initialize_device();
void initialize_view_surface();
void inquire_charsize();
void line_abs_2();
void move_abs_2();
void polygon_rel_2();
void select_view_surface();
void set_echo();
void set_echo_surface();
void set_fill_index();
void set_image_transformation_type();
void set_line_index();
void set_linewidth();
void set_text_index();
void set_viewport_2();
void set_window();
void terminate_device();
void text();

#define ASPECT 0.75		/* aspect ratio of viewport */
#define NCOLOR 127		/* number of colours */
#define SEGMENT_NAME 1     	/* Default segment name set to 1 */
#define WAIT 10			/* how many ms to wait for mouse/keyboard */

static float blue[NCOLOR] =  { 1.0 },	/* Background colour: white */
       	     green[NCOLOR] = { 1.0 },
       	     red[NCOLOR] =   { 1.0 };
int cgpixwindd();
extern int sm_interrupt;			/* respond to ^C */
static int ncolor;			/* number of colours requested */
static struct vwsurf our_surf = DEFAULT_VWSURF(cgpixwindd);

int
sun_setup(disp)
char disp[];
{
   char *color;
   int r,g,b;				/* colours for background */
   static int first = 1;		/* Is this the first entry? */

   if(first) {
      first = 0;
   } else {
      default_ctype("black");
      ldef = 32;
      aspect = ASPECT;			/* aspect ratio from viewport */
      return(0);
   }

   if(get_view_surface(&our_surf, NULL)) {
      return(-1);
   }
   initialize_core(BASIC,SYNCHRONOUS, TWOD);
   our_surf.cmapsize = NCOLOR + 1;		/* + 1 for background colour */
   strcpy(our_surf.cmapname,PROGNAME);
   initialize_view_surface(&our_surf, FALSE);	/* keep hidden lines */
   select_view_surface(&our_surf);

   set_viewport_2(0.0, 1.0, 0.0, ASPECT);
   set_window(0.,(double)SCREEN_SIZE, 0.,(double)SCREEN_SIZE);

   set_image_transformation_type(NONE);		/* non-transformable */
   create_retained_segment(SEGMENT_NAME);
   set_linewidth(0.);  
/*
 * Set up to read mouse position
 */
   initialize_device(LOCATOR,1);
   initialize_device(BUTTON,1);
   initialize_device(BUTTON,2);
   initialize_device(BUTTON,3);

   set_echo_surface(LOCATOR,1,&our_surf);
   set_echo(LOCATOR,1,1);
   set_echo_surface(BUTTON,1,&our_surf);
   set_echo_surface(BUTTON,2,&our_surf);
   set_echo_surface(BUTTON,3,&our_surf);
/*
 * Now deal with default colours for the screen
 */
   if(*(color = print_var("background")) != '\0') {
      if(parse_color(color,&r,&g,&b) >= 0) {
	 red[0] = r/255.0;
	 green[0] = g/255.0;
	 blue[0] = b/255.0;
      } else {
	 msg_1s("I don't understand colour %s\n",color);
      }
   }
   define_color_indices(&our_surf,0,0,red,green,blue);
/*
 * For some reason, Sun seems to have installed thir own handler.
 * Calling notify_set_signal_func doesn't seem to work, so reinstall ours.
 */
   (void)signal(SIGINT,hand_C);

   default_ctype("black");
   ldef = 32;
   aspect = ASPECT;			/* aspect ratio from viewport */

   return(0);
}

void
sun_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

   delete_retained_segment(SEGMENT_NAME);
   create_retained_segment(SEGMENT_NAME);
}

void
sun_close()
{
#if 0
   terminate_device(BUTTON,1);
   terminate_device(BUTTON,2);
   terminate_device(BUTTON,3);
   terminate_device(LOCATOR,1);

   deselect_view_surface(&our_surf);
   terminate_core();
#endif
}

int
sun_cursor(get_pos, x, y)
int get_pos;				/* simply return the current position */
int *x, *y;
{
   char str[10];		/* what was typed */
   float xx,yy;			/* position of mouse */
   int button,			/* number of button hit */
       n;			/* number of chars typed */

   for(;;) {
      if(sm_interrupt) {
	 *str = 'q';
	 await_any_button_get_locator_2(0,1,&button,&xx,&yy);
	 break;
      }
      await_any_button_get_locator_2(WAIT,1,&button,&xx,&yy);
      if(button != 0) {		/* a button was pushed */
	 if(button == 1) *str = 'e';
	 else if(button == 2) *str = 'p';
	 else if(button == 3) *str = 'q';
	 break;
      }
      await_keyboard(0,1,str,&n);
      if(n > 0) {		/* a key was typed */
	 await_any_button_get_locator_2(0,1,&button,&xx,&yy);
	 break;
      }
   }
   
   *x = xx*SCREEN_SIZE + 0.5;
   *y = yy/ASPECT*SCREEN_SIZE + 0.5;

   return(*str);
}

void
sun_draw(x,y)
int x,y;
{
   line_abs_2((float)x,(float)y);
}

void
sun_enable()
{
   initialize_device(KEYBOARD,1);
   set_echo_surface(KEYBOARD,1,&our_surf);
}

void
sun_idle()
{
   terminate_device(KEYBOARD,1);
}

int
sun_lweight(lw)
double lw;
{
   set_linewidth(lw*0.15);
   return(0);
}

void
sun_reloc(x,y)
int x,y;
{
   move_abs_2((float)x - 10,(float)y + 30);
}

void
sun_line(x1,y1,x2,y2)
int x1,y1,x2,y2;
{
   move_abs_2((float)x1,(float)y1);
   line_abs_2((float)x2,(float)y2);
}

int
sun_char(s,x,y)
char *s;
int x,y;
{
   float ch,cw;

   if(s == NULL) {
      if(aangle == 0.0 && eexpand == 1.0) {
	 inquire_charsize(&cw,&ch);
	 cheight = 60*ch;		/* hardware sizes: 60 is magic */
	 cwidth = 60*cw;
      }
      return(0);
   }
   move_abs_2((float)x,(float)y);
   text(s);
   return(0);
}

void
sun_ctype(r,g,b)
int r,g,b;				/* only r used, in set_line_index */
{
   if(ncolor > NCOLOR) {		/* must scale to 0:NCOLOR - 1 */
      r = r*(float)(NCOLOR - 1)/(ncolor - 1);
   }
   r++;					/* [0] is the background colour */

   set_line_index(r);
   set_text_index(r);
   set_fill_index(r);
}

int
sun_set_ctype(colors,ncol)
COLOR *colors;
int ncol;
{
   float f,			/* interpolation weight */
	 sq;			/* factor to squash by */
   int i,j;

   if(colors == NULL) {		/* Do I exist? */
      return(0);
   }

   if(ncol < NCOLOR) {
      for(i = 0;i < ncol;i++) {
	 red[i] = colors[i].red/255.;
	 green[i] = colors[i].green/255.;
	 blue[i] = colors[i].blue/255.;
      }
      define_color_indices(&our_surf,1,ncol,red,green,blue);
   } else {			/* we'll have to squash them in */
      sq = (float)(NCOLOR - 1)/(ncol - 1)*0.999999;
      for(i = 0;i < NCOLOR;i++) {
	 j = i/sq;
	 f = i/sq - j;
	 if(j >= ncol) { printf("i = %d j = %d f = %g\n",i,j,f); }
	 red[i] = (colors[j].red*(1 - f) + colors[j+1].red*f)/255;
	 green[i] = (colors[j].green*(1 - f) + colors[j+1].green*f)/255;
	 blue[i] = (colors[j].blue*(1 - f) + colors[j+1].blue*f)/255;
      }
      define_color_indices(&our_surf,1,NCOLOR,red,green,blue);
   }
   ncolor = ncol;
   sun_ctype(0,0,0);
   return(0);
}

int
sun_fill_pt(n)
int n;                          /* number of sides */
{
   float dtheta, theta;
   static float old_ang,	/* old values of angle */
		xlist[MAX_POLYGON + 1],
		ylist[MAX_POLYGON + 1];  /* vertices describing point */
   int i,
       xpsize,			/* scale for points == xscale*PDEF*eexpand */
       ypsize;
   static int num = -1,         /* number of vertices used last time */
	      old_xp,old_yp;	/* old values of xpsize, ypsize */

   if(n < 2) {
      sun_line(xp,yp,xp,yp);
      return(0);
   }

   dtheta = 2*PI/n;
   xpsize = 2*PDEF*sin(dtheta/2)*eexpand;
   ypsize = 2*PDEF*sin(dtheta/2)*eexpand;

   if(n != num || aangle != old_ang || xpsize != old_xp || ypsize != old_yp) {
      if(n > MAX_POLYGON) num = MAX_POLYGON;
      else num = n;

      theta = 3*PI/2 + dtheta/2 + aangle*PI/180;
      old_ang = aangle;
      old_xp = xpsize;
      old_yp = ypsize;

      xlist[0] = PDEF*eexpand*cos(theta);
      ylist[0] = PDEF*eexpand*sin(theta)/aspect;

      theta += dtheta/2;
      for(i = 1;i <= num;i++) {
	 xlist[i] = -xpsize*sin(theta);
	 ylist[i] = ypsize*cos(theta)/aspect;
	 theta += dtheta;
      }
   }
   move_abs_2((float)xp,(float)yp);
   polygon_rel_2(xlist,ylist,n);
   return(0);
}
#endif
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif
