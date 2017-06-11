/*
 * Extract the grammar from a set of yacc rules
 * Syntax: get_grammar [-t] [infile] [outfile]
 *
 * Options:
 *	t	write a file suitable for TeX
 *
 * If files are omitted, use standard in/output
 *
 * You are welcome to make any use you like of this code, providing that
 * my name remains on it, and that you don't make any money out of it.
 *
 *	Robert Lupton (lupton@astro.princeton.edu)
 */
#include <stdio.h>
#include <ctype.h>
#include "options.h"
#include "sm_declare.h"

#define ISSPECIAL(C) ((C) == '{' || (C) == '}' || (C) == '_' ||		      \
						      (C) == '&' || (C) == '%')
#define SIZE 82

static int TeX = 0;			/* produce a TeX file? */
static void line_out();

int
main(ac,av)
int ac;
char **av;
{
   char line[SIZE],
   	*lptr;				/* pointer to line */
   FILE *infil = stdin,
   	*outfil = stdout;
   int ctr = 0,				/* counter for {} */
       print_on = 1,			/* Should I print line? */
       rule;				/* is this line a rule? */
   
   if(ac >= 2 && av[1][0] == '-') {
      if(av[1][1] == 't') {
	 TeX = 1;
      } else {
	 fprintf(stderr,"Unknown flag %s\n",av[1]);
      }
      ac--;
      av++;
   }
   
   if(ac >= 2) {
      if((infil = fopen(av[1],"r")) == NULL) {
	 fprintf(stderr,"Can't open %s\n",av[1]);
	 exit(EXIT_BAD);
      }
   }
   if(ac >= 3) {
      if((outfil = fopen(av[2],"w")) == NULL) {
	 fprintf(stderr,"Can't open %s\n",av[2]);
	 fclose(infil);
	 exit(EXIT_BAD);
      }
   }

   if(TeX) {
      fprintf(outfil,"%\n% YACC grammar from %s\n%\n",(ac > 1)?av[1]:"?");
      fprintf(outfil,
	      "{\\tt\\obeylines\\obeyspaces\\parskip=0pt\\parindent=0pt\n");
      fprintf(outfil,"\\settabs\\+\\ \\ \\ \\ \\ \\ &\\cr\n");
   }

   while(fgets(line,SIZE,infil) != NULL) {
      rule = 0;
      if(line[0] == '%') {
	 if(line[1] == '{' || line[1] == '%') {
	    print_on = 0;
	 } else if(line[1] == '}') {
	    print_on = 1;
	 }
	 line_out(line,outfil);
	 continue;
      }
      if(print_on) {
	 line_out(line,outfil);
	 continue;
      }

      lptr = line;
      if(!isspace(line[0])) {		/* probably start of new rule */
	 while(*lptr != '\0' && !isspace(*lptr)) {	/* skip word */
	    lptr++;
	 }
	 while(isspace(*lptr)) lptr++;	/* skip white space */
	 if(*lptr == ':') {
	    rule = 1;
	 }
	 lptr = line;
      }

      while(isspace(*lptr)) lptr++;	/* skip white space */
      if(rule || *lptr == '|') {	/* a rule */
	 if(TeX) {
	    fprintf(outfil,"\\+");
	    if(isspace(line[0])) {
	       fprintf(outfil,"&");
	    }
	 }
	 if(!TeX) {			/* we don't want it for TeX */
	    lptr = line;
	 }
	 while(*lptr != '\0') {
	    while(*lptr != '\0') {
	       if(TeX) {
		  if(ISSPECIAL(*lptr)) {
		     putc('\\',outfil);
		  } else if(*lptr == '/' && *(lptr + 1) == '*') {
		     while(!(*++lptr == '*' && *(lptr + 1) == '/')) ;
		     if(*lptr == '*' && *(lptr + 1) == '/') lptr += 2;
		  }
		  if(*lptr == '\n') {
		     fprintf(outfil,"\\cr");
		  }
	       }
	       putc(*lptr,outfil);
	       if(*lptr == '{' && *(lptr - 1) != '\'') {
	   	  ctr = 0;		/* we'll see that { again */
		  break;
	       } else {
		  lptr++;
	       }
	    }
	    while(*lptr != '\0') {
	       if(*lptr == '}') {
		  if(--ctr == 0) {
		     break;
		  }
	       } else if(*lptr == '{') {
		  ctr++;
	       }
	       lptr++;
	    }
	 }
      } else if(*lptr == ';') {
	 line_out(line,outfil);
	 if(TeX) {
	    fprintf(outfil,"\\+\\cr");
	 }
	 putc('\n',outfil);
      }
   }

   if(TeX) {
      fprintf(outfil,"}\n");
   }

   fclose(infil); fclose(outfil);
   exit(EXIT_OK);
   /*NOTREACHED*/
}
	    
static void	 
line_out(line,fil)
char *line;
FILE *fil;
{
   if(line[0] == '\0' || !TeX) {
      fputs(line,fil);
      return;
   }

   fprintf(fil,"\\+");
   if(isspace(*line)) {
      fprintf(fil,"&");
      while(isspace(*++line)) ;
   }

   for(;*line != '\0';line++) {
      if(ISSPECIAL(*line)) {
	 putc('\\',fil);
      } else if(*line == '/' && *(line + 1) == '*') {
	 while(!(*++line == '*' && *(line + 1) == '/')) ;
	 if(*line == '*' && *(line + 1) == '/') line += 2;
      }
      if(*line == '\n') {
	 break;
      }
      putc(*line,fil);
   }
   fprintf(fil,"\\cr\n");
}
