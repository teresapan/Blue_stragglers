/* -*- c -*- */

%define smLib_DOCSTRING
"
Low-level bindings for SM; you should be importing sm not smLib
"
%enddef

%module(docstring=smLib_DOCSTRING) smLib

%feature("autodoc", "1");

//%rename("%(command:perl -pe 's/^sm_(.)/\l$1/' <<< )s") "";

%{
#  include <stdio.h> 
/*
 * SM
 */
#  include "options.h"
#  include "vectors.h"
#  include "sm_declare.h"
#  include "sm.h"
/*
 * Numarray
 */
#include <numpy/arrayobject.h>

static int dimensions_match = 0;	/* did all arrays dimensions match? */
%}

#if SWIG_VERSION <= 0x010328		/* == 1.3.28 */
%include "exception.i"
#else
%include "typemaps/exception.swg"
%define SWIG_exception(CODE, MSG)
   %error(CODE, MSG)
%enddef
#endif
%import "numarray.i";

%ignore angle_vec;
%ignore expand_vec;
%ignore CDEF;
%ignore CHEIGHT;
%ignore COLOR;
%ignore COLORPtr;
%ignore CWIDTH;
%ignore FILL_DOTS;
%ignore FILL_SOLID;
%ignore MAX_POLYGON;
%ignore PDEF;
%ignore PI;
%ignore PROGNAME;
%ignore SCREEN_SIZE;

%include "sm.h"

%init %{
    import_array();			/* for numPy */
%}

/************************************************************************************************************/
/*
 * Define a helper function that checks that both arguments have the same dimension
 */
%define CHECK_DIMENSIONS(FUNC);
   %exception sm_##FUNC##_helper {
      $action;
      if (!dimensions_match) {
	 SWIG_exception(SWIG_IndexError, "Arrays' dimensions differ");
      }
   }
%enddef

%define TWO_VECTOR_FUNC(FUNC)
   %{
   static void sm_##FUNC##_helper(REAL x[], int nx, REAL y[], int ny) {
      dimensions_match = (nx == ny) ? 1 : 0;
      if (dimensions_match) {
	 sm_##FUNC(x, y, nx);
      }
   }
   %}

   CHECK_DIMENSIONS(FUNC);

   %rename(FUNC) sm_##FUNC##_helper;
   static void sm_##FUNC##_helper(REAL [], int, REAL [], int);
%enddef

%define THREE_VECTOR_FUNC(FUNC)
   %{
   static void sm_##FUNC##_helper(REAL x[], int nx, REAL y[], int ny, REAL l[], int nl) {
      dimensions_match = (nx == ny || ny != nl) ? 1 : 0;
      if (dimensions_match) {
	 sm_##FUNC(x, y, l, nx);
      }
   }
   %}

   CHECK_DIMENSIONS(FUNC);

   %rename(FUNC) sm_##FUNC##_helper;
   static void sm_##FUNC##_helper(REAL [], int, REAL [], int, REAL [], int);
%enddef

/************************************************************************************************************/

%rename(box) sm_box;		void sm_box(int = 1, int = 2, int = 0, int = 0);

%typemap(arginit) (const char **, const int) {
  $1 = NULL;
}
%typemap(in) (const char **, const int) {
    /* Check if is a list */
    int i;
    $1 = NULL;
    if ($input == Py_None) {
	$1 = NULL; $2 = 0;
    } else if (PyList_Check($input) || PyTuple_Check($input)) {
        int input_is_list = PyList_Check($input);
        $2 = (input_is_list) ? PyList_Size($input) : PyTuple_Size($input);
	if ($2 > 0) {
	    $1 = malloc($2*sizeof(void *));
	    if ($1 == NULL) {
	       PyErr_SetString(PyExc_TypeError, "Failed to malloc pointer array");
	       return NULL;
	    }
	
	    for (i = 0; i < $2; i++) {
		PyObject *o =  (input_is_list) ? PyList_GetItem($input,i) : PyTuple_GetItem($input,i);
	    
		if (PyString_Check(o))
		   $1[i] = PyString_AsString(PyList_GetItem($input,i));
		else {
		   PyErr_SetString(PyExc_TypeError,"list must contain strings");
		   free($1);
		   return NULL;
		}
	    }
	}
    } else {
	PyErr_SetString(PyExc_TypeError,"not a list or tuple");
	return NULL;
    }
}

