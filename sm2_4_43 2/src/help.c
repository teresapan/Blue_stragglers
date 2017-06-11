#include "copyright.h"
/*
 * give help in response to HELP word.
 * if word is help, list all help available,
 * otherwise, print the file help_path/word, if it exists.
 * If it doesn't return -1
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "sm_declare.h"
#define NAMESIZE 40			/* max length of a filename */
#define NDIRS 3				/* max. number of help directories */
#define SIZE 80

typedef struct {			/* structure for directory listings */
   int i;				/* which element of search path */
   char name[NAMESIZE];			/* name of file */
} HELP_FILE;

static char *help_path[NDIRS + 1];	/* list of directories of help files */
static HELP_FILE **read_directory();	/* list of all help files */
extern int sm_interrupt,
  	   sm_verbose;
static int print_help_file();

/*****************************************************************************/

void
set_help_path()
{
   int i;
   char *ptr;
   
   if((ptr = get_val("help")) == NULL) {	/* not defined in .sm */
      msg("Help directory is not defined\n");
      ptr = "";
   }
   if((help_path[0] = malloc(strlen(ptr) + 1)) == NULL) {
      msg("Can't allocate storage for help_path\n");
      help_path[0] = "";
   } else {
      strcpy(help_path[0],ptr);
   }
   
   ptr = help_path[0];
   for(i = 0;i < NDIRS && *ptr != '\0';i++) {
      help_path[i] = ptr;
      while(*ptr != '\0') {
	 if(isspace(*ptr)) {
	    *ptr++ = '\0';
	    while(isspace(*ptr)) ptr++;
	    break;
	 } else {
	    ptr++;
	 }
      }
   }
   help_path[i] = NULL;
}

/*****************************************************************************/
/*
 * Search the help directories for WORD; if it is found return 1, if we
 * want to stop searching, return -1, else 0.
 */
int
help(word)
char  word[];
{
   char  buff[SIZE + 1];
   HELP_FILE **files;
   int i;
   int nfile;

   (void)more((char *)NULL);			/* initialise more */

   if(word[0] == '.')			/* don't print current directory */
      return(0);
   if(!strcmp(word,"help")) {
      more("Syntax: HELP word,\n");
      more("where word is a macro, variable, vector, or one of the following:\n");
      if((files = read_directory(&nfile)) != NULL) {
	 char line[82];
	 int nlines;			/* number of lines of output */

	 nlines = nfile/5 + (nfile%5 == 0 ? 0 : 1);
	 for(i = 0;!sm_interrupt && i < nlines;i++) {
	    sprintf(line,"%-15s %-15s %-15s %-15s %-15s\n",
		    i >= nfile ? "" : files[i]->name,
		    nlines + i >= nfile ? "" : files[nlines + i]->name,
		    2*nlines + i >= nfile ? "" : files[2*nlines + i]->name,
		    3*nlines + i >= nfile ? "" : files[3*nlines + i]->name,
		    4*nlines + i >= nfile ? "" : files[4*nlines + i]->name);
	    if(more(line) < 0) break;
	 }
	 files--;			/* we ++'d files in read_directory */
	 free((char *)files[0]);
	 free((char *)files);
	 if(i < nlines) return(-1);
      } else {
	 for(i = 0;help_path[i] != NULL;i++) {
#ifdef vms
	    (void)sprintf(buff,"dir/notrail/version=1 %s*..",help_path[i]);
#else
	    (void)sprintf(buff,"ls %s",help_path[i]);
#endif
	    (void)system(buff);
	 }
      }
      return(print_help_file(word));
   }
/*
 * We've dealt with the special case `help help', now look for a file named
 * `word' on the search path
 */
   return(print_help_file(word));
}

/*****************************************************************************/

