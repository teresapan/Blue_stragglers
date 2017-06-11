#include "copyright.h"
/*
 *			     STRING.C				
 *	this module handles everything with the character fonts	
 *--------------------------------------------------------------
 *	 load_font()
 *	 old_string(s,drawit)		Old-style labels (static)
 *	 tex_string			TeX-style labels (static)
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(__MSDOS__) || defined(__OS2__)
#  include <fcntl.h>
#endif
#include "defs.h"
#include "sm.h"
#include "devices.h"
#include "fonts.h"
#include "tree.h"
#include "sm_declare.h"

#if defined(__MSDOS__) || defined(__OS2__)
#  define OPEN_MODE O_BINARY
#else
#  define OPEN_MODE 0
#endif

#define RETURN(I)			/* cleanup and return */ \
  if(fbin >= 0) close(fbin); \
  if(font != NULL) free((char *)font); \
  if(fp != NULL) free(fp); \
  if(ns != NULL) free(ns); \
  return(I);

#define NVAL 6				/* number of vals to save */
#define RESTORE_VALS(ARR)		/* restore some values */\
  x_0 = ARR[0]; y_0 = ARR[1];\
  swidth = ARR[2]; sdepth = ARR[3]; sheight = ARR[4]; baseline = ARR[5];
#define SAVE_VALS(ARR)			/* save some values */\
  ARR[0] = x_0; ARR[1] = y_0;\
  ARR[2] = swidth; ARR[3] = sdepth; ARR[4] = sheight; ARR[5] = baseline;

#define SET_EXPAND			/* set the various expand coeffs */\
  expd = eexpand*sfac;			/* full expansion */\
  ecos = sm_cos*expd;			/* expanded cosine */\
  esin = sm_sin*expd;			/* expanded sine */\
  rsh = 16*shift/sfac;			/* relative shift */

#define SET_LDH(X,D,H)			/* set s{width,depth,height} */\
  swidth += sfac*(X);\
  if(((D) - CDEF*rsh)*sfac - baseline > sdepth) \
				sdepth = ((D) - CDEF*rsh)*sfac - baseline;\
  if(((H) + CDEF*rsh)*sfac + baseline > sheight) \
				sheight = ((H) + CDEF*rsh)*sfac + baseline;


static char *tex_font_alias(),
	    *get_num(),
	    *ns = NULL,			/* number of strokes in character */
	    *read_group(),
	    *tex_macro(),
	    *tex_string();
static Signed char *font = NULL,	/* ptr to stroke table */
		   *id,			/* depth of character */
		   *ih,			/* height of character */
		   *il,			/* left adjust */
		   *ir;			/* right adjust */
static float xasp,yasp;			/* x and y aspect ratios */
int s_depth = 0, s_height = 0,		/* pixel depth, height, and width */
    s_width = 0;			/* 		of str as $variables */
static int sdepth,sheight,swidth;	/* pixel depth, height, width of str */
static int baseline;			/* height of string baseline */
static int *fp = NULL,			/* pointer to chars */
	   group = 0,			/* level of TeX grouping */
  	   nfont = 0,			/* number of fonts */
	   read_fonts = 0,		/* have we read them yet? */
           tex_font(),
	   tex_special();
long x_0 = 0, y_0 = 0;			/* origin of character */
static void gdimen(),			/* dimensions of a string as drawn */
	    i_string(),
	    old_string(),
	    set_tex_def(),
	    tex_dot P_ARGS(( int, int, double, double )),
	    tex_rule();

int
load_font(ffile)			/* read font arrays */
char *ffile;				/* file to read, or NULL */
{
   char *font_file,			/* name of font file */
	tex_name[TEX_NAME_SIZE],	/* name of a TeX definition */
	tex_value[TEX_VAL_SIZE];	/* value of a TeX definition */
   int fbin = -1,
       i,
       narg,				/* number of arguments */
       nchar,		/* Number of characters in fonts, NFONT*NCHAR */
       ndata,		/* number of numbers describing characters in fonts */
       ndefs,		/* number of TeX font definitions */
       texfont,				/* font for a TeX definition */
       val_size;			/* length of a TeX definition */

   if(ffile != NULL) {
      font_file = ffile;
   } else {
      if((font_file = get_val("fonts")) == NULL) {
	 msg("Fonts file not found\n");
	 RETURN(-1);
      }
   }
   if((fbin = open(font_file,OPEN_MODE)) < 0) {
      msg_1s("Can't find font binary file \"%s\"\n",font_file);
      RETURN(-1);
   }

   if(read(fbin,(char *)&i,sizeof(int)) != sizeof(int)) {
      msg("Can't read font file type from font file\n");
      RETURN(-1);
   }
   if(i != FONT_FILE_TYPE) {
      msg_1d("Font file format level is %d,",i);
      msg_1d(" not %d; You must re-run read_fonts\n",FONT_FILE_TYPE);
      RETURN(-1);
   }
   if(read(fbin,(char *)&nchar,sizeof(int)) != sizeof(int)) {
      msg("Can't read nchar from font file\n");
      RETURN(-1);
   }
   nfont = nchar/NCHAR;
   if(nchar%NCHAR != 0) {
      msg_1d("Last %d chars don't fit into a font\n",nchar%NCHAR);
      nchar = nfont*NCHAR;
   }

   if(nfont > NFONT) {
      msg_1d("Too many fonts (%d, > ",nfont);
      msg_1d("%d) specified in load_font\n",NFONT);
      nfont = NFONT;
   }
   if(read(fbin,(char *)&ndata,sizeof(int)) != sizeof(int)) {
      msg("Can't read ndata from font file\n");
      RETURN(-1);
   }
   if((font = (Signed char *)malloc(ndata + 4*nchar)) == NULL) {
      msg("Couldn't allocate space for font table\n");
      RETURN(-1);
   }
   id = font + ndata;
   ih = id + nchar;
   il = ih + nchar;
   ir = il + nchar;

   if((ns = malloc(nchar)) == NULL) {
      fprintf(stderr,"Can't allocate storage for ns\n");
      RETURN(-1);
   }
   if(read(fbin,ns,nchar) != nchar) {
      msg("Can't read ns from font file\n");
      RETURN(-1);
   }
   
   if(read(fbin,(char *)il,nchar) != nchar) {
      msg("Can't read il from font file\n");
      RETURN(-1);
   }
   if(read(fbin,(char *)ir,nchar) != nchar) {
      msg("Can't read ir from font file\n");
      RETURN(-1);
   }
   if(read(fbin,(char *)id,nchar) != nchar) {
      msg("Can't read id from font file\n");
      RETURN(-1);
   }
   if(read(fbin,(char *)ih,nchar) != nchar) {
      msg("Can't read ih from font file\n");
      RETURN(-1);
   }

   if((fp = (int *)malloc(nchar*sizeof(int))) == NULL) {
      msg("Can't allocate storage for fp\n");
      RETURN(-1);
   }
   if(read(fbin,(char *)fp,nchar*sizeof(int)) != nchar*sizeof(int)) {
      msg("Can't read fp from font file\n");
      RETURN(-1);
   }
   if(read(fbin,(char *)font,ndata) != ndata) {
      msg("Can't read font from font file\n");
      RETURN(-1);
   }
/*
 * Now read TeX definitions
 */
   if(read(fbin,(char *)&ndefs,sizeof(int)) != sizeof(int)) {
      msg("Can't read number of TeX definitions\n");
      close(fbin);
      return(-1);
   }
   for(i = 1;i <= ndefs;i++) {
      if(read(fbin,tex_name,TEX_NAME_SIZE) != TEX_NAME_SIZE) {
	 msg_1d("Can't read name of %d'th TeX definition\n",i);
	 break;
      }
      if(read(fbin,(char *)&val_size,sizeof(int)) != sizeof(int)) {
	 msg_1s("Can't read length of TeX definition for \"%s\"\n",tex_name);
	 break;
      }
      if(read(fbin,tex_value,val_size) != val_size) {
	 msg_1s("Can't read value for TeX definition \"%s\"\n",tex_name);
	 break;
      }
      tex_value[val_size] = '\0';
      if(read(fbin,(char *)&texfont,sizeof(int)) != sizeof(int)) {
	 msg_1s("Can't read font for TeX definition \"%s\"\n",tex_name);
	 break;
      }
      if(read(fbin,(char *)&narg,sizeof(int)) != sizeof(int)) {
	 msg_1s("Can't read number of arguments for TeX definition \"%s\"\n",
		tex_name);
	 break;
      }
      set_tex_def(tex_name,tex_value,texfont,narg);
   }
   (void)close(fbin);
/*
 * A couple of special cases:
 */
   set_tex_def(","," ",SCRIPT,0);
   set_tex_def("!"," ",GREEK,0);

   read_fonts = 1;
   return(0);
}

