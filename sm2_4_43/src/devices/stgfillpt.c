#include "copyright.h"
#include <math.h>
#include <stdio.h>
#include "options.h"
#include "tty.h"
#include "stdgraph.h"
#include "sm.h"
#include "sm_declare.h"

#define INC_THETA			/* theta += dtheta */ \
  temp = c_theta*c_dtheta - s_theta*s_dtheta; \
  s_theta = s_theta*c_dtheta + c_theta*s_dtheta; \
  c_theta = temp
#define SIZE 40

int
stg_fill_pt(n)
int n;
{
   char fs[SIZE],		/* Fill graphcap capabilities */
   	fd[SIZE],
   	fe[SIZE];
   float dtheta, theta,
         c_theta,s_theta,		/* cos/sin(theta) and cos/sin(tha) */
         c_dtheta,s_dtheta,		/* cos/sin(dtheta) */
   	 ps_x,ps_y,			/* point sizes in x and y */
   	 temp;
   int j;
   
   j = ttygets(g_tty,"FS",fs,SIZE);
   j += ttygets(g_tty,"FD",fd,SIZE);
   j += ttygets(g_tty,"FE",fe,SIZE);
   if(j == 0) {				/* No "fill" capability */
      return(-1);
   }

   g_reg[E_IOP].i = 0;
   if(fs[0] != '\0') {
      if(stg_encode(fs, g_mem, g_reg) == OK) {
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
      }
   }

   ps_x = ps_y = PDEF*eexpand;
   if(aspect > 1.0) {			/* allow for aspect ratio of screen */
      ps_y /= aspect;
   } else if(aspect < 1.0) {
      ps_x *= aspect;
   }

   dtheta = 2*PI/n;
   theta = (3*PI + dtheta)/2 + aangle*PI/180;
   c_theta = cos(theta);
   s_theta = sin(theta);
   c_dtheta = cos(dtheta);
   s_dtheta = sin(dtheta);
   
   stg_reloc((int)(xp + ps_x*c_theta),(int)(yp + ps_y*s_theta));

   for(j = 0;j < n;j++)  {
      INC_THETA;
      g_xc = g_reg[1].i = xscreen_to_pix*(xp + ps_x*c_theta) + 0.5;
      g_yc = g_reg[2].i = yscreen_to_pix*(yp + ps_y*s_theta) + 0.5;
      g_reg[E_IOP].i = 0;

      if(stg_encode(fd, g_mem, g_reg) == OK) { /* encode the point */
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
      }
   }
   
   if(fe[0] != '\0') {
      g_reg[E_IOP].i = 0;
      if(stg_encode(fe, g_mem, g_reg) == OK) {
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
      }
   }

   return(0);
}


int
stg_fill_polygon(style,x,y,n)
int style;
short *x,*y;
int n;
{
   char fs[SIZE],		/* Fill graphcap capabilities */
   	fd[SIZE],
   	fe[SIZE];
   int i;
   
   i = ttygets(g_tty,"FS",fs,SIZE);
   i += ttygets(g_tty,"FD",fd,SIZE);
   i += ttygets(g_tty,"FE",fe,SIZE);
   if(i == 0 || style != FILL_SOLID) {	/* No "fill" capability */
      return(-1);
   }
   if(n == 0) return(0);

   g_reg[E_IOP].i = 0;
   if(fs[0] != '\0') {
      if(stg_encode(fs, g_mem, g_reg) == OK) {
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
      }
   }

   stg_reloc(x[0],y[0]);

   for(i = 1;i < n;i++)  {
      g_xc = g_reg[1].i = xscreen_to_pix*x[i] + 0.5;
      g_yc = g_reg[2].i = yscreen_to_pix*y[i] + 0.5;
      g_reg[E_IOP].i = 0;

      if(stg_encode(fd, g_mem, g_reg) == OK) { /* encode the point */
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
      }
   }
   
   if(fe[0] != '\0') {
      g_reg[E_IOP].i = 0;
      if(stg_encode(fe, g_mem, g_reg) == OK) {
	 ttwrite(g_out, g_mem, g_reg[E_IOP].i);
      }
   }

   return(0);
}
