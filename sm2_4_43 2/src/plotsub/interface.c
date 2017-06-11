#include "copyright.h"
/*
 * Interface for Fortran to SM. Notice that these #definitions won't
 * handle 2 string variables -- see sm_format() for an example
 *
 * Also some functions needed by standalone SM (e.g. sm_identification()),
 * and some dummies for interactive SM functions (e.g. msg()).
 */
#include <stdio.h>
#include "options.h"
#include "devices.h"
#include "sm.h"
#include "defs.h"
#include "tree.h"
#include "sm_declare.h"

/*
 * Deal with fortran-C calling conventions. We must give
 * the fortran functions different names from the C ones. Most unix
 * fortran compilers add an extra trailing _ to fortran names, and
 * so the default values of FORTRAN_PREPEND and FORTRAN_APPEND are '' and '_'
 *
 * Pre-ansi CPPs cannot manage this, so we assume that FORTRAN_PREPEND
 * has the value of 'f' if defined, and that FORTRAN_APPEND is _ otherwise
 */
#if !defined(VMS)
#  if defined(ANSI_CPP)
#     if defined(FORTRAN_PREPEND)
#        if defined(FORTRAN_APPEND)
#           define DECLARE(F) \
		     CONCATENATE(FORTRAN_PREPEND,CONCATENATE(F,FORTRAN_APPEND))
#        else
#           define DECLARE(F) CONCATENATE(FORTRAN_PREPEND,F)
#        endif
#     else
#        if defined(FORTRAN_APPEND)
#           define DECLARE(F) CONCATENATE(F,FORTRAN_APPEND)
#        else
#           define DECLARE(F) F
#        endif
#     endif
#  else
#     if defined(FORTRAN_PREPEND)		/* box --> fbox etc. */
#        define DECLARE(F) CONCATENATE(f,F)
#     else					/* box --> box_ etc. */
#        define DECLARE(F) CONCATENATE(F,_)
#     endif
#  endif
#  define DEC_STRING(V) char *V; int len
#  define LEN ,len
#  define STRING(V) nullcpy(buff,V,len)
#else
#  include <descrip.h>			/* box --> fbox etc. */
#  define DECLARE(F) CONCATENATE(f,F)
#  define DEC_STRING(V) struct dsc$descriptor *V
#  define LEN				/* NoOp */
#  define STRING(V) strncpy(buff,V->dsc$a_pointer,V->dsc$w_length+1)
#endif

void sm_filetype();
void sm_gflush();
static char buff[100],			/* buffer for arguments */
       default_font[100] = "",		/* desired default font */
       graphcap[200],			/* graphcap file */
       file_type[80] = "C",		/* file type for 2-D reads */
       TeX_strings[20] = "",		/* use TeX-style strings? */
       x_gutter[20] = "",		/* x- and */
       y_gutter[20] = "";		/* y-multipliers for window spacing */
/*
 * make a null string from a Fortran string
 */
static char *
nullcpy(d,s,icnt)
char *d,			/* null terminated string (destination) */
     *s;			/* non-null terminated string (source) */
int icnt;			/* total length of source string */
{
   if(icnt > 0) {
      while(s[icnt - 1] == ' ')	icnt--;
      (void)strncpy(d,s,icnt);
   }
   d[icnt] = '\0';
   return(d);
}

/*
 * 2 functions concerned with calling the SM parser from a programme
 */
int
DECLARE(sm_parser)(args LEN)
DEC_STRING(args);
{
   return(sm_parser(STRING(args)));
}

int
DECLARE(sm_array_to_vector)(a,n,name LEN)
REAL *a;
int *n;
DEC_STRING(name);
{
   return(sm_array_to_vector(a,*n,STRING(name)));
}

/*
 * and now the regular SM functions
 */
void
DECLARE(sm_angle)(a)
REAL *a;
{
   REAL ang[1];			/* a is modified in set_angle, but
					   fortran may have passed a const */

   ang[0] = *a;
   set_angle(ang, 1);
}

