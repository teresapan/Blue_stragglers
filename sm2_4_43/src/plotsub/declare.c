#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "sm.h"
#include "sm_declare.h"
#define X1 3500				/* Screen origin of boxes is (X1,Y1) */
#define Y1 3500
#define X2 31000			/* Screen top right is (X2,Y2) */
#define Y2 31000

float aangle = 0.,			/* rotation of chars and points */
      aspect = 1.0,			/* aspect ratio of screen, y/x */
      cheight = 1.0,			/* official height of characters */
      cwidth = 1.0,			/* official width of characters */
      eexpand = 1.0,			/* Global expansion of chars, pts */
      sm_cos = 1., sm_sin = 0.,		/* cos(aangle), sin(aangle) */
      fx1 = 0.,				/* User coords of graphics area */
      fx2 = 1.,
      fy1 = 0.,
      fy2 = 1.,
      fsx = (float)(X2 - X1),		/* fsx = (gx2-gx1)/(fx2-fx1) */
      fsy = (float)(Y2 - Y1),
      ffx = (float)X1,			/* ffx = gx1 - fsx*fx1,  etc. */
      ffy = (float)Y1,
      llweight = 1.0,			/* Line weight */
      lltype_expand = 1,		/* expansion for ltypes */
      xscreen_to_pix = 1,			/* scale SCREEN to window coords */
      yscreen_to_pix = 1;
int   gx1 = X1,				/* Device coords of graphics area */
      gx2 = X2,
      gy1 = Y1,
      gy2 = Y2,
      n_angle_vec = 0,			/* dimension of angle_vec */
      n_expand_vec = 0,			/* dimension of expand_vec */
      ldef = 32,			/* scale spacing for lweight>1 lines */
      lltype = 0,			/* Line type: dotted, etc. */
      termout = 1;			/* True if plotting on terminal */
long xp = 0, yp = 0;			/* Present plot position */
REAL *angle_vec = NULL,			/* vector of angles */
     *expand_vec = NULL;		/* vector of expansions */
/*
 * Now a function to persuade the loader to extract this file from the library
 */
void declare_vars() { ; }