%typemap(freearg) (const char **, const int) {
    free($1);
}

%{
   static void sm_axis_helper(REAL small[], int nsmall, REAL big[], int nbig,
			      int ilabel, int iclock,
			      const char **lab, int const nlab,
			      double a1, double a2,
			      int ax, int ay, int alen) {
      dimensions_match = (nbig == nlab || nlab == 0) ? 1 : 0;
      if (dimensions_match) {
	 const int horizontal = (fabs(aangle) < 45 || fabs(aangle - 180) < 45) ? 1 : 0;	/* is axis horizontal? */
	 VECTOR vsmall, vbig, vlab;

	 vec_init(&vsmall, VEC_FLOAT);
	 vsmall.name = "small"; vsmall.dimen = nsmall; vsmall.vec.f = small;
	 vec_init(&vbig, VEC_FLOAT);
	 vbig.name = "big"; vbig.dimen = nbig; vbig.vec.f = big;
	 vec_init(&vlab, VEC_STRING);
	 vlab.name = "lab"; vlab.dimen = nlab; vlab.vec.s.s_s = NULL; vlab.vec.s.s = (char **)lab;

	 if (a1 == a2) {
	    if (horizontal) {
	       a1 = fx1; a2 = fx2;
	    } else {
	       a1 = fy1; a2 = fy2;
	    }
	 }

	 if (ax < 0) {
	    ax = gx1;
	 }
	 if (ay < 0) {
	    ay = gy1;
	 }
	 if (alen < 0) {
	    alen = (horizontal) ? gx2 - gx1 : gy2 - gy1;
	 }

	 vec_axis(a1, a2, &vsmall, &vbig, nlab == 0 ? NULL : &vlab, ax, ay, alen, ilabel, iclock);
      }
   }
%}
CHECK_DIMENSIONS(axis);
%feature("docstring") sm_axis_helper {

Makes an axis labeled from a1 to a2 at location ax, ay, of length alen.
The first two arguments specify where you want small and big ticks;
the fifth argument labels (if present) is a list of strings that will be used to label the big ticks.

Argument ilabel is 0 for no labels, 1 for labels parallel to axis, 2 for
perpendicular to axis, and 3 for neither labels nor ticks.
					    
Use angle() to determine the angle of the axis.

Argument iclock is used for (too) many purposes; it iss treated as three
integers of 1, 2, and 1 bits respectively.

ICLOCK      & 0x1:      Control the ticks orientation:
                        0       Anticlockwise from the axis
                        1       Clockwise from the axis
(ICLOCK>>1) & 0x3:      Control the tick direction:
                        0       Ticks are perpendicular to axis
                        1       Ticks are vertical
                        2       Ticks are vertical
                        3       Do not draw any ticks
(ICLOCK>>3) & 0x1:      Control which side of the axis the ticks appear:
                        0       The same size as the labels
                        1       The opposite side to the labels
}

%apply (REAL [], int) { (REAL big[], int) };
%apply (REAL [], int) { (REAL small[], int) };
%apply (const char **, const int) { (const char **labels, const int) };

%rename(axis) sm_axis_helper;
static void sm_axis_helper(REAL small[], int, REAL big[], int, 
			   int ilabel, int iclock, const char **labels = NULL, const int = 0,
			   double a1 = 0.0, double a2 = 0.0,
			   int ax = -1, int ay = -1, int alen = -1);

%exception sm_device {
   $action
   if(result < 0) {
      SWIG_exception(SWIG_SystemError, "Unknown SM device");
   }
}
%rename(device) sm_device;	int sm_device(char *device);

