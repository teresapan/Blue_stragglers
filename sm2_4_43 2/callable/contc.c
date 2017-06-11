#include <stdio.h>
#include "options.h"
#include "sm_declare.h"
#define NXY 20

int
main()
{
   int i,j;
   float lev[NXY],*z[NXY],zz[NXY][NXY],min,max;

   if(sm_device("x11") < 0) {
      exit(-1);
   }
   sm_location(5000,30000,5000,30000);
   for(i = 0;i < NXY;i++) {
      z[i] = zz[i];
      for(j = 0;j < NXY;j++) {
         zz[i][j] = (j - 9)*(j - 9) + i*i;
      }
   }

   sm_graphics();
   sm_limits(-1.0,21.0,-1.0,21.0);
   sm_box(1,2,0,0);
   sm_defimage(z,0.,20.,0.,20.,NXY,NXY);
   sm_minmax(&min,&max);
   for(i = 0;i < NXY;i++) {
      lev[i] = min + i/(NXY - 1.)*(max - min);
   }
   sm_levels(lev,NXY);
   sm_contour();
   sm_gflush();
   sm_redraw(0);
   sm_hardcopy();
   sm_alpha();
   return(0);
}
