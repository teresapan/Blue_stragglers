/*
 * concatenate a number of SM font files
 */
#include <stdio.h>

#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(__BORLANDC__)
#  include <fcntl.h>
#endif
#include "fonts.h"
#include "sm_declare.h"

typedef struct {
   char name[TEX_NAME_SIZE];
   char val[TEX_VAL_SIZE];
   int font;
   int narg;
} TEX_DEF;

#if defined(__MSDOS__)
#  define OPEN_MODE O_BINARY
#else
#  define OPEN_MODE 0
#endif

#define RETURN(I)			/* cleanup and return */ \
  if(fbin >= 0) close(fbin); \
  if(font != NULL) free((char *)font); \
  if(id != NULL) free(id); \
  if(ih != NULL) free(ih); \
  if(ir != NULL) free(ir); \
  if(il != NULL) free(il); \
  if(fp != NULL) free(fp); \
  if(ns != NULL) free(ns); \
  return(I);

static char *ns = NULL;			/* number of strokes in character */
static Signed char *font = NULL,	/* ptr to stroke table */
		   *id = NULL,		/* depth of character */
		   *ih = NULL,		/* height of character */
		   *il = NULL,		/* left adjust */
		   *ir = NULL;		/* right adjust */
static int *fp = NULL;			/* pointer to chars */
static int nchar = 0;			/* Number of characters in fonts,
					   NFONT*NCHAR */
static int ndata = 0;			/* number of numbers describing
					   characters in fonts */
static int nfont = 0;			/* number of fonts loaded */
static int ndefs = 0;
static TEX_DEF *defs = NULL;

static int read_font();
static void usage();

int
main(ac,av)
int ac;
char **av;
{
   int fdout = 1;			/* output file descriptor */
   int i;
   int len;				/* length of a TeX definition */
   
   av++; ac--;
   
   while(ac > 0 && av[0][0] == '-') {
      switch(av[0][1]) {
       case 'o':			/* specify an output file */
	 if(ac == 1) {
	    fprintf(stderr,"You must specify an output file with -o\n");
	    exit(1);
	 }
	 if((fdout = creat(av[1],0644)) < 0) {
	    fprintf(stderr,"Can't create %s\n",av[1]);
	    exit(1);
	 }
	 ac--; av++;
	 break;
       default:
	 fprintf(stderr,"Unknown option: %s\n",av[0]);
	 usage();
	 exit(1);
      }
      ac--; av++;
   }

   if(ac == 0) {
      usage();
      exit(1);
   }
/*
 * read the font files
 */
   while(ac != 0) {
      if(read_font(av[0]) < 0) {
	 exit(1);
      }
      ac--; av++;
   }
/*
 * and write the results
 */
   i = FONT_FILE_TYPE;
   write(fdout,(char *)&i,sizeof(int));
   
   write(fdout,(char *)&nchar,sizeof(int));
   write(fdout,(char *)&ndata,sizeof(int));
   write(fdout,ns,nchar);
   write(fdout,(char *)il,nchar);
   write(fdout,(char *)ir,nchar);
   write(fdout,(char *)id,nchar);
   write(fdout,(char *)ih,nchar);
   write(fdout,(char *)fp,nchar*sizeof(int));
   write(fdout,(char *)font,ndata);

   write(fdout,(char *)&ndefs,sizeof(int));
   for(i = 0;i < ndefs;i++) {
      write(fdout,defs[i].name,TEX_NAME_SIZE);
      len = strlen(defs[i].val);
      write(fdout,(char *)&len,sizeof(int));
      write(fdout,defs[i].val,len);
      write(fdout,(char *)&defs[i].font,sizeof(int));
      write(fdout,(char *)&defs[i].narg,sizeof(int));
   }
   if(fdout != 1) {
      close(fdout);
   }
   
   return(0);
}   

/*****************************************************************************/
/*
 * Print usage information
 */
static void
usage()
{
   fprintf(stderr,"Usage: cat_fonts [-o outfile] file [file ...]\n");
}

/*****************************************************************************/
/*
 * read a font file
 */