/*****************************************************************************/
/*
 * Return the width, depth, and height of a string
 */
void
string_size(str,width,depth,height)
char *str;
float *width;
float *depth,*height;
{
   int save_vals[NVAL];

   SAVE_VALS(save_vals);
   i_string(str,0);
   if(width != NULL) *width = swidth;
   if(depth != NULL) *depth = sdepth;
   if(height != NULL) *height = sheight;
   s_depth = sdepth;			/* for DEFINE var | */
   s_height = sheight;
   s_width = swidth;
   RESTORE_VALS(save_vals);
}

/*
 * Actually draw a string on the device
 */
void
draw_string(str)
char *str;
{
   i_string(str,1);
   if(group > 0) {
      msg_1s("String \"%s\" ends within a group\n",str);
   }
}

/*
 * Write a string to the plotter (if drawit is true, otherwise simply
 * find its length)
 */
static void
i_string(s,drawit)
char *s;				/* string to write */
int drawit;				/* should I really draw it? */
{
   char *TeX_strings = print_var("TeX_strings");
   double condense = 1.0;		/* factor to compress font by */
   double slant = SLANT;		/* slant of \it characters */
   float height_scale,			/* transverse expansion */
	 width_scale;			/* expansion along drawing dir. */
   int default_font,			/* default font (TeX only) */
       TeX;				/* Use TeX strings? */
   int xp0 = xp,yp0 = yp;		/* initial plot pointer */
/*
 * For the simple case, generate the characters in hardware.
 */
   TeX = (*TeX_strings != '\0' && *TeX_strings != 0); /* do we want TeX? */

   if(eexpand == 1. && aangle == 0. && CHAR((char *)NULL,0,0) == 0) {
      if((TeX && !tex_special(s)) || (!TeX && strchr(s,'\\') == NULL)) {
	 swidth = (int)strlen(s)*cwidth;
	 sheight = cheight;
	 sdepth =  0;
	 if(drawit) {
	    x_0 = (xp > SCREEN_SIZE) ? SCREEN_SIZE : xp;
	    x_0 = (x_0 < 0) ? 0 : x_0;
	    y_0 = (yp > SCREEN_SIZE) ? SCREEN_SIZE : yp;
	    y_0 = (y_0 < 0) ? 0 : y_0;
	    (void)CHAR(s,(int)x_0,(int)y_0);
	    xp = xp0 + swidth;		/* reset current position */
	 }
	 return;
      }
   }
   if(!read_fonts) {
      if(load_font((char *)NULL) < 0) {
	 msg("I can't read the fonts file so I can't write labels\n");
	 msg("Maybe you haven't opened a device from non-interactive SM?\n");
	 exit(EXIT_BAD);
      }
   }

   group = 0;
   swidth = 0.0;
   sheight = sdepth = 0.0;
   baseline = 0;
   x_0 = xp;
   y_0 = yp;

   if(aspect > 1.0) {
      xasp = aspect;
      yasp = 1.0;
   } else if (aspect < 1.0) {
      xasp = 1.0;
      yasp = aspect;
   } else {
      xasp = 1.0;
      yasp = 1.0;
   }

   if(TeX) {
      char *def_font;
      float mag;			/* magnification of font */
      
      def_font = print_var("default_font");
      if(*def_font == '\0') {
	 def_font = "rm";
      }
      
      mag = 1.0;
      if((default_font = tex_font(0,def_font,1,&mag,&slant,&condense,0)) < 0) {
	 msg_1s("Unknown default font %s, using \\rm\n",def_font);
	 if((default_font = tex_font(0,"rm",1,&mag,&slant,&condense,0)) < 0) {
	    msg("Can't find \\rm font -- good luck\n");
	    default_font = 0;
	 }
      }
      (void)tex_string(s,drawit,default_font,mag,0.0,slant,condense);
   } else {
      old_string(s,drawit);
   }
/*
 * The plot pointer should be moved to the baseline at the right of the
 * last character, so succesive label commands can be used to write a
 * connected piece of prose
 */
   if(drawit) {
      sc_relocate((int)(xp0 + swidth*eexpand*sm_cos*xasp),
		  (int)(yp0 + swidth*eexpand*sm_sin/yasp));
   }
/*
 * Convert dimensions to as-drawn lengths
 */
   height_scale = hypot(sm_sin*xasp,sm_cos/yasp);
   width_scale = hypot(sm_cos*xasp,sm_sin/yasp);
   
   swidth *= eexpand*width_scale;
   sdepth *= eexpand*height_scale;
   sheight *= eexpand*height_scale;

}