void
DECLARE(sm_axis)(a1,a2,as,ab,ax,ay,alen,ilabel,iclock)
REAL *a1,*a2,*as,*ab;
int *ax,*ay,*alen,*ilabel,*iclock;
{
  sm_axis(*a1,*a2,*as,*ab,*ax,*ay,*alen,*ilabel,*iclock);
}

#if defined(VMS_BOX_BUG)		/* workaround VMS name pollution */
void
DECLARE(sm_box)(lab1,lab2,lab3,lab4)
int *lab1,*lab2,*lab3,*lab4;
{
   sm_box(*lab1,*lab2,*lab3,*lab4);
}
#else
void
DECLARE(sm_box)(lab1,lab2,lab3,lab4)
int *lab1,*lab2,*lab3,*lab4;
{
   sm_box(*lab1,*lab2,*lab3,*lab4);
}
#endif

void
DECLARE(sm_conn)(x,y,nxy)
REAL *x,*y;
int *nxy;
{
   sm_conn(x,y,*nxy);
}

void
DECLARE(sm_connect)(x,y,nxy)
REAL *x,*y;
int *nxy;
{
   sm_conn(x,y,*nxy);
}

void
DECLARE(sm_contour)()
{
   sm_contour();
}

void
DECLARE(sm_ctype)(ct LEN)
DEC_STRING(ct);
{
   sm_ctype(STRING(ct));
}


void
DECLARE(sm_ctype_i)(i)
int *i;
{
   sm_ctype_i(*i);
}

void
DECLARE(sm_set_ctypes)(vals, npt)
REAL *vals;
int *npt;
{
   sm_set_ctypes(vals, *npt);
}

/*
 * return current position of cursor, and key struck
 */
void
DECLARE(sm_curs)(x,y,key)
REAL *x,*y;
int *key;
{
   void sm_curs();
   
   sm_curs(x,y,key);
}

/*
 * define a variable
 */
#ifdef vms
void
DECLARE(sm_defvar)(name,value,len1,len2)
struct dsc$descriptor *name,*value;
int len1,len2;
{
   char buff0[100],buff2[100];		/* can't use buff, as sm_defvar is
					   called from sm_device() */
   void sm_defvar();

   nullcpy(buff0,name->dsc$a_pointer,name->dsc$w_length+1);
   nullcpy(buff2,value->dsc$a_pointer,value->dsc$w_length+1);
   sm_defvar(buff0,buff2);
}
#else
void
DECLARE(sm_defvar)(name,value,len1,len2)
char *name,*value;
int len1,len2;
{
   char buff0[100],buff2[100];		/* can't use buff, as sm_defvar is
					   called from sm_device() */

   nullcpy(buff0,name,len1);
   nullcpy(buff2,value,len2);
   sm_defvar(buff0,buff2);
}
#endif

/*
 * return a variable's value
 */
float
DECLARE(sm_get_variable)(name LEN)
DEC_STRING(name);
{
   return(sm_get_variable(STRING(name)));
}


/*
 * two functions to support images from fortran
 */
static float **z = NULL;			/* pointers to rows */

void
DECLARE(sm_defimage)(arr,x1,x2,y1,y2,nx,ny)
float *arr;
REAL *x1,*x2,*y1,*y2;
int *nx,*ny;
{
   int i;
   
   if((z = (float **)malloc((unsigned)(*ny)*sizeof(float *))) == NULL) {
      msg("Can't get storage in defimage\n");
      z = NULL;
      return;
   }
   for(i = 0;i < *ny;i++) {
      z[i] = &arr[i*(*nx)];
   }
   sm_defimage(z,*x1,*x2,*y1,*y2,*nx,*ny);
}

void
DECLARE(sm_delimage)()
{
   sm_delimage();
   if(z != NULL) {
      free((char *)z);
      z = NULL;
   }
}

void
DECLARE(sm_dot)()
{
   sm_dot();
}

