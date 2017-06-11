#include <stdio.h>
#include "options.h"
#include "sm_declare.h"
#define NXY 20

int
main(ac, av)
int ac;
char **av;
{
   char *device = "x11";
   REAL x[NXY],y[NXY],err[NXY], pptype;
   int i;

   if(ac > 1) {
      device = av[1];
   }

   if(sm_device(device) < 0) {
      exit(-1);
   }
   sm_defvar("TeX_strings","1");
   sm_defvar("default_font","\\2\\bf");
   sm_location(5000,30000,5000,30000);
   for(i = 0;i < NXY;i++) {
      x[i] = i;
      y[i] = i*i;
      err[i] = 10;
   }

   sm_graphics();
   sm_window(2,2,1,1,2,1);
   sm_limits(0,5.0,0.0,500.);
   sm_ticksize(-1.,0.,0.,0.);
   sm_expand(0.7);
   sm_box(1,0,0,0);
   sm_expand(1.0);
   sm_ticksize(0.,0.,0.,0.);
   sm_window(1,1,1,1,1,1);
   sm_gflush();
   sm_alpha();
   printf("Hit a key or mouse button "); fflush(stdout);
   sm_curs(x,y,&i);
   sm_erase();
   sm_limits(-1.0,22.0,0.0,500.);
   sm_box(1,2,0,0);
   sm_angle(45.);
   pptype = 40;
   sm_ptype(&pptype,1);
   sm_points(x,y,NXY);
   sm_angle(0.);
   sm_lweight(1);
   sm_expand(2.);
   sm_identification("Robert \\int \\ f(x) dx");
   sm_expand(1.);
   sm_xlabel("x axis");
   sm_ylabel("Y");
   sm_errorbar(x,y,err,2,NXY);
   sm_errorbar(x,y,err,4,NXY);
   sm_lweight(3);
   sm_histogram(x,y,NXY);
   sm_lweight(1);
   sm_ltype(2);
   sm_conn(x,y,NXY);
   sm_ltype(0);
   sm_relocate(10.,100.);
   sm_label("Hello ");
   sm_dot();
   sm_draw(20.,100.);
   sm_putlabel(4,"Goodbye \\point6 0");
   sm_grid(0,0);
   sm_ltype(1); sm_ctype("red");
   sm_grid(1,0);
   sm_ltype(0); sm_ctype("default");
   sm_gflush();
   sm_alpha();
   printf("Hit a key or mouse button "); fflush(stdout);
   sm_curs(x,y,&i);
   printf("Cursor: %c at (%g,%g)\n",i,*x,*y);
   printf("Hit any key to exit ");
   fflush(stdout);
   sm_parser("echo Hello World");
   sm_redraw(0);
   (void)getchar();
   sm_hardcopy();
   sm_alpha();
   return(0);
}