static int
read_font(font_file)
char *font_file;
{
   int fbin = -1,
       i,
       val_size;
   int nc,nd;				/* nchar and (ndata or ndefs)
					   for this file */

   if((fbin = open(font_file,OPEN_MODE)) < 0) {
      fprintf(stderr,"Can't find font binary file %s\n",font_file);
      return(-1);
   }

   if(read(fbin,(char *)&i,sizeof(int)) != sizeof(int)) {
      fprintf(stderr,"Can't read font file type from %s\n",font_file);
      RETURN(-1);
   }
   if(i != FONT_FILE_TYPE) {
      fprintf(stderr,
	  "Font file format level is %d, not %d; You must re-run read_fonts\n",
	      i,FONT_FILE_TYPE);
      RETURN(-1);
   }
   if(read(fbin,(char *)&nc,sizeof(int)) != sizeof(int)) {
      fprintf(stderr,"Can't read nchar from %s\n",font_file);
      RETURN(-1);
   }
   if(nc%NCHAR != 0) {
      fprintf(stderr,"Last %d chars don't fit into a font\n",nc%NCHAR);
      nc = (int)(nc/NCHAR)*NCHAR;
   }
   nfont += nc/NCHAR;

   if(nfont > NFONT) {
      fprintf(stderr,"Too many fonts (%d, > %d) specified in read_font\n",
	      nfont,NFONT);
      nfont = NFONT;
   }
   if(read(fbin,(char *)&nd,sizeof(int)) != sizeof(int)) {
      fprintf(stderr,"Can't read ndata from %s\n",font_file);
      RETURN(-1);
   }
   if(ndata == 0) {
      font = (Signed char *)malloc(nd);
      id =  (Signed char *)malloc(nc);
      ih =  (Signed char *)malloc(nc);
      il =  (Signed char *)malloc(nc);
      ir =  (Signed char *)malloc(nc);
   } else {
      font = (Signed char *)realloc(font,ndata + nd);
      id =  (Signed char *)realloc(id,nchar + nc);
      ih =  (Signed char *)realloc(ih,nchar + nc);
      il =  (Signed char *)realloc(il,nchar + nc);
      ir =  (Signed char *)realloc(ir,nchar + nc);
   }
   if(font == NULL || id == NULL || ih == NULL || il == NULL || ir == NULL) {
      fprintf(stderr,"Couldn't allocate space for font table\n");
      RETURN(-1);
   }

   if(nchar == 0) {
      if((ns = malloc(nc)) == NULL) {
	 fprintf(stderr,"Can't allocate storage for ns\n");
	 RETURN(-1);
      }
   } else {
      if((ns = realloc(ns,nchar + nc)) == NULL) {
	 fprintf(stderr,"Can't reallocate storage for ns\n");
	 RETURN(-1);
      }
   }
   if(read(fbin,ns + nchar,nc) != nc) {
      fprintf(stderr,"Can't read ns from %s\n",font_file);
      RETURN(-1);
   }
   
   if(read(fbin,(char *)(il + nchar),nc) != nc) {
      fprintf(stderr,"Can't read il from %s\n",font_file);
      RETURN(-1);
   }
   if(read(fbin,(char *)(ir + nchar),nc) != nc) {
      fprintf(stderr,"Can't read ir from %s\n",font_file);
      RETURN(-1);
   }
   if(read(fbin,(char *)(id + nchar),nc) != nc) {
      fprintf(stderr,"Can't read id from %s\n",font_file);
      RETURN(-1);
   }
   if(read(fbin,(char *)(ih + nchar),nc) != nc) {
      fprintf(stderr,"Can't read ih from %s\n",font_file);
      RETURN(-1);
   }

   if(nchar == 0) {
      if((fp = (int *)malloc(nc*sizeof(int))) == NULL) {
	 fprintf(stderr,"Can't allocate storage for fp\n");
	 RETURN(-1);
      }
   } else {
      if((fp = (int *)realloc(fp,(nchar + nc)*sizeof(int))) == NULL) {
	 fprintf(stderr,"Can't allocate storage for fp\n");
	 RETURN(-1);
      }
   }
   if(read(fbin,(char *)(fp + nchar),nc*sizeof(int)) != nc*sizeof(int)) {
      fprintf(stderr,"Can't read fp from %s\n",font_file);
      RETURN(-1);
   }
   for(i = 0;i < nc;i++) {		/* fix fp for previous fonts read */
      fp[nchar + i] += ndata;
   }
   
   if(read(fbin,(char *)(font + ndata),nd) != nd) {
      fprintf(stderr,"Can't read font from %s\n",font_file);
      RETURN(-1);
   }
   nchar += nc;
   ndata += nd;
/*
 * Now read TeX definitions
 */
   if(read(fbin,(char *)&nd,sizeof(int)) != sizeof(int)) {
      fprintf(stderr,"Can't read number of TeX definitions in %s\n",font_file);
      close(fbin);
      return(-1);
   }

   if(ndefs == 0) {
      if((defs = (TEX_DEF *)malloc(nd*sizeof(TEX_DEF))) == NULL) {
	 fprintf(stderr,"Can't allocate storage for TeX definitions\n");
	 close(fbin);
	 return(-1);
      }
   } else {
      if((defs = (TEX_DEF *)realloc(defs,(nd + ndefs)*sizeof(TEX_DEF)))
	 							     == NULL) {
	 fprintf(stderr,"Can't allocate storage for TeX definitions\n");
	 close(fbin);
	 return(-1);
      }
   }

   for(i = ndefs;i < ndefs + nd;i++) {
      if(read(fbin,defs[i].name,TEX_NAME_SIZE) != TEX_NAME_SIZE) {
	 fprintf(stderr,"Can't read name of %d'th TeX definition in %s\n",
		 i + 1 - ndefs,font_file);
	 break;
      }
      if(read(fbin,(char *)&val_size,sizeof(int)) != sizeof(int)) {
	 fprintf(stderr,"Can't read length of TeX definition for \"%s\"\n",
		 defs[i].name);
	 break;
      }
      if(read(fbin,defs[i].val,val_size) != val_size) {
	 fprintf(stderr,"Can't read value for TeX definition \"%s\"\n",
		 defs[i].name);
	 break;
      }
      defs[i].val[val_size] = '\0';
      if(read(fbin,(char *)&defs[i].font,sizeof(int)) != sizeof(int)) {
	 fprintf(stderr,"Can't read font for TeX definition \"%s\"\n",
		 defs[i].name);
	 break;
      }
      if(read(fbin,(char *)&defs[i].narg,sizeof(int)) != sizeof(int)) {
	 fprintf(stderr,"Can't read narg for TeX definition \"%s\"\n",
		defs[i].name);
	 break;
      }
   }
   ndefs += nd;
   close(fbin);
   
   return(0);
}