void
DECLARE(sm_draw)(x,y)
REAL *x,*y;
{
   sm_draw(*x,*y);
}

void
DECLARE(sm_erase)()
{
   ERASE();
}

void
DECLARE(sm_errorbar)(x,y,err,dir,nxy)
REAL *x,*y,*err;
int *dir,
    *nxy;
{
   sm_errorbar(x,y,err,*dir,*nxy);
}

void
DECLARE(sm_expand)(ex)
REAL *ex;
{
   set_expand(ex, 1);
}

#ifdef vms
void
DECLARE(sm_format)(fx,fy,len1,len2)
struct dsc$descriptor *fx,*fy;
int len1,len2;
{
   char buff2[100];

   nullcpy(buff,fx->dsc$a_pointer,fx->dsc$w_length+1);
   nullcpy(buff2,fy->dsc$a_pointer,fy->dsc$w_length+1);
   sm_format(buff,buff2);
}
#else
void
DECLARE(sm_format)(fx,fy,len1,len2)
char *fx,*fy;
int len1,len2;
{
   char buff2[100];

   nullcpy(buff,fx,len1);
   nullcpy(buff2,fy,len2);
   sm_format(buff,buff2);
}
#endif

void
DECLARE(sm_filetype)(type LEN)
DEC_STRING(type);
{
   sm_filetype(STRING(type));
}

void
DECLARE(sm_gflush)()
{
   sm_gflush();
}

void
DECLARE(sm_hardcopy)()
{
   IDLE();
   (void)sm_device("nodevice");
}

void
DECLARE(sm_histogram)(x,y,nxy)
REAL *x,*y;
int *nxy;
{
   sm_histogram(x,y,*nxy);
}

void
DECLARE(sm_label)(s LEN)
DEC_STRING(s);
{
   sm_label(STRING(s));
}

void
DECLARE(sm_identification)(str LEN)
DEC_STRING(str);
{
   sm_identification(STRING(str));
}

int
DECLARE(sm_device)(name LEN)
DEC_STRING(name);
{
   return(sm_device(STRING(name)));
}

void
DECLARE(sm_grelocate)(x,y)
int *x,*y;
{
   sm_grelocate((float)*x,(float)*y);
}

void
DECLARE(sm_grid)(i,j)
int *i,*j;
{
   sm_grid(*i,*j);
}

void
DECLARE(sm_levels)(vals,n)
REAL *vals;
int *n;
{
   sm_levels(vals,*n);
}

void
DECLARE(sm_limits)(x1,x2,y1,y2)
REAL *x1,*x2,*y1,*y2;
{
   sm_limits(*x1,*x2,*y1,*y2);
}

void
DECLARE(sm_location)(x1,x2,y1,y2)
int *x1,*x2,*y1,*y2;
{
   sm_location(*x1,*x2,*y1,*y2);
}

void
DECLARE(sm_ltype)(lt)
int *lt;
{
   sm_ltype(*lt);
}

void
DECLARE(sm_lweight)(lw)
REAL *lw;
{
   sm_lweight(*lw);
}

void
DECLARE(sm_minmax)(x,y)
REAL *x,*y;
{
   float fx, fy;
   
   sm_minmax(&fx, &fy);			/* pass by address */
   *x = fx; *y = fy;
}

void
DECLARE(sm_notation)(a1,a2,as,ab)
REAL *a1,*a2,*as,*ab;
{
   sm_notation(*a1,*a2,*as,*ab);
}

void
DECLARE(sm_plotsym)(x,y,nxy,sym,nsym)
REAL *x,*y;
int *nxy,
    *sym,
    *nsym;
{
   sm_plotsym(x,y,*nxy,sym,*nsym);
}

void
DECLARE(sm_points)(x,y,nxy)
REAL *x,*y;
int *nxy;
{
   sm_points(x,y,*nxy);
}

void
DECLARE(sm_ptype)(style,ns)
REAL *style;
int *ns;
{
   sm_ptype(style,*ns);
}

