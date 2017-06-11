#include "copyright.h"
/*********************************************************************/
/*                            SET.C                                  */
/*     routines to modify the global plotting parameters             */
/*-------------------------------------------------------------------*/
/*       ltype(arg)                                            	     */
/*	 save_ctype(s)                                               */
/*       lweight(arg)                                            */
/*       set_angle(arg,n)                                              */
/*       set_expand(arg)                                             */
/*       limits(ax1,ax2,ay1,ay2)                                     */
/*       location(ax1,ax2,ay1,ay2)                                   */
/*       set_scale()                                                 */
/*	 get_variable(i)					     */
/*********************************************************************/
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "defs.h"
#include "sm.h"
#include "devices.h"
#include "vectors.h"
#include "sm_declare.h"

extern int sm_verbose;			/* how verbose should I be? */
static int cctype = 0;
static int exit_status = 0;
static float distance = 0.0, theta = 0.01, phi = 0.0;
/*
 * Reset line properties for a new device
 * calling set_expand will set cheight/cwidth correctly
 * for the current expansion
 */
void
set_dev()
{
   char *color;

   sm_ltype(lltype);
   if(llweight < 0) {			/* ldef should have been increased */
      ldef = -llweight*ldef + 0.5;	/* lweight() starts by /= -llweight */
   }
   sm_lweight(llweight);

   if(*(color = print_var("foreground")) != '\0') {
      default_ctype(color);
   }
   set_color((VECTOR *)NULL);
   sm_ctype_i(cctype);
   set_expand(expand_vec,n_expand_vec);
}

/*
 * Force a given aspect ratio. This only applies until the device is
 * changed
 */
void
set_aspect(asp)
double asp;
{
   if(asp == 0.0) {
      msg_1f("Current aspect ratio is %g\n",aspect);
      return;
   }
   aspect = asp;
}

/*
 * Set a linetype. Linetypes supported in hardware are set to be -ve
 * Note that there are two special values: 10 (erase lines), and 11
 * (stop erasing lines). Ltype 11 is for internal use only.
 */
void
sm_ltype(arg)
int arg;
{
    reset_chopper();			/* reset the line chopper */

    if(arg < 0) arg = -arg;
    if(lltype == -10 && arg != 10) {	/* turning off erase mode */
       (void)LTYPE(11);			/* notify device drivers */
    }
    if(arg < 7) {
       lltype = arg;
       if(fabs(lltype_expand - 1) > 1e-5) {
	  (void)LTYPE(0);
       } else {
	  if(LTYPE(lltype) == 0) lltype = -lltype;
       }
    } else if(arg == 10) {		/* erase line */
       if(LTYPE(arg) != 0) {
	  msg("I can't erase lines, sorry.\n");
	  if(lltype == -10) {		/* set to delete by another device */
	     sm_ltype(0);
	  }
       } else {
          lltype = -arg;		/* MUST be -ve! lt 10 doesn't exist */
       }
    } else {
       msg_1d("Unknown ltype: %d\n",arg);
    }
}

void
save_ctype(i)
int i;			/* desired colour */
{
   cctype = i;
}

int
current_ctype()
{
   return(cctype);
}

/*
 * Set a lineweight. Lineweights supported in hardware are set to be -ve
 */
void
sm_lweight(arg)
double arg;
{
   if(arg < 0) arg = -arg;

   if(llweight < 0) {			/* correct ldef back to lweight 1 */
      ldef /= -llweight;
   }
   
   llweight = arg;

   if(LWEIGHT(llweight) == 0) {
      ldef = llweight*ldef + 0.5;	/* lines thicker now */
      llweight = -llweight;
   }
}

void
set_angle(arg, n)
REAL *arg;
int n;
{
   static int angle_vec_size = 0;

   if(n > angle_vec_size) {
      if(angle_vec != NULL) free((char *)angle_vec);
      if((angle_vec = (REAL *)malloc(n*sizeof(REAL))) == NULL) {
	 fprintf(stderr,"Can't allocate storage for angle_vec\n");
	 angle_vec_size = 0;
      }
      angle_vec_size = n;
   }
   n_angle_vec = n;
   aangle = *arg;
   (void)memcpy((Void *)angle_vec,(Const Void *)arg,n_angle_vec*sizeof(REAL));

   sm_sin = sin(*arg*PI/180);
   sm_cos = cos(*arg*PI/180);
   set_eexpand(eexpand);		/* get proper character sizes */
}

