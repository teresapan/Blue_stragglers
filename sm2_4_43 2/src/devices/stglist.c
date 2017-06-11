#include "copyright.h"
/*
 * list all sdgraph devices
 * assume that more() is already initialised
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "sm_declare.h"

#define SIZE 130
extern int sm_interrupt;			/* respond to ^C */

int
stg_list(pattern)
char *pattern;				/* pattern to match */
{
   char ch,
	dev[81],			/* name or alias of device */
   	file[81],
	*graphcap,			/* name of graphcap file */
	line[SIZE+1],			/* line from file */
	outline[SIZE+1];		/* line to print */
   FILE *fil;				/* graphcap file */
   int i,j;
   int k;				/* used to see whether we have seen
					   the main device name yet */
   int nfile;				/* how many files have we read? */

   if(*(graphcap = print_var("graphcap")) == '\0') {
      msg("File graphcap is not defined\n");
      return(0);
   }

   if(*pattern != '\0') {
      if(match_pattern("",pattern,(char **)NULL) == NULL) {/* setup regexp */
	 return(0);
      }
   }
   
   (void)more((char *)NULL);		/* initialise more */

   for(nfile = 0;*graphcap != '\0';nfile++) {
      for(j = 0;j < 80 && *graphcap != '\0' && !isspace(*graphcap);j++) {
	 file[j] = *graphcap++;
      }
      file[j] = '\0';
      while(isspace(*graphcap)) graphcap++;

      if((fil = fopen(file,"r")) == NULL) {
	 sprintf(outline,"%s\tCan't open file %s\n\n",
		 (nfile == 0 ? "" : "\n"),file);
	 more(outline);
	 continue;
      }
      sprintf(outline,"%s\tFile: %s\n\n",(nfile == 0 ? "" : "\n"),file);
      more(outline);

      while(!sm_interrupt && fgets(line,SIZE,fil) != NULL) {
	 outline[0] = '\0';
	 if((i = strlen(line)) == 0) {		/* blank */
	    continue;
	 } else if(line[0] == '#') {		/* comment */
	    continue;
	 } else if(isspace(line[0])) {		/* continuation */
	    continue;
	 } else {
	    k = 0;
	    for(i = j = 0;(ch = line[i]) != ':';i++) {
	       dev[j++] = ch;
	       if(ch == '|' || ch == '\0' || ch == '\n') {
		  dev[j-1] = '\0';
		  if(k == 1) {
		     k = 2;
		     (void)strncat(outline,"( ",SIZE);
		  }
		  (void)strncat(outline,dev,SIZE);
		  (void)strncat(outline," ",SIZE);
		  if(k == 0) {
		     k = 1;
		  }
		  j = 0;
		  if(ch == '\0') break;
	       } else if(k == 0 && ch == '=') { /* a macro definition */
		  k = -1;		/* i.e. continue outer loop */
		  break;
	       }
	    }
	    if(k < 0) continue;
	    
	    dev[j] = '\0';
	    if(k == 0) {		/* we saw no aliases */
	       sprintf(outline,"%s :\n",dev);
	    } else {
	       if(k == 2) (void)strncat(outline,") ",SIZE);
	       (void)strncat(outline,": ",SIZE);
	       (void)strncat(outline,dev,SIZE);
	       (void)strncat(outline,"\n",SIZE);
	    }
	    if(*pattern != '\0') {
	       if(match_pattern(outline,(char *)NULL,(char **)NULL) == NULL) {
		  continue;		/* doesn't match */
	       }
	    }
	    if(more(outline) < 0) {
	       (void)fclose(fil);
	       return(-1);
	    }
	 }
      }
      (void)fclose(fil);
   }
   return(0);
}