static int
print_help_file(file)
char *file;
{
   char  buff[SIZE + 1];
   FILE  *fil;
   int i, j, jstart;

   for(i = 0;help_path[i] != NULL;i++) {
      (void)sprintf(buff,"%s%s",help_path[i],file);

      if((fil = fopen(buff,"r")) != NULL) {
	 while(fgets(buff,SIZE,fil) != NULL) {
	    if(more(buff) < 0) {
	       (void)fclose(fil);
	       return(-1);
	    }
	 }
	 (void)fclose(fil);
	 return(1);
      }
   }
/*
 * Not found it; look to see if it's in an index file somewhere (the
 * equivalent of man's .so capability). Index files consist of a number
 * of lines, each with two words; the first is the extra help entry being
 * declared, the second where to look it up (so "foo bar" declares that
 * "HELP foo" should be equivalent to "HELP bar"). Comments start at #.
 */
   for(i = 0;help_path[i] != NULL;i++) {
      (void)sprintf(buff,"%s%s",help_path[i],"help.ind");

      if((fil = fopen(buff,"r")) != NULL) {
	 while(fgets(buff,SIZE,fil) != NULL) {
	    for(j = 0;buff[j] != '\0' && !isspace(buff[j]);j++) continue;

	    if(j == 0 || buff[j-1] == '#') {	/* a blank or comment line */
	       continue;
	    }

	    if(strncmp(file,buff,j) != 0 || file[j] != '\0') {
	       continue;
	    }
	    while(isspace(buff[j])) j++;
	    jstart = j;
	    while(buff[j] != '\0' && !isspace(buff[j])) j++;
	    buff[j] = '\0';

	    if(print_help_file(&buff[jstart]) != 0) { /* got it */
	       (void)fclose(fil);
	       return(1);
	    }
	 }
	 (void)fclose(fil);
      }
   }

   return(0);
}

/**************************************************************/
/*
 * Look for a string in the help files
 */
#define NCONTEXT 5			/* number of lines of context */
#define LSIZE 80			/* length of a line */

#define RETURN				/* cleanup and return */	\
  if(fil != NULL) fclose(fil);						\
  files--;				/* we ++'d in read_directory */ \
  free((char *)files[0]);						\
  free((char *)files);							\
  return;

void
help_apropos(str)
char *str;
{
   char buff[LSIZE + 1];
   FILE *fil = NULL;
   HELP_FILE **files;			/* names of help files */
   int i,j;
   char lines[NCONTEXT][LSIZE + 1];	/* lines that we've read */
   int lnum;				/* current line number */
   int lnum_last;			/* last lnum printed */
   int ncontext;			/* number of lines of context */
   int nfile;				/* count the help files */
   int printed_filename;		/* have we printed the filename yet? */

   ncontext = (sm_verbose == 0) ? 1 : NCONTEXT;
   
   if((files = read_directory(&nfile)) == NULL) {
      return;
   }

   (void)match_pattern("",str,(char **)NULL);
   (void)more((char *)NULL);

   for(j = 0;!sm_interrupt && files[j] != NULL;j++) {
      sprintf(buff,"%s%s",help_path[files[j]->i],files[j]->name);
      if((fil = fopen(buff,"r")) == NULL) {
	 msg_2s("Can't open %s%s\n",help_path[files[j]->i],files[j]->name);
	 continue;
      }

      for(i = 0;i < ncontext;i++) {
	 lines[i][0] = '\0';
      }
      printed_filename = 0;
      lnum_last = -ncontext;
      for(lnum = 0;fgets(lines[lnum%ncontext],LSIZE,fil) != NULL;lnum++) {
	 if(match_pattern(lines[lnum%ncontext],(char *)NULL,(char **)NULL)
	    							     != NULL) {
	    if((i = lnum - ncontext/2) < 0) {
	       i = 0;
	    } else if(i <= lnum_last) {
	       i = lnum_last + 1;
	    }
	    if(!printed_filename) {
	       printed_filename = 1;
	       if(sm_verbose == 0) {
		  sprintf(buff,"%s:\n",files[j]->name);
	       } else {
		  sprintf(buff,"\n\t\t\t*** %s ***\n\n",files[j]->name);
	       }
	       if(more(buff) < 0) {
		  RETURN;
	       }
	    } else if(i > lnum_last + 1) {
	       if(sm_verbose > 0) {
		  sprintf(buff,"------------\n");
		  if(more(buff) < 0) {
		     RETURN;
		  }
	       }
	    }
	    lnum_last = lnum + ncontext/2;

	    for(;i <= lnum;i++) {
	       if(more(lines[i%ncontext]) < 0) {
		  RETURN;
	       }
	    }
	    for(lnum++;lnum <= lnum_last &&
			fgets(lines[lnum%ncontext],LSIZE,fil) != NULL;lnum++) {
	       if(more(lines[lnum%ncontext]) < 0) {
		  RETURN;
	       }
	       if(match_pattern(lines[lnum%ncontext],(char *)NULL,
						      (char **)NULL) != NULL) {
		  lnum_last = lnum + ncontext/2;
	       }
	    }
	    lnum--;			/* ++'ed at both ends of last loop */
	 }
      }
      fclose(fil); fil = NULL;
   }
   RETURN;
}