%rename(defvar) sm_defvar;	void sm_defvar(char *variable, char *value);
%pythoncode {
   defvar("TeX_strings", "1")
}
%rename(location) sm_location;	void sm_location(int x0 = 3500, int x1 = 31000, int y0 = 3500, int y1 = 31000);
%rename(graphics) sm_graphics;	void sm_graphics(void);
%feature("docstring") sm_window {
   Subdivide plotting window into NXxNY regions, and choose the (KX, KY)th (1 <= KZ <= NZ)
   If KZ2 is >= 0, use the regions from KX ... KX2
}
%rename(window) sm_window;	void sm_window(int nx = 1, int ny = 1, int kx = 1, int ky = 1, int kx = -1, int ky = -1);
%rename(limits) sm_limits;	void sm_limits(double, double, double, double);
%rename(ticksize) sm_ticksize;	void sm_ticksize(double, double, double, double);
%feature("docstring") sm_notation {
   Use exponential notation when labelling axis points outside the specified range.
   notation() resets the default.
}
%rename(notation) sm_notation;
void sm_notation(double xmin = 0, double xmax = 0, double ymin = 0, double ymax = 0);

%rename(expand) sm_expand;	void sm_expand(double expand = 1);
%rename(gflush) sm_gflush;	void sm_gflush(void);

#if SWIG_VERSION >= 0x010323		/* == 1.3.23 */
%apply double *OUTPUT { REAL *xp};
%apply double *OUTPUT { REAL *yp};
%apply char *OUTPUT { int *key};
%feature("autodoc", "cursor()") sm_curs;
%feature("docstring") sm_curs {
Pop up the cursor, wait for a keypress, and return the list (x, y, key)
}
%rename(cursor) sm_curs;	void sm_curs(REAL *xp, REAL *yp, int *key);
#else
%pythoncode {
def cursor():
   raise NotImplementedError, "cursor is only supported with swig versions later than 1.3.22"
}
#endif

%rename(erase) sm_erase;	void sm_erase(int = 1);
%rename(page) sm_page;	void sm_page(void);

%rename(angle) sm_angle;	void sm_angle(double = 0);

%{
   static const REAL forty_one = 41;
%}
%feature("autodoc", "ptype(ptype = [41])") sm_ptype;
%feature("docstring") sm_ptype {
   Set the point type to the specified list or array

   Ptype ns causes points to be drawn as n sided polygons of a style s,
where s refers to:
        0 = open 
        1 = skeletal (center connected to vertices)
        2 = starred
        3 = solid

   For example, ptype([11]) makes points appear as dots, ptype([41]) makes
(diagonal) crosses, and ptype([63]) makes filled hexagons. Points made
up of lines (types 0, 1, and 2) are drawn using the current LTYPE.
}
%rename(ptype) sm_ptype;	void sm_ptype(REAL [] = &forty_one, int = 1);

TWO_VECTOR_FUNC(points);
TWO_VECTOR_FUNC(histogram);
TWO_VECTOR_FUNC(connect);
THREE_VECTOR_FUNC(points_if);
THREE_VECTOR_FUNC(histogram_if);
THREE_VECTOR_FUNC(connect_if);

%feature("docstring") sm_lweight {
   Make all lines in the plot darker (1 or less: normal)
}
%rename(lweight) sm_lweight;	void sm_lweight(double = 1);

%feature("docstring") sm_identification {
   Add an identification string to a plot
}
%rename(identification) sm_identification; void sm_identification(char *idStr);

%feature("docstring") sm_xlabel {
   Write a label below the x-axis
}
%rename(xlabel) sm_xlabel;	void sm_xlabel(char *lab);

%feature("docstring") sm_ylabel {
   Write a label to the left of the y-axis
}
%rename(ylabel) sm_ylabel;	void sm_ylabel(char *lab);

