#include "copyright.h"
/*
 * Read Mongo files.
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "sm_declare.h"
#ifndef EXCLAMATION_IS_COMMENT
#define EXCLAMATION_IS_COMMENT 1	/* Did an ! introduce a comment in
					   Mongo? */
#endif
#define DSIZE 2000		/* amount to increment storage for macro by */
#define EXTRA 100		/* space for a last line */

extern int sm_interrupt;		/* respond to ^C */
static int get_old_line();

void
read_old(name,file)
char name[],			/* name for new macro */
     file[];			/* file to read */
{
   char *macro,			/* macro of file */
	rest[80],		/* rest of line */
	sub_name[40];		/* name of a macro defined within the file */
   FILE *fil;
   int arg,				/* an argument */
       i,j,k,
       in_macro = 0,			/* are we in a macro definition? */
       narg = 0,			/* number of arguments */
       save_i = 0,			/* index of start of definition */
       size;				/* current dimension of macro */

   if((fil = fopen(file,"r")) == NULL) {
      msg_1s("Can't open %s\n",file);
      return;
   }
   size = DSIZE;
   if((macro = malloc((unsigned)size)) == NULL) {
      msg("Can't get storage for macro in read_old\n");
      return;
   }

   for(i = 0;get_old_line(&macro[i],size - i - 1,fil) >= 0;) {
      if(sm_interrupt) {
	 (void)fclose(fil);
	 return;
      }
      if(in_macro) {
	 if(!strncmp(&macro[i],"end",3) || !strncmp(&macro[i],"END",3)) {
	    macro[i-1] = '\0';		/* end sub-definition */
	    (void)define(sub_name,&macro[save_i],narg,narg,0);
	    i = save_i;
	    in_macro = 0;
	    continue;
	 }
	 for(j = i;macro[j] != '\0';j++) {	/* look for arguments */
	    if(macro[j] == '&') {
	       if(isdigit(macro[j+1])) {
		  macro[j] = '$';
		  arg = macro[j+1] + 0 - '0';
		  if(arg > narg) {
		     narg = arg;
		  }
	       }
	    }
	 }
      } else if(!strncmp(&macro[i],"def",3) || !strncmp(&macro[i],"DEF",3)) {
	 if(str_scanf(&macro[i],"%*s %s %s",sub_name,rest) == 1) {
	    save_i = i;			/* save start of definition */
	    in_macro = 1;
	    narg = 0;
	    continue;			/* skip rest of line */
	 } else {			/* probably a SM variable */
	    ;
	 }
      }
      k = strlen(&macro[i]);
      if((i += k) >= size - EXTRA) {	/* room for a line */
      	 size += DSIZE;
	 if((macro = realloc(macro,(unsigned)size)) == NULL) {
	    msg("Can't realloc storage in read_old\n");
	    return;
	 }
      }
   }

   (void)fclose(fil);
   if(macro[strlen(macro) - 1] == '\n')	{ /* delete last newline */
      macro[strlen(macro) - 1] = '\0';
   }
   (void)define(name,macro,0,0,0);
   free(macro);
}

/*********************************************************/
/*
 * Get a line, and convert some keywords to SM
 */
static int
get_old_line(buff,n,fil)
char buff[];			/* buffer to put line into */
int n;				/* maximum number of characters */
FILE *fil;			/* file to read from */
{
   char word[50];
   int i,j,k,l;

   if(fgets(buff,n - 1,fil) == NULL) {
      buff[0] = '\0';
      return(-1);
   }
/*
 * Strip trailing white space
 */
   for(i = strlen(buff) - 2;i >= 0 && isspace(buff[i]);i--) {
      buff[i] = '\n'; buff[i+1] = '\0';
   }
   
   if(!strncmp(buff,"connect\n",8) || !strncmp(buff,"CONNECT\n",8)) {
      (void)sprintf(buff,"connect x y\n");
   } else if(!strncmp(buff,"data",4) || !strncmp(buff,"DATA",4)) {
      (void)str_scanf(buff,"%*s %s",word);
      (void)sprintf(buff,"data \"%s\"\n",word);
   } else if(!strncmp(buff,"histogram\n",10) ||
	     				!strncmp(buff,"HISTOGRAM\n",10)){
      (void)sprintf(buff,"histogram x y\n");
   } else if(!strncmp(buff,"errorbar",8) || !strncmp(buff,"ERRORBAR",8)){
      if(str_scanf(buff,"%*s %d",&i) == 1) {
	 (void)sprintf(buff,"errorbar x y error_col %d\n",i);
      }
   } else if(!strncmp(buff,"limits\n",7) || !strncmp(buff,"LIMITS\n",7)) {
      (void)sprintf(buff,"limits x y\n");
   } else if(!strncmp(buff,"points\n",7) || !strncmp(buff,"POINTS\n",7)) {
      (void)sprintf(buff,"points x y\n");
   } else if(!strncmp(buff,"window",6) || !strncmp(buff,"WINDOW",6)) {
      if(str_scanf(buff,"%*s %d %d %d %d",&i,&j,&k,&l) == 3) { /* NOT 4 */
	 (void)sprintf(buff,"window %d %d %d %d\n",i,j,
		       (k-1)%j + 1,(k-1)/j + 1);
      }
   } else if(!strncmp(buff,"xcolumn",7) || !strncmp(buff,"XCOLUMN",7)) {
      (void)str_scanf(buff,"%*s %d",&i);
      (void)sprintf(buff,"read x %d\n",i);
   } else if(!strncmp(buff,"ycolumn",7) || !strncmp(buff,"YCOLUMN",7)) {
      (void)str_scanf(buff,"%*s %d",&i);
      (void)sprintf(buff,"read y %d\n",i);
   }
#if EXCLAMATION_IS_COMMENT
/*
 * Now changing characters as needed
 */
   for(i = 0;buff[i] != '\0';i++) {
      if(buff[i] == '!') buff[i] = '#';
   }
#endif

   return(0);
}