void
DECLARE(sm_putlabel)(n,str LEN)
int *n;
DEC_STRING(str);
{
   sm_putlabel(*n,STRING(str));
}

void
DECLARE(sm_readimage)(name,x1,x2,y1,y2 LEN)
DEC_STRING(name);
REAL *x1,*x2,*y1,*y2;
{
   read_image(STRING(name),0,*x1,*x2,*y1,*y2, 0, 0, 0, 0);
}

void
DECLARE(sm_redraw)(fd)
int *fd;
{
   REDRAW(*fd);
}

void
DECLARE(sm_page)()
{
   PAGE();
}

void
DECLARE(sm_relocate)(x,y)
REAL *x,*y;
{
   sm_relocate(*x,*y);
}

void
DECLARE(sm_shade)(delta,x,y,nxy,type)
int *delta;
REAL *x,*y;
int *nxy,
    *type;				/* shade as what? */
{
   sm_shade(*delta,x,y,*nxy,*type);
}

void
DECLARE(sm_ticksize)(xs,xb,ys,yb)
REAL *xs,*xb,*ys,*yb;
{
   sm_ticksize(*xs,*xb,*ys,*yb);
}

void
DECLARE(sm_toscreen)(xu,yu,xs,ys)
REAL *xu,*yu;				/* User coordinates */
int *xs,*ys;				/* screen coordinates */
{
   sm_toscreen(*xu,*yu,xs,ys);
}

void
DECLARE(sm_alpha)()
{
   IDLE();
}

void
DECLARE(sm_graphics)()
{
   ENABLE();
}

void
DECLARE(sm_window)(nx,ny,kx,ky,kx2,ky2)
int *nx,*ny,*kx,*ky,*kx2,*ky2;
{
   sm_window(*nx,*ny,*kx,*ky,*kx2,*ky2);
}

void
DECLARE(sm_xlabel)(s LEN)
DEC_STRING(s);
{
   sm_xlabel(STRING(s));
}

void
DECLARE(sm_ylabel)(s LEN)
DEC_STRING(s);
{
   sm_ylabel(STRING(s));
}

/*
 * Now some functions that we need.
 * print_var will only return the value of variables known to sm_defvar()
 *
 * The rest are C functions for non-interactive SM
 */

void
sm_alpha()
{
   IDLE();
}

void
sm_curs(x,y,key)
REAL *x,*y;
int *key;
{
   int px,py;				/* SCREEN coords of cursor */
   
   *key = CURSOR(0,&px,&py);		/* read the position */
   *x = (px - ffx)/fsx;			/* SCREEN --> user coords */
   *y = (py - ffy)/fsy;
}

/**************************************************/
/*
 * Initialise sm_device `name'
 */
int
sm_device(name)
char *name;
{
   static int first = 1;

   if(first) {
      first = 0;
      set_defaults_file((char *)NULL,(char *)NULL); /* set the .sm path */
      load_font(NULL);			/* indeed, load fonts */
      init_colors();			/* indeed, initialise colours */
      sm_defvar("graphcap",get_val("graphcap"));
      if (default_font[0] == '\0') {
	  sm_defvar("default_font",get_val("default_font"));
      }
/*
 * Now two functions to persuade the loader to get some definitions from a
 * library
 */
      declare_devs();			/* device definitions */
      declare_vars();			/* various variables */
#ifdef vms
      if(first == 2) {			/* never called - used to get
      						system from plotsub.olb */
	 system("");
      }
#endif
      (void)sm_device("nodevice");
   }

   if(set_device(name) < 0) {
      return(-1);
   }
   
   ENABLE(); set_dev(); IDLE();
   return(0);
}

void
sm_erase(int raise_on_erase)
{
   ERASE(raise_on_erase);
}

void
sm_filetype(type)
char *type;
{
   sm_defvar("file_type",type);
}

void
sm_gflush()
{
   GFLUSH();
}