%{
   static void sm_errorbar_helper(REAL x[], int nx, REAL y[], int ny, REAL e[], int ne, int loc) {
      dimensions_match = (nx == ny && ny == ne) ? 1 : 0;
      if (dimensions_match) {
	 sm_errorbar(x, y, e, loc, nx);
      }
   }
%}
CHECK_DIMENSIONS(errorbar);
%rename(errorbar) sm_errorbar_helper;
static void sm_errorbar_helper(REAL [], int, REAL [], int, REAL [], int, int);

%feature("docstring") sm_ltype {
   Set the current linetype:
        0 = solid
        1 = dot
        2 = short dash
        3 = long dash
        4 = dot - short dash
        5 = dot - long dash
        6 = short dash - long dash
        10= erase lines   
}
%rename(ltype) sm_ltype;	void sm_ltype(int ltype = 0);

%feature("docstring") sm_relocate {
   Move the plot pointer to (x, y)
}
%rename(relocate) sm_relocate;	void sm_relocate(double x, double y);

%feature("docstring") sm_label {
   Draw a label at the current plotting point (may be set using relocate)
}
%rename(label) sm_label;	void sm_label(char *str);

%feature("docstring") sm_dot {
   Draw a point at the current plotting point (may be set using relocate)
}
%rename(dot) sm_dot;		void sm_dot(void);

%feature("docstring") sm_dot {
   Draw a line to (x,y) from the current plotting point (may be set using relocate)
}
%rename(draw) sm_draw;		void sm_draw(double x, double y);

%feature("docstring") sm_putlabel {
   Write str onto the plot near the current plotting point (may be set using relocate).
The parameter where specifies the location of the string:
               label to left     centre     right
        label above      7         8         9 
              centered   4         5         6
              below      1         2         3
}
%rename(putlabel) sm_putlabel;	void sm_putlabel(int where, char *str);

%feature("docstring") sm_grid {
   Draw a grid at major (major_minor = 0) or minor ( == 1)
If xy is 1 draw only an x-grid; if 2 draw only a y-grid; if 3 (or 0) draw both
}
%rename(grid) sm_grid;		void sm_grid(int major_minor = 0, int xy = 3);

%rename(ctype_i) sm_ctype_i;	void sm_ctype_i(int = 0);
%rename(ctype) sm_ctype;	void sm_ctype(char * = "default");
%rename(add_ctype) sm_add_ctype; int sm_add_ctype(const char *name, int red, int green, int blue);
%rename(delete_ctype) sm_delete_ctype; int sm_delete_ctype(const char *name);
%rename(redraw) sm_redraw;	void sm_redraw(int);
%rename(hardcopy) sm_hardcopy;	void sm_hardcopy(void);

%feature("docstring") sm_levels {
   Set the contour levels for contour()
}
%rename(levels) sm_levels;      void sm_levels(REAL [], int);

%{
void contour(REAL **arr, int nx, int ny, double x1, double x2, double y1, double y2)
{
   if (x1 == x2) {
      x1 = 0; x2 = nx - 1;
   }
   if (y1 == y2) {
      y1 = 0; y2 = ny - 1;
   }

   sm_defimage(arr, x1, x2, y1, y2, nx, ny);
   sm_contour();
   sm_delimage();
}
%}
%feature("autodoc", "contour(pyArray, x1=0, x2=nx-1, y1=0, y2=nx-1)") contour;
%feature("docstring") contour {
   Make a contour plot of pyArray, using contour levels set by levels()

   If x1/x2 or y1/y2 are set, they are taken to be the x and y ranges
   of the array (default: 0..N-1)
}
void contour(REAL **, int, int, double x1 = 0, double x2 = 0, double y1 = 0, double y2 = 0);
/*
 * Shade next
 */
%feature("docstring") sm_shade {
   Shade the area between two lines
}

%{
static void sm_shade_helper(float spacing, REAL x[], int nx, REAL y[], int ny, int type) {
   dimensions_match = (nx == ny) ? 1 : 0;
   if (dimensions_match) {
      sm_shade(spacing, x, y, nx, type);
   }
}
%}

CHECK_DIMENSIONS(shade);

%rename(shade) sm_shade_helper;
static void sm_shade_helper(float, REAL [], int, REAL [], int, int);

