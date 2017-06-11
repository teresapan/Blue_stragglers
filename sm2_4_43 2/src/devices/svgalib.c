/*
 * This the SM device driver for linux's svgalib
 */
#include "copyright.h"
#include "options.h"

#if defined(SVGALIB)

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <vga.h>
#include "sm.h"
#include "sm_declare.h"

static void show_modes(void);

static int color_index = 1;		/* current index into palette */
static int width, height;		/* screen size */
static int mode = 1;			/* graphics mode */
static int text_mode = 1;		/* are we in text mode? */

static void (*svga_hand_usr2)(int) = NULL; /* installed USR2 handler */
static void
hand_usr2(int sig)	/* our USR2 handler */
{
   if(svga_hand_usr2 != NULL) {
      (*svga_hand_usr2)(sig);
   }
   printf("Setting mode to %d\n", mode); fflush(stdout);
   vga_setmode(mode);
}

int
svgalib_setup(char *s)
{
   static char forecolor[21] = "white";	/* name of foreground colour */
   static int initialised = 0;		/* is display initialised? */
   char *word;
/*
 * Process argument string
 */
   if(*(word = next_word(s)) != '\0') {
      do {
	 if(strcmp(word,"modes") == 0) { /* show modes */
	    show_modes();
	    return(-1);
	 } else if(strcmp(word,"-fg") == 0) { /* choose a foreground colour */
	    if((word = next_word((char *)NULL)) == NULL) {
	       msg("You must follow -fg with a colour's name\n");
	       return(-1);
	    }
	    strncpy(forecolor,word,20);
	 } else if(strcmp(word,"-m") == 0) { /* choose a mode */
	    int oldmode = mode;
	    if((word = next_word((char *)NULL)) == NULL) {
	       msg("You must follow -m with a number\n");
	       return(-1);
	    }
	    if((mode = atoi(word)) != oldmode) {
	       initialised = 0;
	    }

	    if(!vga_hasmode(mode)) {
	       msg_1d("mode %d is not valid; I'll try 1\n",mode);
	       mode = 1;
	       if(!vga_hasmode(mode)) {
		  msg("mode 1 is invalid; try \"device svga modes\"\n");
		  return(-1);
	       }
	    }
	 } else {
	    msg_1s("Unknown option: %s\n",word);
	 }
      } while(*(word = next_word((char *)NULL)) != '\0');
   }
/*
 * Done with argument list
 */
   if(!initialised) {
      vga_init();
   }
   vga_setmode(mode);
#if 0
   vga_runinbackground(VGA_GOTOBACK,set_go); 
   vga_runinbackground(1); 
#endif
   
   width = vga_getxdim() - 1;
   height = vga_getydim() - 1;
      
   xscreen_to_pix = width/(float)SCREEN_SIZE;
   yscreen_to_pix = height/(float)SCREEN_SIZE;
   aspect = (float)(height)/width;
   ldef = 1/xscreen_to_pix;

   default_ctype(forecolor);		/* set_dev()'ll look in .sm */

   vga_flip();

   initialised = 1;
   text_mode = 1;
/*
 * Install our handler for USR2 in addition to svgalib's
 */
   if(svga_hand_usr2 == NULL) {
      svga_hand_usr2 = signal(SIGUSR2, SIG_DFL);
   }
   (void)signal(SIGUSR2, hand_usr2);

   return(0);
}

int
svgalib_char(char *s,
	     int x, int y)
{
   return(-1);
}

void svgalib_close() {;}

int
svgalib_cursor(x, y)
int *x, *y;
{
   return(-1);
}

int svgalib_dot(x, y)
int x, y;
{
   svgalib_line(x - (int)(llweight/(2*xscreen_to_pix)),y,
	    x + (int)(llweight/(2*xscreen_to_pix)),y);
   return(0);
}

void
svgalib_draw(x, y)
int x, y;
{
   svgalib_line(xp, yp, x, y);
}

void
svgalib_enable(void)
{
   if(text_mode) {
      text_mode = 0;
      vga_flip();
      vga_setcolor(color_index);
   }
}

void
svgalib_idle()
{
   if(!text_mode) {
      text_mode = 1;
      vga_flip();
   }
}