void
sm_graphics()
{
   ENABLE();
}

/*
 * Get sm_hardcopy by closing current sm_device
 */
void
sm_hardcopy()
{
   IDLE();
   sm_device("nodevice");
}

void
sm_identification(lab)
char *lab;
{
   char *date,
	str[26 + 100];		/* leave room for date */
   REAL angsave, exsave,
        ex,
	zero = 0.0;
   TIME_T tim;

   angsave = aangle;
   exsave = eexpand;
   sm_ltype(0);
   ex = 0.5*eexpand;
   set_expand(&ex,1);
   set_angle(&zero,1);
   xp = gx2;
   yp = gy2 + 0.5*(SCREEN_SIZE - gy2);
   time(&tim);			/* get time */
   date = ctime(&tim);		/* and then fine date */
   date[24] = '\0';		/* remove newline */
   (void)sprintf(str,"%s : %s",lab,date);
   sm_putlabel(4,str);
   set_angle(&angsave,1);
   set_expand(&exsave,1);
}
/*
 * Convert user to screen coordinates
 */
void
sm_toscreen(xu,yu,xs,ys)
double xu,yu;				/* User coordinates */
int *xs,*ys;				/* screen coordinates */
{
   double dxs = ffx + fsx * xu;
   double dys = ffy + fsy * yu;
   int x = dxs, y = dys;
/*
 * Worry that converting e.g. 902315536159.6 to long can result in
 * -2147483648; e.g. gcc 2.95 under Linux
 */
   if(x < 0 && dxs > 0) {
      x = -(x + 1);			/* out of range of long */
   }
   if(y < 0 && dys > 0) {
      y = -(y + 1);			/* out of range of long */
   }

   *xs = x;
   *ys = y;
}

/*****************************************************************/
/*
 * A few functions where the SM calling sequence is different from
 * that advertised
 */
void
sm_angle(aa)
double aa;
{
   REAL a = aa;

   set_angle(&a,1);
}

void
sm_expand(ee)
double ee;
{
   REAL e = ee;
   
   set_expand(&e,1);
}

void
sm_page()
{
   PAGE();
}

void
sm_redraw(fd)
int fd;
{
   REDRAW(fd);
}

/*
 * Define a variable in non-interactive SM, and print the corresponding
 * value. We have to play silly games with i_print_var so as to avoid
 * pulling in dummies.o when we are linking in all of SM with libparser.o
 */
void
sm_defvar(name,value)
char *name,*value;
{
   if(value == NULL) value = "";
   
   if(!strcmp("file_type",name)) {
      strcpy(file_type,value);
   } else if(!strcmp("default_font",name)) {
      strcpy(default_font,value);
   } else if(!strcmp("graphcap",name)) {
      strcpy(graphcap,value);
   } else if(!strcmp("TeX_strings",name)) {
      strcpy(TeX_strings,value);
   } else if(!strcmp("x_gutter",name)) {
      strcpy(x_gutter,value);
   } else if(!strcmp("y_gutter",name)) {
      strcpy(y_gutter,value);
   } else {
      msg_1s("You can't define %s in non-interactive SM\n",name);
   }
}

char *
i_print_var(name)
char *name;
{
   int i;
   
   if(!strcmp(name,"file_type")) {
      return(file_type);
   } else if(!strcmp("default_font",name)) {
      return(default_font);
   } else if(!strcmp("graphcap",name)) {
      return(graphcap);
   } else if(!strcmp(name,"TeX_strings")) {
      return(TeX_strings);
   } else if(!strcmp(name,"x_gutter")) {
      return(x_gutter);
   } else if(!strcmp(name,"y_gutter")) {
      return(y_gutter);
   } else if((i = index_variable(name)) >= 0) {	/* an SM internal variable */
      return(get_variable(i));
   } else {
      return("");
   }
}

float
sm_get_variable(name)
char *name;
{
   return(atof(i_print_var(name)));
}
