#include "copyright.h"
/*
 * Deal with colours. Colours are defined by numbers, which are interpreted
 * as indices into the array colours. By convention, the first NCOLOR of
 * these colours may also be refered to by name, as given in colour_name[],
 * and initially (when colors is NULL) colour numbers are adjusted to
 * give these hues.
 *
 * Elements in colors[] are intepreted as being r*RED + b*BLUE + g*GREEN
 * where r,g, and b are in the range [0,255]
 */
#include <stdio.h>
#include <assert.h>
#include "options.h"
#include "devices.h"
#include "vectors.h"
#include "sm.h"
#include "sm_declare.h"

static char **color_names = NULL;	/* names of colours */
static COLOR *colors = NULL;		/* values of colours */
static int ncol = -1,			/* number of defined colours */
           ncol_names = -1;		/* and number of names */

static void realloc_colors()
{
    int i;
    
    colors = realloc(colors, (ncol + 1)*sizeof(COLOR));
    color_names = realloc(color_names, ncol*sizeof(char *));
    assert(colors != NULL && color_names != NULL);
    color_names[0] = realloc(color_names[0], ncol*(VEC_STR_SIZE + 1));
    assert(color_names[0] != NULL);

    for (i = 0; i < ncol_names; i++) {
	color_names[i] = color_names[0] + i*VEC_STR_SIZE;
    }
}

void
init_colors()
{
   static struct {
      char *name;
      COLOR color;
   } icolors[] = {
      {"default", {0,   0,   0  }},
      {"white",   {255, 255, 255}},
      {"black",   {0,   0,   0  }},
      {"red",     {255, 0,   0  }},
      {"green",   {0,   255, 0  }},
      {"blue",    {0,   0,   255}},
      {"cyan",    {0,   255, 255}},
      {"magenta", {255, 0,   255}},
      {"yellow",  {255, 255, 0  }}
   };
   int i;
   VECTOR temp;

   temp.name = "in init_colors";
/*
 * Set colour names
 */
   temp.type = VEC_STRING;
   if(vec_malloc(&temp,sizeof(icolors)/sizeof(icolors[0])) != 0) {
      msg("Can't allocate string vector in init_colors\n");
      return;
   }
   for(i = 0;i < temp.dimen;i++) {
      (void)strncpy(temp.vec.s.s[i],icolors[i].name,VEC_STR_SIZE);
   }
   set_color(&temp);
   vec_free(&temp);			/* actually this is a no-op */
/*
 * And now colour values
 */
   temp.type = VEC_FLOAT;
   if(vec_malloc(&temp,sizeof(icolors)/sizeof(icolors[0])) != 0) {
      msg("Can't allocate float vector in init_colors\n");
      return;
   }
   for(i = 0;i < temp.dimen;i++) {
      temp.vec.f[i] = (int)(icolors[i].color.red +
		   256L*(icolors[i].color.green + 256L*icolors[i].color.blue));
   }
   set_color(&temp);
   vec_free(&temp);
}

void
sm_ctype(name)
char *name;			/* name of desired color */
{
   int i;

   if((i = parse_color(name,(int *)NULL,(int *)NULL,(int *)NULL)) < 0) {
      msg_1s("Unknown colour %s\n",name);
   } else {
      sm_ctype_i(i);
   }
}

/*
 * Return the number associated with a name
 */
int
parse_color(name,r,g,b)
char *name;
int *r,*g,*b;				/* values associated with colour
					   (may be NULL) */
{
   int i;
   int my_r, my_g, my_b;		/* places for {r,g,b} to point if NULL */

   if (r == NULL) r = &my_r;
   if (g == NULL) g = &my_g;
   if (b == NULL) b = &my_b;

   if(name == NULL) return(-1);
   
   for(i = 0;i < ncol_names;i++) {
      if(strcmp(name,color_names[i]) == 0) {
	 break;
      }
   }

   if (r == &my_r && i < ncol_names) {
       return i;
   }
   
   if(i == ncol_names) {
      if(str_scanf(name,"#%2x%2x%2x",r,g,b) == 3 ||
	 str_scanf(name,"%d %d %d",r,g,b) == 3) {
	  colors[ncol].red = *r;
	  colors[ncol].green = *g;
	  colors[ncol].blue = *b;

	  return(ncol);
      }

      return(-1);
   }

   return(get_color(i,r,g,b));
}

void
sm_ctype_i(num)
int num;				/* number of desired colour */
{
   int r,g,b;				/* intensities of red, green, blue */

   if(get_color(num,&r,&g,&b) < 0) {
      msg_1d("Illegal colour %d\n",num);
   } else {
      if(SET_CTYPE((COLOR *)NULL,0) == 0) {	/* need array index */
	 CTYPE(num,0,0);
      } else {
	 CTYPE(r,g,b);
      }
      save_ctype(num);
   }
}

/*************************************************************/
/*
 * Set the default ctype for a device
 */
void
default_ctype(color)
char *color;
{
   int i;

   if((i = parse_color(color,(int *)NULL,(int *)NULL,(int *)NULL)) < 0) {
      msg_1s("Unknown default colour %s, using white\n",color);
      i = 1;
   }
   
   if(i < ncol) {
      colors[0] = colors[i];
   }
}