/*****************************************************************************/
/*
 * Old-style labels
 */
static void
old_string(s,drawit)			/* draw char string */
char *s;
int drawit;
{
   Signed char *t;
   float fac,				/* magnification factor */
   	 sf_old,sh_old,
   	 x,y,expd,rsh,ecos,esin,
	 sfac,				/* reduction of chars */
   	 shift;				/* shift of sub/super */
   int it_old,io_old,is_old,
       ila,cc,xa,i,
       iesc,			       /* escape sequence flag */
       relo,			       /* relocate flag */
       ital,			       /* italic flag */
       ioff,			       /* offset to font type */
       isup;			       /* sub or super level */
   long	ix,iy;

   sfac = 1.0;
   shift = 0.0;
   relo = iesc = ital = ioff = isup = 0;
   it_old = ital;
   io_old = ioff;
   is_old = isup;
   sf_old = sfac;
   sh_old = shift;
   if(aspect > 1.0) {
      xasp = aspect;
      yasp = 1.0;
   } else if (aspect < 1.0) {
      xasp = 1.0;
      yasp = aspect;
   } else {
      xasp = 1.0;
      yasp = 1.0;
   }

   while((cc = *s++) != '\0') {
      if(cc == '\\' && iesc++ < 2)  {	/* iesc == 2 ==> \\\ ==> a real \ */
	 ;				/* escape code */
      } else if(iesc == 1 || iesc == 2) { /* real escape sequence */
	 switch(cc) {
	  case '-':			/* change size of fonts */
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
	    if(cc == '-') {
	       cc = *s++;
	       if(!isdigit(cc)) {
		  if(drawit)
		     msg("You must specify a magnification, not just \\-\n");
		  break;
	       }
	       fac = 1/MAGSTEP;
	    } else {
	       fac = MAGSTEP;
	    }
	    for(cc -= '0';cc > 0;cc--) sfac *= fac;
	    break;  
	  case 'd': case 'D':		/* down - subscript */
	    sfac *= ((--isup)>=0 ? (1/SUB_SUPER_EXPAND) : SUB_SUPER_EXPAND);
	    shift -= sfac;
	    break;
	  case 'e': case 'E':		/* end, forced exit */
	    if(drawit) {		/* update current pos */
	       sc_relocate(x_0,y_0);
	    }
	    return;
	  case 'g': case 'G':		/* greek */
	    if(drawit && nfont < GREEK + 1) {
	       msg("No Greek (\\g) font is defined\n");
	    } else {
	       ioff = GREEK*NCHAR;
	    }
	    break;
	  case 'i': case 'I':		/* italic */
	    ital=~ital;
	    break;
	  case 'p': case 'P':		/* private */
	    if(drawit && nfont < PRIVATE + 1) {
	       msg("No Private (\\p) font is defined\n");
	    } else {
	       ioff = PRIVATE*NCHAR;
	    }
	    break;
	  case 'o': case 'O':		/* old english */
	    if(drawit && nfont < OLD_ENGLISH + 1) {
	       msg("No Old English (\\o) font is defined\n");
	    } else {
	       ioff = OLD_ENGLISH*NCHAR;
	    }
	    break;
	  case 'r': case 'R':		/* roman */
	    if(drawit && nfont < ROMAN + 1) {
	       msg("No Roman (\\r) font is defined\n");
	    } else {
	       ioff = ROMAN*NCHAR;
	    }
	    break;
	  case 's': case 'S':		/* script */
	    if(drawit && nfont < SCRIPT + 1) {
	       msg("No Script (\\s) font is defined\n");
	    } else {
	       ioff = SCRIPT*NCHAR;
	    }
	    break;
	  case 't': case 'T':		/* `tiny' (nowadays just simple) */
	    if(drawit && nfont < TINY + 1) {
	       msg("No `Tiny' (\\t) font is defined\n");
	    } else {
	       ioff = TINY*NCHAR;
	    }
	    break;
	  case 'u': case 'U':		/* up - superscript */
	    shift +=  sfac;
	    sfac*=((++isup)>0 ? SUB_SUPER_EXPAND : (1/SUB_SUPER_EXPAND));
	    break;
	  default:
	    if(drawit) msg_1s("Illegal escape: \\%s\n",(s-1));
	    break;
	 }

	 if(iesc > 1) {		       /* permanent change */
	    it_old = ital;
	    io_old = ioff;
	    is_old = isup;
	    sf_old = sfac;
	    sh_old = shift;
	 }
	 iesc = 0;			/* clear flag */
      } else {				/* normal character */
	 SET_EXPAND;
	 if(iscntrl(cc)) {		/* control chars are not in fonts */
	    cc = 0;
	 } else {
	    cc -= ' ';
	 }
	 cc += ioff;		     	/* select font */

	 if(drawit) {
	    relo = 1;			/* start new character */
	    ila	 = il[cc];		/* left adjust */
	    t	 = font + fp[cc] - 1;
	    for(i=0;i < ns[cc]; i++ ){
	       ix = *t++;	 /* x */
	       iy = *t++;	 /* y */
	       if(ix == FONT_MOVE) {
		  relo = 1;		/* is it a new start? */
	       } else {
		  x  = ix - ila + (ital ? SLANT*(iy-23) : 0);
		  y  = iy + rsh;
		  ix = xasp*CDEF*(ecos*x - esin*y) + x_0;
		  iy = CDEF*(esin*x + ecos*y)/yasp + y_0;

		  if (relo) {
		     relo = 0;		/* relocate, reset flag */
		     sc_relocate(ix,iy);
		  } else {
		     draw_dline(xp,yp,ix,iy);
		  }
		  sc_relocate(ix,iy);	/* store last pos */
	       }
	    }
	 }
	 xa = ir[cc] - il[cc];		/* width of char */
	 SET_LDH(CDEF*xa,CDEF*id[cc],CDEF*ih[cc]);
	 x_0 += CDEF*ecos*xa*xasp;
	 y_0 += CDEF*esin*xa/yasp;		/* set new char origin */

	 ital = it_old;			/* restore attributes */
	 ioff = io_old;
	 isup = is_old;
	 sfac = sf_old;
	 shift = sh_old;
	 iesc = 0;			/* clear flag (it may be 2) */
      }
   }
   if(drawit) {				/* update current pos */
      sc_relocate(x_0,y_0);
   }
   return;
}