/*******************************************************************/
/*
 * read_directory() returns an array of character pointers, of which
 * the first is NOT used; it's there purely to preserve the address
 * that malloc() returned. The last pointer is NULL.
 */
#if defined(HAVE_READDIR)
#include <sys/types.h>
#include <dirent.h>

static int compar();

static HELP_FILE **
read_directory(nfile)
int *nfile;				/* count the help files */
{
   char *current;			/* the current name */
   DIR *dptr = NULL;			/* pointer for traversing directory  */
   struct dirent *entry;		/* an entry in the directory */
   int i,j;
   HELP_FILE **names;			/* names of help files */
   HELP_FILE *names_s;			/* storage for names */
   char *ptr;
   int size_names;			/* number of help files to search */

   *nfile = 0;
   size_names = 100;
   if((names = (HELP_FILE **)malloc(size_names*sizeof(HELP_FILE *))) == NULL) {
      fprintf(stderr,"Can't allocate storage for names in read_directory\n");
      return(NULL);
   }
   if((names_s = (HELP_FILE *)malloc(size_names*sizeof(HELP_FILE))) == NULL) {
      fprintf(stderr,"Can't allocate storage for names_s\n");
      free((char *)names);
      return(NULL);
   }
   for(j = 0;j < size_names;j++) {
      names[j] = names_s + j;
   }

   *nfile = 1;				/* keep the first entry for free() */
   for(i = 0;help_path[i] != NULL;i++) {
      if((dptr = opendir(help_path[i])) == NULL) {
	 continue;
      }
      
      while(!sm_interrupt && (entry = readdir(dptr)) != NULL) {
	 if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) {
	    continue;
	 }
	 if((ptr = strrchr(entry->d_name,'~')) != NULL && *(ptr + 1) == '\0') {
	    continue;
	 }
	 names[*nfile]->i = i;
	 strncpy(names[(*nfile)++]->name,entry->d_name,NAMESIZE);
	 if(*nfile >= size_names) {
	    free((char *)names);
	    size_names *= 2;
	    if((names_s = (HELP_FILE *)realloc(names_s,
				      size_names*sizeof(HELP_FILE))) == NULL) {
	       msg("Can't reallocate storage for names_s\n");
	       return(NULL);
	    }
	    if((names = (HELP_FILE **)malloc(
				    size_names*sizeof(HELP_FILE *))) == NULL) {
	       msg("Can't allocate storage for names in read_directory\n");
	       free(names_s);
	       return(NULL);
	    }
	    for(j = 0;j < size_names;j++) {
	       names[j] = names_s + j;
	    }
	 }
      }
      closedir(dptr);
   }

   (*nfile)--;				/* first entry isn't really used */
   names++;				/*   "     "     "     "      "  */
   qsort((Void *)names,*nfile,sizeof(HELP_FILE *),compar);

   current = "";
   for(i = j = 0;j < *nfile;j++) {	/* reject duplicates */
      if(strcmp(current,names[j]->name) != 0) {
	 names[i] = names[j];
	 current = names[i++]->name;
      } else {
	 if(names[i]->i > names[j]->i) names[i]->i = names[j]->i;
      }
   }
   *nfile = (j == 0 ? 0 : i);
   names[*nfile] = NULL;

   return(names);
}

static int
compar(a,b)
Void *a,*b;
{
   return(strcmp((*(HELP_FILE **)a)->name,(*(HELP_FILE **)b)->name));
}

#else
static HELP_FILE **
read_directory(nfile)
int *nfile;
{
   *nfile = 0;
   return(NULL);
}
#endif