void
svgalib_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

   vga_clear();
}

int
svgalib_fill_pt(n)
int n;
{
   return(-1);
}

int
svgalib_fill_polygon(style, x, y, n)
int style;
short *x, *y;
int n;
{
   return(-1);
}

void svgalib_gflush() { ; }

void
svgalib_line(x1,y1,x2,y2)
int x1,y1,x2,y2;
{
   x1 = x1*xscreen_to_pix + 0.5;
   x2 = x2*xscreen_to_pix + 0.5;
   y1 = (height - 1) - y1*yscreen_to_pix + 0.5;
   y2 = (height - 1) - y2*yscreen_to_pix + 0.5;

   vga_drawline(x1, y1, x2, y2);
}

int
svgalib_ltype(lt)
int lt;
{
   return(-1);
}

int
svgalib_lweight(lw)
double lw;
{
   return(-1);
}

void svgalib_page()
{
   vga_flip();
}

void
svgalib_redraw(fd)
int fd;
{
   ;
}

void
svgalib_reloc(x, y)
int x, y;
{
   ;
}

int
svgalib_set_ctype(COLOR *colors,
		  int ncolors)
{
   int i;

   if(colors == NULL) {			/* just an enquiry */
      return(0);
   }

   for(i = 0;i < ncolors;i++) {
      vga_setpalette(i + 1,colors[i].red,colors[i].green,colors[i].blue);
   }

   return(0);
}

void
svgalib_ctype(r, g, b)
int r, g, b;
{
   color_index = r + 1;			/* 0 is the background */
   vga_setcolor(color_index);
}

/*
 * This function is stolen from vgatest.c and is not copyright SM
 */
static void
show_modes(void)
{
   int high = 0;
   int i;
   
   for (i = 1; i <= GLASTMODE; i++) {
      if(vga_hasmode(i)) {
	 vga_modeinfo *info;
	 char expl[100];
	 char *cols = NULL;
	
	 *expl = '\0';
	 info = vga_getmodeinfo(i);
	 switch (info->colors) {
	  case 2:
	    cols = "2";
	    strcpy(expl, "1 bitplane, monochrome");
	    break;
	  case 16:
	    cols = "16";
	    strcpy(expl, "4 bitplanes");
	    break;
	  case 256:
	    if (i == G320x200x256)
	      strcpy(expl, "packed-pixel");
	    else
	     if (i == G320x240x256 || i == G320x400x256 || i == G360x480x256)
	       strcpy(expl, "Mode X");
	     else
	       strcpy(expl, "packed-pixel, banked");
	    break;
	  case 1 << 15:
	    cols = "32K";
	    strcpy(expl, "5-5-5 RGB, blue at LSB, banked");
	    break;
	  case 1 << 16:
	    cols = "64K";
	    strcpy(expl, "5-6-5 RGB, blue at LSB, banked");
	    break;
	  case 1 << 24:
	    cols = "16M";
	    if(info->bytesperpixel==3) {
	       if(info->flags&RGB_MISORDERED)
		 strcpy(expl, "8-8-8 BGR, red byte first, banked");
	       else
		 strcpy(expl, "8-8-8 RGB, blue byte first, banked");
	    } else if(info->flags&RGB_MISORDERED) {
	       strcpy(expl, "8-8-8 RGBX, 32-bit pixels, X byte first, banked");
	    } else {
	       strcpy(expl,
		      "8-8-8 XRGB, 32-bit pixels, blue byte first, banked");
	    }
	    break;
	 }
	 if (info->flags&IS_INTERLACED) {
	    if (*expl != '\0') strcat(expl, ", ");
	    strcat(expl, "interlaced");
	 }
	 if (info->flags&IS_DYNAMICMODE) {
	    if (*expl != '\0') strcat(expl, ", ");
	    strcat(expl, "dynamically loaded");
	 }
	 high = i;
	 printf("%5d: %dx%d, ",i, info->width, info->height);
	 if (cols == NULL) printf("%d", info->colors);
	 else  printf("%s", cols);
	 printf(" colors ");
	 if (*expl != '\0') printf("(%s)", expl);
	 printf("\n");
      }
   }
}
#endif
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif
