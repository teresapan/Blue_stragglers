#include "copyright.h"
/*
 * Read the fonts files, and convert them to a binary format, then read
 * the TeX definition file and convert it to binary too.
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(__BORLANDC__)
#  include <fcntl.h>
#  include <sys\stat.h>
   extern unsigned _stklen = 0xe000;
#endif
#include "fonts.h"
#include "sm_declare.h"

#define LSIZE 81			/* length of a line */
#define LN2I 1.442695022		/* 1/ln(e) */
#define VERY_SMALL 1e-5

typedef struct {
   int nstroke,				/* number of strokes in a character */
       num,				/* number of character */
       offset,				/* offset in data */
       rank;				/* desired rank in data */
} GLYPH;

static int get_nums(),
	   read_index();
static void sort();

#if defined(__BORLANDC__)
#  define FOPEN_MODE "rt"
#else
#  define FOPEN_MODE "r"
#endif

int
main(ac,av)
int ac;
char *av[];
{
   char c,
   	line[LSIZE],
        nstroke[NCHAR*NFONT],		/* number of strokes in character */
	tex_font_name[TEX_NAME_SIZE],	/* name of a font */
	tex_name[TEX_NAME_SIZE],	/* name of a TeX definition */
	tex_value[TEX_VAL_SIZE];	/* value of a TeX definition */
   Signed char depth[NCHAR*NFONT],	/* depth of character */
	       *font,			/* actual description of characters */
	       height[NCHAR*NFONT],	/* height of character */
	       ih,id,			/* height and depth of character */
	       il,ir,			/* left and right offsets */
   	       iy,			/* y-coord of a point in the font */
	       ladj[NCHAR*NFONT],	/* left adjustment of character */
	       radj[NCHAR*NFONT];	/* right adjustment of character */
   FILE *infil;				/* in and out streams */
   GLYPH index[NCHAR*NFONT];		/* index to which characters we want */
   int i,j,k,
       narg,				/* number of arguments */
       ndata,				/* number of ints describing chars */
       ndefs,				/* number of TeX definitions */
       nglyph,				/* number of glyphs requested */
       ns,				/* number of strokes in a glyph */
       num,				/* number of a glyph */
       off,				/* offset of a character */
       offset[NCHAR*NFONT],		/* offsets of characters in font */
       out,				/* fd for binary file */
       tex_font;			/* font for a TeX definition */
   if(ac < 5) {
      fprintf(stderr,
	"Syntax: read_fonts data_file index_file TeX_defs_file binary_file\n");
      exit(EXIT_OK);
   }

   if((infil = fopen(av[1],FOPEN_MODE)) == NULL) {
      fprintf(stderr,"Can't open %s\n",av[1]);
      exit(EXIT_BAD);
   }
   if((nglyph = read_index(index,av[2])) < 0) {
      exit(EXIT_BAD);
   }
#if defined(__BORLANDC__)
   _fmode = O_BINARY;
   if((out = creat(av[4],S_IWRITE)) < 0) {
#else
   if((out = creat(av[4],0644)) < 0) {
#endif
      fprintf(stderr,"Can't open %s\n",av[4]);
      exit(EXIT_BAD);
   }
/*
 * Now sort index into increasing glyph number
 */
   sort(index,nglyph);
/*
 * index is in increasing glyph order, so read file to find offsets
 */
   for(i = 0;i < nglyph;) {
      if(fgets(line,LSIZE,infil) == NULL) {
	 fprintf(stderr,"Reached end of file after reading %d glyphs\n",i);
	 exit(EXIT_BAD);
      }
      if(get_nums(line,&num,&ns) < 2) {
	 fprintf(stderr,"Error reading %dth glyph\n",i+1);
	 fprintf(stderr,"\"%s\"\n",line);
	 exit(EXIT_BAD);
      }
/*
 * Reading a continuation line for ns == 32 is due to a bug in
 * the code that wrote the font tables: they include a blank line
 */
      if(ns >= 32) fgets(line,LSIZE,infil); /* skip continuation lines */
      for(j = ns - 68;j > 0;j -= 36) {
	 fgets(line,LSIZE,infil);
      }

      if(num > index[i].num) {
	 fprintf(stderr,"Glyph %d is missing\n",index[i].num);
	 exit(EXIT_BAD);
      } else if(num == index[i].num) {
	 nstroke[index[i].rank] = index[i].nstroke = ns - 1;
	 i++;
	 while(index[i].num == num) {
	    nstroke[index[i].rank] = index[i].nstroke = ns - 1;
	    i++;
	 }
      }
   }
   fseek(infil,0,0);			/* rewind file */
/*
 * We now have the lengths of all the characters, so we can find the offsets.
 */
   for(off = i = 0;i < nglyph;i++) {
      index[i].offset = off;
      offset[index[i].rank] = off + 1;	/* offset is 1-indexed */
      if(i > 0 && index[i].num == index[i-1].num) {
	 offset[index[i].rank] = offset[index[i-1].rank];
      } else {
	 off += 2*index[i].nstroke;	/* increment offset for next char */
      }
   }

   ndata = off;
   if((font = (Signed char *)malloc(ndata)) == NULL) {
      fprintf(stderr,"Can't malloc space for array font\n");
      exit(EXIT_BAD);
   }
/*
 * So read the offsets etc.
 */
   for(i = 0;i < nglyph;) {
      if(fgets(line,LSIZE,infil) == NULL) {
	 fprintf(stderr,"Reached end of file after reading %d glyphs\n",i);
	 exit(EXIT_BAD);
      }
      if(get_nums(line,&num,&ns) < 2) {
	 fprintf(stderr,"Error reading %dth glyph\n",i+1);
	 exit(EXIT_BAD);
      }
      if(num < index[i].num) {
/*
 * Reading a continuation line for ns == 32 is due to a bug in
 * the code that wrote the font tables: they include a blank line
 */
	 if(ns >= 32) fgets(line,LSIZE,infil); /* continuation lines */
	 for(j = ns - 68;j > 0;j -= 36) {
	    fgets(line,LSIZE,infil);
	 }
      } if(num > index[i].num) {
	 fprintf(stderr,"Glyph %d is missing\n",index[i].num);
	 exit(EXIT_BAD);
      } else if(num == index[i].num) {
	 sscanf(&line[8],"%c",&il);
	 ladj[index[i].rank] = il - 'R';
	 sscanf(&line[9],"%c",&ir);
	 radj[index[i].rank] = ir - 'R';
	 off = index[i].offset;
	 ns--;				/* first `stroke' is [lr]adj */
	 ih = id = 0;
	 for(j = 0,k = 10;j < ns;j++,k += 2) {
	    if(k == 72) {		/* next line please */
	       if(fgets(line,LSIZE,infil) == NULL) {
		  fprintf(stderr,"Reached end of file in %dth glyph\n",i);
		  exit(EXIT_BAD);
	       }
	       k = 0;
	    }
	    sscanf(&line[k],"%c",&c);	/* x */
	    font[off + 2*j] = (c == ' ') ? FONT_MOVE : c - 'R';
	    sscanf(&line[k+1],"%c",&c);	/* y */
	    iy = font[off + 2*j + 1] = -(c - '[');
	    if(font[off + 2*j] != FONT_MOVE) {
	       if(iy > ih) ih = iy;
	       if(iy < id) id = iy;
	    }
	 }
	 depth[index[i].rank] = -id;
	 height[index[i].rank] = ih;
	 if(ns == 31) fgets(line,LSIZE,infil); /* 32 bug again */
	 i++;
	 while(index[i].num == num) {
	    ladj[index[i].rank] = il - 'R';
	    radj[index[i].rank] = ir - 'R';
	    depth[index[i].rank] = -id;
	    height[index[i].rank] = ih;
	    i++;
	 }
      }
   }
   (void)fclose(infil);

   i = FONT_FILE_TYPE;
   write(out,(char *)&i,sizeof(int));
   
   write(out,(char *)&nglyph,sizeof(int));
   write(out,(char *)&ndata,sizeof(int));
   write(out,nstroke,nglyph);
   write(out,(char *)ladj,nglyph);
   write(out,(char *)radj,nglyph);
   write(out,(char *)depth,nglyph);
   write(out,(char *)height,nglyph);
   write(out,(char *)offset,nglyph*sizeof(int));
   write(out,(char *)font,ndata);
/*
 * Now deal with the file of TeX definitions
 */
   if((infil = fopen(av[3],FOPEN_MODE)) == NULL) {
      fprintf(stderr,"Can't open TeX definitions file %s\n",av[3]);
      exit(EXIT_BAD);
   }
/*
 * count number of definitions
 */
   for(ndefs = 0;fgets(line,LSIZE,infil) != NULL;) {
      if(line[0] != '#' && line[strlen(line) - 1] != '\\') ndefs++;
   }
   (void)fseek(infil,0,0L);		/* and rewind */
   
   write(out,(char *)&ndefs,sizeof(int));
   for(i = 0;i < ndefs;i++) {
      (void)fgets(line,LSIZE,infil);	/* can't be NULL */
      if(line[0] == '#') {
	 i--;				/* a comment, so not one of ndefs */
	 continue;
      }
      for(j = k = 0;k < TEX_NAME_SIZE && line[j] != '\0';j++) {
	 if(isspace(line[j])) {
	    while(k < TEX_NAME_SIZE) tex_name[k++] = '\0';
	    while(isspace(line[j])) {
	       j++;
	    }
	    break;
	 }
	 tex_name[k++] = line[j];
      }

      narg = 1;
      if(line[j] == '-') {		/* look for a number of arguments */
	 j++;
	 narg = -1;
      }
      if(isdigit(line[j])) {
	 narg *= line[j++] - '0';
	 while(isspace(line[j])) {
	    j++;
	 }
      } else {
	 narg = 0;
      }

      for(k = 0;k < TEX_NAME_SIZE && line[j] != '\0';j++) {
	 if(isspace(line[j])) {
	    tex_font_name[k] = '\0';

	    for(k = 0;k < NFONT;k++) {
	       if(!strcmp(tex_font_name,fonts[k].ab1) ||
		  		!strcmp(tex_font_name,fonts[k].ab2)) {
		  tex_font = k;
		  break;
	       }
	    }
	    if(k == NFONT) {
	       if(!strcmp(tex_font_name,"cu")) {
		  tex_font = CURRENT;
	       } else {
		  fprintf(stderr,"`%s' is not a font\n",tex_font_name);
		  tex_font = ROMAN;
	       }
	    }
	    break;
	 }
	 tex_font_name[k++] = line[j];
      }
      j++;			/* skip one character before definition */
      for(k = 0;k < TEX_VAL_SIZE && line[j] != '\0';j++) {
	 if(line[j] == '\n') {
	    if(j > 0 && line[j - 1] == '\\') {
	       k--;
	       (void)fgets(line,LSIZE,infil);	/* can't be NULL */
	       for(j = 0;isspace(line[j]);j++) continue;
	       j--;			/* it'll be ++'ed */
	       continue;
	    } else {
	       break;
	    }
	 }
	 tex_value[k++] = line[j];
      }
      write(out,tex_name,TEX_NAME_SIZE);
      if(narg < 0) {
	 sprintf(tex_value,"\\%s",tex_font_name);
	 k = strlen(tex_value);
      }
      write(out,(char *)&k,sizeof(int)); /* length of value */
      write(out,tex_value,k);
      write(out,(char *)&tex_font,sizeof(int));
      write(out,(char *)&narg,sizeof(int));
   }

   (void)fclose(infil);
   (void)close(out);
   return(EXIT_OK);
}

/*************************************************************/
/*
 * Read the index file, which should consist of a list of
 * desired glyphs (by Hershey number), or ranges of glyphs
 */
static int
read_index(index,indexfile)
GLYPH *index;
char *indexfile;
{
   char c,
   	line[LSIZE];
   FILE *fil;
   int i,j,k,
       num;				/* number of glyph */

   if((fil = fopen(indexfile,FOPEN_MODE)) == NULL) {
      fprintf(stderr,"Can't open %s\n",indexfile);
      return(-1);
   }

   for(i = 0;i < NFONT;i++) {
      for(j = 0;j < NCHAR;) {
	 for(;;) {			/* deal with comments */
#ifdef vms       			/* VMS has a bug with mixing i/o: */
	    fscanf(fil,"%[ \t\n\f]",line); /* skip whitespace */
      	    if(fscanf(fil,"%[#]",line) <= 0) { /* Does field start with a #? */
	       break;
	    }
	    fscanf(fil,"%*[^\n]\n");	/* eat rest of line */
#else
	    while(c = getc(fil), isspace(c)) continue;
	    if(c == '#') {		/* a comment */
     	       fgets(line,LSIZE,fil);	/* read to end of line */
	    } else { 
	       ungetc(c,fil);
	       break;
	    }
#endif
	 }

	 if(fscanf(fil,"%d",&num) < 1) {
	    fclose(fil);
	    return(NCHAR*i + j);
	 }
	 if(num > 0) {
	    index[i*NCHAR + j].num = num;
	    index[i*NCHAR + j].rank = i*NCHAR + j;
	    j++;
	 } else {
	    for(k = index[i*NCHAR + j - 1].num + 1;k <= -num;k++) {
	       if(j >= NCHAR) {
		  fprintf(stderr,"Range to %d doesn't fit in %dth font\n",
			  					      -num,i);
		  break;
	       }
	       index[i*NCHAR + j].num = k;
	       index[i*NCHAR + j].rank = i*NCHAR + j;
	       j++;
	    }
	 }
      }
   }

   fclose(fil);
   return(NCHAR*NFONT);
}
      

static int
get_nums(line,num,ns)
char *line;
int *num,
    *ns;
{
   char buff[6];
   int i;
   
   for(i = 0;i < 5;i++) buff[i] = line[i];
   buff[i] = '\0';
   *num = atoi(buff);
   for(i = 0;i < 3;i++) buff[i] = line[i+5];
   buff[i] = '\0';
   *ns = atoi(buff);
   if(*num == 0 && *ns == 0) {
      return(0);
   }
   return(2);
}

/*
 * We could use qsort, except that it is missing under VMS
 *
 * The sort is a Shell sort, taken from Press et.al. Numerical Recipes.
 */
static void
sort(array,dimen)
GLYPH *array;
int dimen;
{
   GLYPH temp;
   int i,j,m,n,				/* counters */
       lognb2;				/* (int)(log_2(dimen)) */

   lognb2 = log((double)dimen)*LN2I + VERY_SMALL; /* ~ log_2(dimen) */
   m = dimen;
   for(n = 0;n < lognb2;n++) {
      m /= 2;
      for(j = m;j < dimen;j++) {
         i = j - m;
	 temp = array[j];
	 while(i >= 0 && temp.num < array[i].num) {
	    array[i + m] = array[i];
	    i -= m;
	 }
	 array[i + m] = temp;
      }
   }
}