/*********************************************************/
/*
 * And now the code to deal with TeX-style strings
 */
#define BOLD 020			/* set `bold' bit */
#define D_BOLD 1.5			/* offset for drawing bold chars */
#define ITALIC 040			/* set `italic' bit */
#define SIZE 40				/* size of "TeX" macro names */

static char *
tex_string(str,drawit,font_id,sfac,shift,slant,condense)
char *str;				/* string to process */
int drawit,				/* do I actually want to draw this? */
    font_id;				/* font to use */
double sfac,				/* current expansion */
      shift,				/* level of sub/superscript */
      slant,				/* slant of \it fonts */
      condense;				/* amount to condense fonts */
{
   char name[SIZE];			/* name of "TeX" definition */
   Signed char *t;
   float expd,rsh,ecos,esin,
	 sfac0 = sfac,			/* base level of sfac */
	 shift0 = shift,		/* base value of shift */
   	 x,y;
   float mag;				/* magnification of font */
   int c,
       i,
       ila,ix,iy,xa,
       initial_ctype = current_ctype(),	/* ctype upon entry to function */
       relo,				/* relocate, don't draw line */
       save_pos = 0,			/* are x_0, y_0 saved? */
       save_vals[NVAL];			/* saved values of
					   x_0, y_0, s{width,depth,height} */

   for(;;) {
      save_pos--;
      switch (*str) {
       case '\0':
	 sm_ctype_i(initial_ctype);	/* restore ctype */
	 return("");
       case '^':			/* raise next group */
	 if(save_pos > 0) {
	    RESTORE_VALS(save_vals);
	 }
	 sfac *= SUB_SUPER_EXPAND;
	 shift += sfac;
	 str++;
	 SAVE_VALS(save_vals); save_pos = 3;
	 continue;
       case '_':			/* lower next group */
	 if(save_pos > 0) {
	    RESTORE_VALS(save_vals);
	 }
	 sfac *= SUB_SUPER_EXPAND;
	 shift -= sfac;
	 str++;
	 SAVE_VALS(save_vals); save_pos = 3;
	 continue;
       case '{':			/* start group */
	 group++;
/*
 * now process the group
 */
	 str = tex_string(++str,drawit,font_id,sfac,shift,slant,condense);
	 sfac = sfac0;			/* reset after group */
	 shift = shift0;
	 continue;
       case '}':			/* end group */
	 if(--group < 0) {
	    if(drawit) msg_1s("Too many closing braces: \"%s\"\n",str);
	    group = 0;
	 } else {
	    sm_ctype_i(initial_ctype);	/* restore ctype */
	    return(++str);
	 }
	 break;
       case '\\':			/* "TeX" macro, escape, or font */
	 str++;
	 if(*str == '^' || *str == '_' || *str == '\\' || *str == ' ' ||
	    				  *str == '{' || *str == '}') {
	    break;
	 } else if(isdigit(*str) || (*str == '-' && isdigit(*(str+1)))) {
	    mag = 1.0;
	    i = 0;
	    if(*str == '-') name[i++] = *str++;
	    if(isdigit(*str)) name[i++] = *str++;
	    name[i] = '\0';
	    if(tex_font(font_id,name,drawit,&mag,&slant,&condense,0) >= 0) {
	       sfac0 *= mag; sfac *= mag;
	    }
	    continue;
	 }

	 if(*str == '\0') {
	    i = 0;
	 } else {
	    for(i = 0;i < SIZE;i++) {	/* find name of "TeX" macro */
	       name[i] = *str++;
	       if(!isalpha(name[i])) break;
	    }
	    if(i == 0) {
	       i++;			/* single non-alpha character */
	    } else if(!isspace(name[i])) {
	       str--;			/* went one too far */
	    }
	 }
	 name[i] = '\0';

	 mag = 1.0;
	 if((i = tex_font(font_id,name,drawit,&mag,&slant,&condense,0)) >= 0) {
	    font_id = i;
	    sfac *= mag;
	    sfac0 *= mag;
	    continue;
	 }
	 if(!strcmp(name,"point") || !strcmp(name,"apoint")) {
	    float ang;
	    int n,style;

	    if(name[0] == 'a') {	/* specify angle ptype */
	       while(isspace(*str)) str++;
	       ang = atof2(str);
	       while(isdigit(*str)) str++;
	       if(*str == '.') {
		  str++;
		  while(isdigit(*str)) str++;
	       }
	       while(isspace(*str)) str++;
	    } else {
	       ang = aangle;
	    }

	    n = atoi(str);
	    if(isspace(*(str + 1))) {	/* a single digit, so it's n */
	       str++;
	       while(isspace(*str)) str++;
	       style = atoi(str);
	    } else {
	       style = n%10;
	       n /= 10;			/* remove the style */
	    }
	    if(drawit && n == 0) {
	       msg_1s("You must specify a ptype with \\point: \"%s\"\n",str);
	       continue;
	    }
	    while(isdigit(*str)) {
	       str++;
	    }
	    SET_EXPAND;

	    if(drawit) {
	       x = 1.1*PDEF;
	       y = x + rsh;
	       sc_relocate((long)((ecos*x - esin*y)*xasp) + x_0, 
			   (long)((esin*x + ecos*y)/yasp) + y_0);
	       tex_dot(n,style,expd,aangle + ang);
	    }

	    xa = 2*(1.1*PDEF);		/* width of dot */
	    SET_LDH(xa,0,2.2*PDEF/sfac);
	    x_0 += ecos*xa*xasp;
	    y_0 += esin*xa/yasp;
	 } else if(!strcmp(name,"ctype") || !strcmp(name,"colour") || !strcmp(name,"color")) { /* Change colours */
	    char value[TEX_VAL_SIZE];
	    str = read_group(value,TEX_VAL_SIZE,str,drawit);
	    sm_ctype(value);
	 } else if(!strcmp(name,"def")) { /* define a new TeX definition */
	    char value[TEX_VAL_SIZE];	/* value of a new "TeX" definition */
	    int narg = 0;		/* number of arguments */

	    if(drawit) {		/* only need define it once... */
	       while(str != '\0' && *str != '{') str++;
	       str = read_group(value,TEX_VAL_SIZE,str,drawit);
	       continue;
	    }
	    if(*str != '\\') {
	       msg_1s("A name must follow \\def, not \"%s\"\n",str);
	       continue;
	    }
	    str++;			/* skip \ */
	    for(i = 0;i < SIZE;i++) {	/* find name of new "TeX" macro */
	       name[i] = *str++;
	       if(isspace(name[i]) || name[i] == '\0' || name[i] == '{' ||
		  name[i] == '#') break;
	    }
	    name[i] = '\0';
	    
	    while(*(str-1) == '#') {	/* arguments */
	       if(*str == '-') {        /* some special type of definition */
		  if(narg != 0) {
		     msg_1s(
			 "Only one -ve argument is allowed for TeX macro %s\n",
									 name);
		     continue;
		  }
		  str++;
		  if(!isdigit(*str)) {
		     msg_1s("Please provide a digit with \\%s#1\n",name);
		     continue;
		  }
		  narg = -1*(*str++ - '0');
	       } else if(*str++ - '0' != ++narg) {
		  msg_1s("Inconsistent argument numbering for TeX macro %s\n",
			 name);
		  continue;
	       }
	       str++;
	    }
	    str--;			/* unread the '{' */
	    if(*str != '{') {
	       msg_2s("A value in {} must follow \\def\\%s, not \"%s\"\n",
		      name,str);
	       continue;
	    }
	    str = read_group(value,TEX_VAL_SIZE,str,drawit);
	    set_tex_def(name,value,font_id,narg);
	 } else if(strcmp(name,"hrule") == 0) {
	    int len = -1;
	    
	    str = get_num(str,&len,font_id,shift,slant,condense,drawit);

	    if(drawit && len < 0) {
	       msg_1s(
	       "You must specify a length with \\hrule: \"%s\"\n",str);
	       continue;
	    }

	    SET_EXPAND;

	    x = 0;
	    y = rsh;
	    if(drawit) {
	       sc_relocate((long)((ecos*x - esin*y)*xasp) + x_0,
			   (long)((esin*x + ecos*y)/yasp) + y_0);
	       tex_rule(0,len*expd,'h');
	    }

	    SET_LDH(len,0,y);
	    x_0 += xasp*ecos*len;
	    y_0 += esin*len/yasp;
	 } else if(!strcmp(name,"kern")) {
	    int dx;
	    
	    str = get_num(str,&dx,font_id,shift,slant,condense,drawit);
	    SET_EXPAND;
	    x_0 += dx*xasp*ecos;
	    y_0 += dx*esin/yasp;
	    if(drawit) {
	       sc_relocate(x_0,y_0);
	    }
	    SET_LDH(dx,0,0);
	 } else if(!strcmp(name,"move")) {
	    int dx,dy;
	    char group_str[TEX_VAL_SIZE];
	    int saved[NVAL];

	    SAVE_VALS(saved);
	    str = get_num(str,&dx,font_id,shift,slant,condense,drawit);
	    str = get_num(str,&dy,font_id,shift,slant,condense,drawit);
	    SET_EXPAND;
	    x_0 += xasp*(ecos*dx - esin*dy);
	    y_0 += (esin*dx + ecos*dy)/yasp;

	    str = read_group(group_str,TEX_VAL_SIZE,str,drawit);
	    tex_string(group_str,drawit,font_id,sfac,shift,slant,condense);
	    
	    RESTORE_VALS(saved);
	 } else if(!strcmp(name,"raise")) {
	    int dy;
	    
	    str = get_num(str,&dy,font_id,shift,slant,condense,drawit);
	    SET_EXPAND;
	    x_0 -= xasp*esin*dy;
	    y_0 += ecos/yasp*dy;
	    if(drawit) {
	       sc_relocate(x_0,y_0);
	    }
	    baseline += dy;
	    SET_LDH(0,0,0);
	 } else if(!strcmp(name,"line")) {
	    int len = -1,lt = -1;
	    
	    while(isspace(*str)) str++;
	    if(isdigit(*str)) lt = *str++ - '0';
	    while(isspace(*str)) str++;
	    str = get_num(str,&len,font_id,shift,slant,condense,drawit);

	    if(drawit && (len < 0 || lt < 0)) {
	       msg_1s(
	       "You must specify a length and type with \\line: \"%s\"\n",str);
	       continue;
	    }

	    SET_EXPAND;

	    x = 0; y = 150 + rsh;
	    if(drawit) {
	       sc_relocate((long)((ecos*x - esin*y)*xasp) + x_0,
			   (long)((esin*x + ecos*y)/yasp) + y_0);
	       tex_rule(lt,len*expd,'h');
	    }

	    SET_LDH(len,0,y);
	    x_0 += xasp*ecos*len;
	    y_0 += esin*len/yasp;
	 } else if(strcmp(name,"advance") == 0 || strcmp(name,"depth") == 0 ||
		   strcmp(name,"divide") == 0 || strcmp(name,"height") == 0 ||
		   strcmp(name,"multiply") == 0 || strcmp(name,"width") == 0) {
	    if(!drawit) {
	       msg_1s("You can only use \\%s when SM is expecting a ",name);
	       msg_2s("number, not at\n\"\\%s%s\"\n",name,str);
	    }
	 } else if(!strcmp(name,"condense")) {
	    condense = atof(str);
	    if(*str == '+' || *str == '-') str++; /* skip number */
	    while(isdigit(*str)) {
	       str++;
	    }
	    if(*str == '.') str++;
	    while(isdigit(*str)) {
	       str++;
	    }
	 } else if(!strcmp(name,"slant")) {
	    slant = atof(str);
	    if(slant == 0) {
	       slant = SLANT;
	    }
	    if(*str == '+' || *str == '-') str++; /* skip number */
	    while(isdigit(*str)) {
	       str++;
	    }
	    if(*str == '.') str++;
	    while(isdigit(*str)) {
	       str++;
	    }
	 } else if(!strcmp(name,"vrule")) {
	    int dp,ht;
	    
	    str = get_num(str,&dp,font_id,shift,slant,condense,drawit);
	    str = get_num(str,&ht,font_id,shift,slant,condense,drawit);
	    SET_EXPAND;

	    if(drawit) {
	       sc_relocate((long)(xasp*esin*dp) + x_0,
			   (long)(-ecos*dp/yasp) + y_0);
	       tex_rule(0,(dp + ht)*expd,'v');
	    }

	    SET_LDH(0,dp,ht);
	 } else {
	    str = tex_macro(name,drawit,font_id,sfac,shift,slant,condense,str);
	 }
	 sfac = sfac0;			/* reset up/down */
	 shift = shift0;
	 continue;
       default:
	 break;
      }

      SET_EXPAND;

      c = *str++;
      if(iscntrl(c)) {			/* control chars are not in fonts */
	 c = 0;
      } else {
	 c -= ' ';
      }
      c += (font_id & ~(BOLD | ITALIC))*NCHAR;	/* select font */
      
      if(drawit) {
	 int j;
	 for(j = 0;j <= ((font_id & BOLD) ? 1 : 0);j++) {
	    if(j > 0) {
	       x_0 += CDEF*ecos*xasp*D_BOLD;
	       y_0 += CDEF*esin/yasp*D_BOLD;
	    }
	    
	    relo = 1;			/* start new character */
	    ila = il[c];
	    t = &font[fp[c] - 1];
	    for(i=0;i < ns[c];i++) {
	       ix = *t++;
	       iy = *t++;
	       if(ix == FONT_MOVE) {
		  relo = 1;		/* is it a new start? */
	       } else {
		  x = condense*(ix - ila +
				((font_id & ITALIC) ? slant*(iy-23) : 0));
		  y = iy + rsh;
		  ix = CDEF*xasp*(ecos*x - esin*y) + x_0;
		  iy = CDEF*(esin*x + ecos*y)/yasp + y_0;
		  
		  if(relo) {
		     relo = 0;		/* relocate, reset flag */
		     sc_relocate(ix,iy);
		  } else {
		     draw_dline(xp,yp,ix,iy);
		  }
	       }
	    }
	 }
      }
      xa = condense*(ir[c] - il[c]);		/* width of char */
      if(font_id & BOLD) {
	 xa += D_BOLD;
      }
      SET_LDH(CDEF*xa,CDEF*id[c],CDEF*ih[c]);
      x_0 += CDEF*ecos*xa*xasp;
      y_0 += CDEF*esin*xa/yasp;		/* set new char origin */

      sfac = sfac0;			/* reset up/down */
      shift = shift0;
   }
   /*NOTREACHED*/
}