void
set_expand(arg,n)
REAL *arg;
int n;
{
   static int expand_vec_size = 0;
   REAL real_eexpand;

   if(arg == NULL) {
      real_eexpand = eexpand;
      arg = &real_eexpand;
      n = 1;
   }
   if(*arg == 0.0) {
      *arg = 1e-6;
   } else if(*arg < 0.0) {
      msg("Negative expansions are not allowed\n");
      return;
   }

   if(n > expand_vec_size) {
      if(expand_vec != NULL) free((char *)expand_vec);
      if((expand_vec = (REAL *)malloc(n*sizeof(REAL))) == NULL) {
	 fprintf(stderr,"Can't allocate storage for expand_vec\n");
	 expand_vec_size = 0;
      }
      expand_vec_size = n;
   }
   n_expand_vec = n;
   (void)memcpy((Void *)expand_vec,(Const Void *)arg,
		n_expand_vec*sizeof(REAL));

   set_eexpand(*arg);
}

/*
 * set an expand for ltype
 */
void
sm_set_ltype_expand(val)
double val;
{
   lltype_expand = val;
   sm_ltype(lltype >= 0 ? lltype : -lltype);
}

/*
 * do the work of setting an expand value; no effect upon expansion vectors
 */
void
set_eexpand(val)
double val;
{
   eexpand = val;
   cheight = CDEF*CHEIGHT*eexpand;
   cwidth = CDEF*CWIDTH*eexpand;
   (void)CHAR((char *)NULL,0,0);	/* give dev_char a chance to adjust
					   character sizes for hardware */
}


int
sm_limits(ax1,ax2,ay1,ay2)
double ax1,ax2,ay1,ay2;
{
    if (ax1 != ax1) {			/* NaN */
	msg("Lower x-axis limit == NaN\n");
	return(-1);
    }
    if (ax1 == ax1 + 1) {			/* +-Inf */
	msg("Lower x-axis limit == +-Inf\n");
	return(-1);
    }
    if (ax2 != ax2) {			/* NaN */
	msg("Upper x-axis limit == NaN\n");
	return(-1);
    }
    if (ax2 == ax2 + 1) {			/* +-Inf */
	msg("Upper x-axis limit == +-Inf\n");
	return(-1);
    }

    if (ay1 != ay1) {			/* NaN */
	msg("Lower y-axis limit == NaN\n");
	return(-1);
    }
    if (ay1 == ay1 + 1) {			/* +-Inf */
	msg("Lower y-axis limit == +-Inf\n");
	return(-1);
    }
    if (ay2 != ay2) {			/* NaN */
	msg("Upper y-axis limit == NaN\n");
	return(-1);
    }
    if (ay2 == ay2 + 1) {			/* +-Inf */
	msg("Upper y-axis limit == +-Inf\n");
	return(-1);
    }

   if(ax1 != ax2) {
      fx1=ax1; fx2=ax2;
   }
   if(ay1 != ay2) {
      fy1=ay1; fy2=ay2;
   }
   set_scale();
   return(0);
}

void
set_scale()
{
    fsx = (gx2-gx1)/(fx2-fx1);
    fsy = (gy2-gy1)/(fy2-fy1);
    ffx = gx1 - fsx*fx1;
    ffy = gy1 - fsy*fy1;
}

/*
 * set the exit status from a ! command
 */
void
set_exit_status(i)
int i;
{
   exit_status = i;
}

void
set_viewpoint_params(d,t,p)
double d;				/* distance */
double t,p;				/* theta, phi */
{
   distance = d;
   theta = t;
   phi = p;
}

/*******************************************/
/*
 * Deal with SM internal variables
 */
#define NONE 0
#define ABS_INT 1
#define FLOAT 2
#define ABS_FLOAT 3
#define INT 4
#define PTYPE 5
#define UXP 6
#define UYP 7
#define DATE 8
#define NX 9
#define NY 10
#if defined(BGI)
#  define CORELEFT 11
#endif
#define STR 12
#define EXPAND 13
#define ANGLE 14
#define CWD 15