/*************************************************************/
/*
 * Define a new set of colours. If the device needs to know them all
 * at once (e.g. sunwindows), use SET_CTYPE to tell it; if vals is
 * NULL this is all that is needed (we have just changed devices)
 */
void
set_color(vec)
VECTOR *vec;				/* vector with values to define */
{
   if(vec == NULL) {			/* initialise a new device */
      if(colors != NULL) {		/* this calls to SET_CTYPE may */
	 SET_CTYPE(colors,ncol);	/* not do anything, that's OK */
      }
      return;
   }

   if(vec->type == VEC_LONG) {
      vec_convert_float(vec);
   }

   if(vec->type == VEC_FLOAT) {		/* a new set of colours */
      sm_set_ctypes(vec->vec.f, vec->dimen);
   } else if(vec->type == VEC_STRING) {	/* a new set of names */
      color_names = vec->vec.s.s;
      ncol_names = vec->dimen;
      vec->dimen = 0;			/* fool vec_free into not freeing vec*/
      vec->vec.s.s = NULL;
      vec->vec.s.s_s = NULL;
      return;
   } else {
      msg_1d("Unknown vector type in set_colors: %d\n",vec->type);
   }
}

/*
 * actually set a new set of colours from an arithmetic array
 */
void
sm_set_ctypes(vals, dimen)
REAL *vals;
int dimen;
{
   int i;
   long	ival;				/* used in extracting colours */

   if(colors != NULL) free((char *)colors);
   if((colors = (COLOR *)malloc((dimen + 1)*sizeof(COLOR))) == NULL) { /* one extra for the scratch colour */
      fprintf(stderr,"Can't allocate colors in set_color()\n");
      return;
   }
      
   for(i = 0;i < dimen;i++) {
      ival = (long) vals[i];
      colors[i].red = ival & 0377;
      ival >>= 8;
      colors[i].green = ival & 0377;
      ival >>= 8;
      colors[i].blue = ival & 0377;
   }
   ncol = dimen;
   
   if(colors != NULL) {			/* this call to SET_CTYPE may */
      SET_CTYPE(colors,ncol);		/* not do anything, that's OK */
   }
}

/*
 * Return the r, g, b, values associated with color number num
 */
int
get_color(num,r,g,b)
int num,				/* number of desired colour */
    *r,*g,*b;				/* values for red, green, and blue */
{
   if(num >= 0 && num <= ncol) {	/* remember; there's a special (ncol+1) element */
      *r = colors[num].red;
      *g = colors[num].green;
      *b = colors[num].blue;
   } else {
      *r = *g = *b = 0;
      return(-1);
   }
   return(num);
}

/*****************************************************************************/
/*
 * return a vector of the current CTYPEs
 */
void
vec_ctype(vec)
VECTOR *vec;
{
   int i;

   if(vec->type == VEC_LONG) {
      vec_convert_float(vec);
   }

   if(vec->type == VEC_STRING) {
      if(vec_malloc(vec,ncol_names) < 0) {
	 msg("Cannot allocate space for CTYPE(STRING)\n");
	 return;
      }

      for(i = 0;i < ncol_names;i++) {
	 (void)strncpy(vec->vec.s.s[i],color_names[i],VEC_STR_SIZE);
      }
   } else if(vec->type == VEC_FLOAT) {
      if(vec_malloc(vec,ncol) < 0) {
	 msg("Cannot allocate space for CTYPE()\n");
	 return;
      }

      for(i = 0;i < ncol;i++) {
	 vec->vec.f[i] = colors[i].red +
				    256*(colors[i].green + 256*colors[i].blue);
      }
   } else {
      msg_1d("Unknown vector type %d in vec_ctype\n",vec->type);
      return;
   }
}

/************************************************************************************************************/
/*
 * Add a new named ctype
 */
int
sm_add_ctype(name,r,g,b)
const char *name;			/* name of colour */
int r, g, b;				/* values for red, green, and blue */
{
    int num;				/* index for new colour */
    if (ncol_names != ncol) {
	msg_1d("You have %d colours but only ", ncol);
	msg_1d("%d colours names; ", ncol_names);
	msg_1s("%s\n; ", name);

	return -1;
    }

    (void)sm_delete_ctype(name);
    
    num = ncol;
    ncol++; ncol_names++;
    realloc_colors();

    (void)strncpy(color_names[num], name, VEC_STR_SIZE);

    colors[num].red = r;
    colors[num].green = g;
    colors[num].blue = b;

    if(colors != NULL) {			/* this call to SET_CTYPE may */
	SET_CTYPE(colors,ncol);		/* not do anything, that's OK */
    }

    return 0;
}

/************************************************************************************************************/
/*
 * Delete a named ctype
 */
int
sm_delete_ctype(name)
const char *name;			/* name of colour */
{
    int i;
    
    if(name == NULL) return(-1);

    if (ncol_names != ncol) {
	msg_1d("You have %d colours but only ", ncol);
	msg_1d("%d colours names; ", ncol_names);
	msg_1s("%s\n; ", name);

	return -1;
    }
    
    for(i = 0;i < ncol_names;i++) {
	if(strcmp(name,color_names[i]) == 0) {
	    for (; i < ncol_names; i++) {
		color_names[i] = color_names[i + 1];
		colors[i] = colors[i + 1];
	    }

	    ncol--; ncol_names--;
	    realloc_colors();

	    return 0;
	}
    }

    return -1;
}