/*
 * Return the id of a font, or -1 if not found. Mag is the magnification
 * to be applied to the font (1: default size), and is specified by a
 * font name such as \rm\3\slant0.7\condense0.5
 */
static int
tex_font(font_id,name,drawit,mag,slant,condense,recurse_level)
int font_id;				/* the old font ID */
char *name;				/* maybe a font name */
int drawit;
float *mag;				/* how much to scale font by */
double *slant,*condense;		/* slant and compression of font */
int recurse_level;			/* how many levels of recursion? */
{
   char font_name[SIZE];		/* name of font */
   int found_font = 0;			/* we found a font */
   int i;

   if(recurse_level++ >= 10) {
      msg_1d("Only %d levels of recursion allowed in tex_font\n",recurse_level);
      return(0);
   }
   if(name == NULL) {
      return(-1);
   }
   if(*name == '\\') {
      name++;
   }
/*
 * Is it just a size change?
 */
   if(isdigit(*name) || (*name == '-' && isdigit(*(name + 1)))) {
      float fac;
      
      if(*name == '-') {
	 name++;
	 fac = 1/MAGSTEP;
      } else {
	 fac = MAGSTEP;
      }
      for(i = *name++ - '0';i > 0;i--) {
	 *mag *= fac;
      }
      found_font = 1;
   } else {
      for(i = 0;i < SIZE && isalpha(*name);i++) {
	 font_name[i] = *name++;
      }
      font_name[i] = '\0';

      if(i == 0) {			/* font names are alpha characters */
	 return(-1);			/* so this can't be a font */
      }
      
      for(i = 0;i < NFONT;i++) {
	 if(strcmp(font_name,fonts[i].ab1) == 0 ||
	    strcmp(font_name,fonts[i].ab2) == 0) {
	    if(i >= nfont) {		/* warn about missing fonts */
	       if(drawit) {
		  msg_1s("No \\%s font is currently defined\n",font_name);
	       }
	    } else {
	       font_id = i | (font_id & (BOLD | ITALIC));
	       found_font = 1;
	    }
	    break;
	 }
      }
/*
 * Might be a special font
 */
      if(!found_font) {
	 if(strcmp(font_name,"b") == 0 || strcmp(font_name,"bf") == 0) {
	    font_id |= BOLD;
	    found_font = 1;
	 } else if(!strcmp(font_name,"i") || !strcmp(font_name,"it")) {
	    font_id |= ITALIC;
	    found_font = 1;
	 }
      }
      if(!found_font) {			/* we haven't found it yet */
/*
 * how about \slant or \condense? There has to be an argument, otherwise
 * we'll process it as a regular control sequence.
 */
	 if(*name != '\0' &&
	    (strcmp(font_name,"condense") == 0 ||
	     strcmp(font_name,"slant") == 0)) {
	    found_font = 1;
	    if(strcmp(font_name,"slant") == 0) {
	       if((*slant = atof(name)) == 0) {
		  *slant = SLANT;
	       }
	    } else {
	       *condense = atof(name);
	    }

	    if(*name == '+' || *name == '-') name++; /* skip number */
	    while(isdigit(*name)) {
	       name++;
	    }
	    if(*name == '.') name++;
	    while(isdigit(*name)) {
	       name++;
	    }
	 }
      }
      if(!found_font) {			/* we haven't found it yet */
	 if((name = tex_font_alias(font_name)) != NULL) {
	    if((font_id = tex_font(font_id,name,drawit,mag,slant,condense,
							  recurse_level)) <0) {
	       if(drawit) {
		  msg_1s("%s is not a valid font name\n",name);
	       }
	       return(-1);
	    } else {
	       found_font = 1;
	    }
	 }
      }
   }
   if(!found_font) {			/* didn't find a font */
      return(-1);
   }

   if(*name == '\0') {			/* nothing left over */
      ;
   } else if(*name == '\\') {
      name++;
      if((i = tex_font(font_id,name,drawit,mag,slant,condense,recurse_level))
	 								 < 0) {
	 if(drawit) {
	    msg_1s("Illegal control sequence in a font specifier: %s\n",name);
	 }
      } else {
	 font_id = i;			/* probably a font modifier */
      }
   } else {
      if(drawit) msg_1s("Junk at end of font specifier: %s\n",name);
   }

   return(font_id);
}