static struct {
   char name[40];
   Void *value;
   char type;
} var[] = {
   { "angle", NULL, ANGLE },
   { "aspect", (Void *)&aspect, FLOAT },
   { "cheight", (Void *)&cheight, FLOAT },
#if defined(CORELEFT)
   { "coreleft", NULL, CORELEFT },
#endif
   { "ctype", (Void *)&cctype, INT },
   { "cwd", NULL, CWD },
   { "cwidth", (Void *)&cwidth, FLOAT },
   { "date", NULL, DATE },
   { "distance", (Void *)&distance, FLOAT },
   { "exit_status", (Void *)&exit_status, INT },
   { "expand", NULL, EXPAND },
   { "fx1", (Void *)&fx1, FLOAT },
   { "fx2", (Void *)&fx2, FLOAT },
   { "fy1", (Void *)&fy1, FLOAT },
   { "fy2", (Void *)&fy2, FLOAT },
   { "gx1", (Void *)&gx1, INT },
   { "gx2", (Void *)&gx2, INT },
   { "gy1", (Void *)&gy1, INT },
   { "gy2", (Void *)&gy2, INT },
   { "label_offset", (Void *)&label_offset_scale, FLOAT },
   { "ltype", (Void *)&lltype, ABS_INT },
   { "ltype_expand", (Void *)&lltype_expand, ABS_FLOAT },
   { "lweight", (Void *)&llweight, ABS_FLOAT },
   { "nx", NULL, NX },
   { "ny", NULL, NY },
   { "phi", (Void *)&phi, FLOAT },
   { "ptype", NULL, PTYPE },
   { "sdepth", (Void *)&s_depth, INT },
   { "sheight", (Void *)&s_height, INT },
   { "swidth", (Void *)&s_width, INT },
   { "theta", (Void *)&theta, FLOAT },
   { "uxp", NULL, UXP },
   { "uyp", NULL, UYP },
   { "verbose", (Void *)&sm_verbose, INT },
   { "version", (Void *)&version_string, STR },
   { "xp", (Void *)&xp, INT },
   { "yp", (Void *)&yp, INT },
   { "", NULL, NONE }
};

/************************************************************/
/*
 * Return the index number of an SM variable
 * Should do a binary search, but the table's too small to be worth it
 */
int
index_variable(name)
char *name;
{
   int i;

   for(i = 0;name[i] != '\0';i++) {
      if(isupper(name[i])) name[i] = tolower(name[i]);
   }
   
   for(i = 0;var[i].name[0] != '\0';i++) {
      if(!strcmp(name,var[i].name)) {
	 return(i);
      }
   }
   return(-1);
}

/************************************************************/
/*
 * Return an SM variable
 */
char *
get_variable(which)
int which;				/* which one do we want? */
{
   char *ptr;
   TIME_T tim;				/* time since 1970 */
   static char value[180];

   value[0] = '\0';

   switch(var[which].type) {
    case ABS_FLOAT:
      (void)sprintf(value,"%g",fabs(*(float *)var[which].value));
      break;
    case ABS_INT:
      (void)sprintf(value,"%d",abs(*(int *)var[which].value));
      break;
    case ANGLE:
      if(n_angle_vec > 1) {
	 (void)sprintf(value,"%g %d", aangle, n_angle_vec);
      } else {
	 (void)sprintf(value,"%g", aangle);
      }
      break;
#if defined(CORELEFT)
    case CORELEFT:
      (void)sprintf(value,"%ld",coreleft());
      break;
#endif
    case CWD:
      (void)getcwd(value, sizeof(value)-1);
      break;
    case DATE:
      (void)time(&tim);
      ptr = ctime(&tim);		/* date as a string */
      ptr += 4;				/* skip day-of-week */
      ptr[20] = '\0';			/* trim off '\n' */
      (void)sprintf(value,"%s",ptr);
      break;
    case EXPAND:
      if(n_expand_vec > 1) {
	 (void)sprintf(value,"%g %d", eexpand, n_expand_vec);
      } else {
	 (void)sprintf(value,"%g", eexpand);
      }
      break;
    case FLOAT:
      (void)sprintf(value,"%g",*(float *)var[which].value);
      break;
    case INT:
      (void)sprintf(value,"%d",*(int *)var[which].value);
      break;
    case NX:
      (void)sprintf(value,"%d",(int)(xscreen_to_pix*SCREEN_SIZE + 0.5));
      break;
    case NY:
      (void)sprintf(value,"%d",(int)(yscreen_to_pix*SCREEN_SIZE + 0.5));
      break;
    case PTYPE:
      (void)sprintf(value,whatis_ptype());
      break;
    case STR:
      strncpy(value,*(char **)var[which].value,80);
      break;
    case UXP:
      (void)sprintf(value,"%g",(xp - ffx)/fsx);
      break;
    case UYP:
      (void)sprintf(value,"%g",(yp - ffy)/fsy);
      break;
    default:
      msg_1s("Unknown internal variable %s\n",var[which].name);
      value[0] = '\0';
      break;
   }
   return(value);
}

/*****************************************************************************/
/*
 * list internal variables
 */
void
list_internal_vars()
{
   int i;

   for(i = 0;var[i].type != NONE;i++) {
      msg_2s("%-10s %s",var[i].name,get_variable(i));
      if(more("\n") < 0) break;
   }
}

/**********************************************************/
/*
 * Set all internal variables
 */
void
set_all_internal()
{
   int i;

   for(i = 0;var[i].type != NONE;i++) {
      setvar_internal(var[i].name);
   }
}