/*
 * Does string contain special "TeX" mode characters?
 */
static int
tex_special(s)
char *s;
{
   for(;;) {
      switch (*s++) {
       case '\0':
	 return(0);
       case '\\': case '_': case '^': case '{': case '}':
	 return(1);
      }
   }
   /*NOTREACHED*/
}
	 
/*
 * Draw a dot in the middle of a TeX string
 */
static void
tex_dot(n,style,expd,dang)
int n,
    style;
double expd,
       dang;
{
   REAL ang = dang;
   REAL save_angle,save_expand;
   int save_gx1, save_gx2, save_gy1, save_gy2;
   float ux,uy;				/* user coordinates to draw point */

   ux = (xp - ffx)/fsx;
   uy = (yp - ffy)/fsy;

   save_angle = aangle;
   save_expand = eexpand;
   save_gx1 = gx1; save_gx2 = gx2; save_gy1 = gy1, save_gy2 = gy2;
   set_angle(&ang,1);
   eexpand = expd;
/*
 * don't use sm_location, as it resets the conversion of user to SCREEN coords
 * and all we want to do is to disable the bounds checking
 */
   gx1 = 0; gx2 = SCREEN_SIZE; gy1 = 0; gy2 = SCREEN_SIZE;

   sm_draw_point(ux,uy,n,style);

   eexpand = save_expand;
   set_angle(&save_angle,1);
   gx1 = save_gx1; gx2 = save_gx2; gy1 = save_gy1; gy2 = save_gy2;
}

/*
 * Draw a line in the middle of a TeX string
 */
static void
tex_rule(lt,len,type)
int lt;
double len;				/* length, SCREEN units */
int type;				/* 'h' or 'v' */
{
   int old_ltype = lltype;
   REAL a,old_ang = aangle;

   a = aangle + (type == 'v' ? 90 : 0);
   set_angle(&a,1);
   sm_ltype(lt);

   draw_dline(xp,yp,(int)(xp+xasp*len*sm_cos),(int)(yp + len*sm_sin/yasp));

   set_angle(&old_ang,1);
   sm_ltype(old_ltype);
}

/*
 * Now the stuff to do with TeX definitions
 */
#define VALSIZE (TEX_VAL_SIZE + 400)	/* macro's size after arg. expansion */
typedef struct {
   char *value;
   int font;
   int narg;
} TEX_NODE;

static char *vvalue;
static int ffont;
static int nnarg;
static Void *tmake();
static TREE tt = { NULL, NULL, tmake};

static void
set_tex_def(name,value,fnt,narg)
char *name;
char *value;
int fnt;
int narg;
{
   vvalue = value;
   ffont = fnt;
   if((nnarg = narg) > 9) {		/* actually this is impossible... */
      msg_1s("Too many arguments to TeX macro %s\n",name);
      return;
   }

   insert_node(name,&tt);
}

/***************************************************************/
/*
 * make a new TeX definition node
 */
static Void *
tmake(name,nn)
char *name;
Void *nn;			/* current value of node */
{
   TEX_NODE *node;
   
   if(nn == NULL) {
      if((node = (TEX_NODE *)malloc(sizeof(TEX_NODE))) == NULL) {
	 msg_1s("malloc returns NULL in tmake() for %s\n",name);
	 return(NULL);
      }
      if((node->value = malloc(strlen(vvalue) + 1)) == NULL) {
	 fprintf(stderr,"Can't allocate storage for node->value\n");
	 free((char *)node);
	 return(NULL);
      }
   } else {
      node = (TEX_NODE *)nn;
      if((node->value = realloc(node->value,strlen(vvalue) + 1)) == NULL) {
	 fprintf(stderr,"Can't reallocate storage for node->value\n");
	 free((char *)node);
	 return(NULL);
      }
   }
   (void)strcpy(node->value,vvalue);
   node->font = ffont;
   node->narg = nnarg;

   return((Void *)node);
}

/*
 * expand the definition of a "TeX" macro, if it is defined
 */
#define ARGSIZE 80

static char *
tex_macro(name,drawit,font_id,sfac,shift,slant,condense,str)
char *name;				/* name of "macro" */
int drawit,				/* do I actually want to draw this? */
    font_id;				/* font to use */
double sfac,				/* current expansion */
       shift,				/* level of sub/superscript */
       slant,				/* slant of \it font */
       condense;			/* amount to condense font */
char *str;				/* where we have read to */
{
   char args[9][ARGSIZE + 1];		/* arguments to macro; allow for '\0'*/
   int i,j,k;
   TEX_NODE *node;
   char *ptr;
   char value[VALSIZE];			/* expanded value of macro */

   if((node = (TEX_NODE *)get_rest(name,&tt)) == NULL) {/* unknown variable */
      if(drawit) msg_1s("Unknown \"TeX\" macro: `%s'\n",name);
   } else {
      if(node->font != CURRENT) {
	 font_id = node->font | (font_id & (BOLD | ITALIC));
      }
	 
      for(i = 0;i < node->narg;i++) {
	 if(*str == '\\') {		/* a single escape sequence */
	    for(j = 0;j < ARGSIZE;j++) {
	       args[i][j] = *str++;
	       if(!isalpha(args[i][j])) break;
	    }
	    if(j == 0) {
	       j++;			/* single non-alpha character */
	    } else if(!isspace(args[i][j])) {
	       str--;			/* went one too far */
	    }
	    args[i][j] = '\0';
	 } else {
	    str = read_group(args[i],ARGSIZE,str,drawit);
	 }
      }

      for(i = 0, ptr = node->value;i < VALSIZE && *ptr != '\0';) {
	 if(*ptr == '#') {
	    ptr++;
	    if(*ptr == '#') {		/* ##: escaped # */
	       value[i++] = '#';
	       ptr++;
	       continue;
	    } else {			/* real argument */
	       if((j = *ptr - '0') > node->narg || j < 1) {
		  msg_1d("Undefined argument #%d ",j);
		  msg_1s("to macro %s\n",name);
		  ptr++;
		  continue;
	       }
	       j--;			/* args is 0-indexed */
	       for(k = 0;args[j][k] != '\0' && i < VALSIZE;) {
		  value[i++] = args[j][k++];
	       }
	       ptr++;
	    }
	 } else {
	    value[i++] = *ptr++;
	 }
      }
      value[i] = '\0';
      
      (void)tex_string(value,drawit,font_id,sfac,shift,slant,condense);
   }
   return(str);
}

/*****************************************************************************/
/*
 * See if a TeX definition specifies a font alias
 */
static char *
tex_font_alias(name)
char *name;				/* name of "macro" */
{
   TEX_NODE *node;

   if((node = (TEX_NODE *)get_rest(name,&tt)) != NULL) { /* a known name */
      if(node->narg == -1) {		/* a font alias */
	 return(node->value);
      }
   }
   return(NULL);
}

/*****************************************************************************/
/*
 * Read a group; either a single letter, a single escape sequence,
 * or a {real group}
 */
static char *
read_group(group_val,size,str,printit)
char *group_val;			/* the group to return */
int size;				/* dimension of group_val[] */
char *str;				/* string to read from */
int printit;				/* should I print errors? */
{
   int brace_level = 0;			/* match {} in group */
   int i;
   char *start = str;			/* start of group */
   
   if(*str != '{') {			/* a group of one character or \name */
      if(*str == '\\') {		/* a single escape sequence */
	 group_val[0] = *str++;
	 for(i = 1;i < size;i++) {
	    group_val[i] = *str++;
	    if(!isalpha(group_val[i])) break;
	 }
	 if(i == 0) {
	    i++;			/* single non-alpha character */
	 } else if(!isspace(group_val[i])) {
	    str--;			/* went one too far */
	 }
	 group_val[i] = '\0';
      } else {				/* a single character */
	 group_val[0] = *str++;
	 group_val[1] = '\0';
      }
      return(str);
   }

   str++;				/* skip the '{' */
   brace_level++;
   for(i = 0;i < size;i++) {
      group_val[i] = *str++;
      if(group_val[i] == '\0') {
	 str--;
	 if(brace_level && printit) {
	    msg_1s("Unpaired {} in group \"%s\"\n",start);
	 }
	 break;
      } else if(group_val[i] == '}') {
	 if(brace_level > 1) {
	    brace_level--;
	 } else {
	    break;
	 }
      } else if(group_val[i] == '{') {
	 brace_level++;
      }
   }
   group_val[i] = '\0';
   return(str);
}

/*
 * Get a number, either a real number like 12 or -3 or +4, or
 * +-\depth{...}, +-\height{...}, or +-\width{...}.
 *
 * The dimensions are given before allowing for the current EXPAND
 * or aspect ratio.
 */
static char *
get_num(str,num,font_id,shift,slant,condense,printit)
char *str;
int *num;
int font_id;
double shift;
double slant;
double condense;
int printit;				/* print error messages? */
{
   int dnum;
   char group_val[TEX_VAL_SIZE];
   int negative = 0;			/* is number negative? */

   while(isspace(*str)) str++;
   if(*str == '-') {
      negative = 1;
      str++;
   } else if(*str == '+') {
      str++;
   }

   if(!strncmp(str,"\\advance",8)) {
      str += 8;
      str = get_num(str,num,font_id,shift,slant,condense,printit);
      while(isspace(*str)) str++;
      if(!strncmp(str,"by",2)) str += 2;
      str = get_num(str,&dnum,font_id,shift,slant,condense,printit);
      *num += dnum;
   } else if(!strncmp(str,"\\depth",6)) {
      str += 6;
      str = read_group(group_val,TEX_VAL_SIZE,str,printit);
      gdimen(group_val,font_id,shift,slant,condense,num,'d');
   } else if(!strncmp(str,"\\divide",7)) {
      str += 7;
      str = get_num(str,num,font_id,shift,slant,condense,printit);
      while(isspace(*str)) str++;
      if(!strncmp(str,"by",2)) str += 2;
      str = get_num(str,&dnum,font_id,shift,slant,condense,printit);
      *num /= (float)dnum/1000;
   } else if(!strncmp(str,"\\height",7)) {
      str += 7;
      str = read_group(group_val,TEX_VAL_SIZE,str,printit);
      gdimen(group_val,font_id,shift,slant,condense,num,'h');
   } else if(!strncmp(str,"\\width",6)) {
      str += 6;
      str = read_group(group_val,TEX_VAL_SIZE,str,printit);
      gdimen(group_val,font_id,shift,slant,condense,num,'w');
   } else if(!strncmp(str,"\\multiply",9)) {
      str += 9;
      str = get_num(str,num,font_id,shift,slant,condense,printit);
      while(isspace(*str)) str++;
      if(!strncmp(str,"by",2)) str += 2;
      str = get_num(str,&dnum,font_id,shift,slant,condense,printit);
      *num *= (float)dnum/1000;
   } else {
      if(isdigit(*str)) {
	 *num = atoi(str);
	 while(isdigit(*str)) str++;
      } else {
	 if(printit) {
	    msg_1s("Expecting a number, found \"%s\"\n",str);
	 }
	 *num = 0;
      }
   }
   if(negative) *num = -*num;
   return(str);
}

/*****************************************************************************/
/*
 * Find the length of a string as drawn, but assuming that the
 * aspect ratio and EXPAND are both one.
 */
static void
gdimen(str,font_id,shift,slant,condense,val,type)
int font_id;
double shift;
double slant,condense;
char *str;
int *val;
int type;
{
   int saved[NVAL];
   
   SAVE_VALS(saved);

   swidth = sdepth = sheight = baseline = 0;
   (void)tex_string(str,0,font_id,1.0,shift,slant,condense);

   switch (type) {
    case 'd':
      *val = sdepth;
      break;
    case 'h':
      *val = sheight;
      break;
    case 'w':
      *val = swidth;
      break;
    default:
      msg_1d("Unknown type in gdimen: %c\n",type);
      *val = 0;
      break;
   }

   RESTORE_VALS(saved);
}
