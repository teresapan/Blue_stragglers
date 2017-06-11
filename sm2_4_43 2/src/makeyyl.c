#include "copyright.h"
/*
 * Syntax: av[0] inc_file input name
 *
 * This programme uses the file inc_file to construct a second stage lex
 * analyser called name() in file name.c.
 *
 * Name takes the output from input(), and recognises keywords for yacc.
 *
 * Input() partitions the input stream into integers (LEXINT),
 * floating point numbers (LEXFLOAT) and words (LEXWORD). A word
 * is any sequence of non-white characters which isn't a float
 * or integer. In addition, an end-of-input marker (ENDMARK) a
 * carriage return ('\n'), and the characters in ctokens[] are 
 * recognised.
 *
 * The behaviour of this lexicographic analyser is altered by the tokens
 * { and }. After a { has been recognised, all further words are returned as
 * WORD, and no further analysis is carried out until a } is recognised.
 *
 * It's behaviour is also altered by " ... ", which turn of the special
 * meaning of most characters
 */
#define STANDALONE			/* this is a stand-alone executable */
#include "options.h"
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "sm_declare.h"
#define NTOKEN 200		/* maximum number of tokens */
#define POUT fprintf(outfil	/* save space */
#define SIZE 40			/* maximum size of token */

static void sort();
static struct token_st {
   char name[SIZE];			/* name of token */
   int val_tok;				/* value of token */
} token[NTOKEN];

int
main(ac,av)
int ac;
char *av[];
{
   char line[100],
   	c,
	outfile[40];		/* name of file to write programme to */
   static char ctokens[] = "+-/(){}[]&|!=,?:%",
					/* single character tokens -- note
					   that you must also add new ones to
					   lexan.c. \n and sctokens are
					   treated specially*/
               sctokens[] = "*<>";	/* special ctokens */
   FILE  *infil,			/* file descriptor for *.h */
	 *outfil;			/* fd for av[3].c */
   int   i,j,
   	 i0,				/* saved value of i */
   	 max_val,			/* largest value found */
	 num_token,			/* number of tokens */
         seen_WORD = 0;			/* have we seen the token WORD yet? */

   if(ac < 4) {
      fprintf(stderr,"Syntax: makeyyl inc_file lexan yylex\n");
      exit(EXIT_BAD);
   }
   if((infil = fopen(av[1],"r")) == NULL) {
      fprintf(stderr,"Can't open %s\n",av[1]);
      exit(EXIT_BAD);
   }

   (void)sprintf(outfile,"%s.c",av[3]);
   if((outfil = fopen(outfile,"w")) == NULL) {
      (void)fprintf(stderr,"Can't open %s\n",outfile);
      (void)fclose(infil);
      exit(EXIT_BAD);
   }
/*
 * Read in the tokens from av[1].
 * Use the val_tok field to give their value
 *
 * There are varying amounts of junk at the start of bison/yacc files,
 * so try to skip it.
 */
   for(i = 0;i < NTOKEN && fgets(line,100,infil) != NULL;i++) {
      if(sscanf(line,"%*[^n]ne %s %d",token[i].name,&token[i].val_tok) != 2) {
	 if(seen_WORD) {
	    break;
	 } else {
	    i--;
	    continue;
	 }
      }
      if(seen_WORD || !strcmp(token[i].name,"WORD")) { /* first real define */
	 seen_WORD = 1;
      } else {
	 i--;
	 continue;
      }
      if(strcmp(token[i].name,"POWER_SYMBOL") == 0 ||
	 strcmp(token[i].name,"LEFT_SHIFT_SYMBOL") == 0 ||
	 strcmp(token[i].name,"RIGHT_SHIFT_SYMBOL") == 0) { /* special */
	 i--;
      }
   }
   (void)fclose(infil);
   if(i == 0) {
      fprintf(stderr,"I couldn't find any tokens\n");
      exit(EXIT_BAD);
   } else if(i == NTOKEN) {
      fprintf(stderr,"Only %d tokens allowed, others ignored\n",NTOKEN);
   }
   num_token = i;

   sort(token,num_token);       /* sort tokens */

   max_val = 0;
   for(i = 0;i < num_token;i++) {
      if(token[i].val_tok > max_val) {
	 max_val = token[i].val_tok;
      }
   }

   POUT,"#include \"copyright.h\"\n");
   POUT,"/*\n");
   POUT," * %s takes the output from %s(), and\n",av[3],av[2]);
   POUT," * recognises keywords for yacc.\n");
   POUT," *\n");
   POUT," * %s() partitions the input stream into integers\n",av[2]);
   POUT," * (LEXINT), floating point numbers (LEXFLOAT) and\n");
   POUT," * words (LEXWORD). A word is any sequence of non-white\n");
   POUT," * characters which isn't a float or integer.\n");
   POUT," * An end-of-input marker (ENDMARK), carriage return (\\n)\n");
   POUT," * and the characters %s are also recognised\n",ctokens);
   POUT," */\n");
   POUT,"#include \"options.h\"\n");
   POUT,"#include <stdio.h>\n");
   POUT,"#include <ctype.h>\n");
   POUT,"#include \"stack.h\"\n");
   POUT,"#include \"vectors.h\"\n");
   POUT,"#include \"yaccun.h\"\n");
   POUT,"#include \"%s\"\n",av[1]);
   POUT,"#include \"lex.h\"\n");
   POUT,"#include \"sm_declare.h\"\n");
   POUT,"\n");
   POUT,"extern int noexpand,	/* shall I recognise tokens ? */\n");
   POUT,"           sm_verbose;	/* Shall I be verbose ? */\n");
   POUT,"\n");
   POUT,"static char overload_tab[] = {\t/* Is token overloaded? */\n   ");
   for(i = 0;i <= max_val;i++) {
      POUT,"\'\\0\',%s",(i%10 == 9) ? "\n   " : " ");
   }
   if(i%10 != 0) POUT,"\n");
   POUT,"};\n");
   POUT,"static char nooverload_tab[] = {\t/* no overloaded tokens */\n   ");
   for(i = 0;i <= max_val;i++) {
      POUT,"\'\\0\',%s",(i%10 == 9) ? "\n   " : " ");
   }
   if(i%10 != 0) POUT,"\n");
   POUT,"};\n");
   POUT,"\n");

   POUT,"static char *overloaded = overload_tab;\n");
   POUT,"\n");
   POUT,"int\n");
   POUT,"#if defined(YYBISON)		/* a bison later than 1.14 */\n");
   POUT,"%s(yylvalp)\n",av[3]);
   POUT,"YYSTYPE *yylvalp;\n");
   POUT,"#else\n");
   POUT,"%s(yylvalp,yyllocp)\n",av[3]);
   POUT,"YYSTYPE *yylvalp;\n");
   POUT,"Void *yyllocp;			/* not used */\n");
   POUT,"#endif\n");
   POUT,"{\n");
   POUT,"  int ll;\n");
   POUT,"\n");
   POUT,"  switch (((ll = %s(yylvalp)) > 0) ? ll : ENDMARK) {\n",av[2]);
   POUT,"  case LEXINT :\n");
   POUT,"     if(sm_verbose > 3)\n");
   POUT,"        fprintf(stderr,\"%%d> INTEGER  %%d\\n\",noexpand,\n");
   POUT,"                                       yylvalp->intval);\n");
   POUT,"     save_int(yylvalp->intval);\n");
   POUT,"     return(INTEGER);\n");
   POUT,"  case LEXFLOAT :\n");
   POUT,"     if(sm_verbose > 3)\n");
   POUT,"        fprintf(stderr,\"%%d> FLOAT   %%g\\n\",noexpand,\n");
   POUT,"                                      yylvalp->floatval);\n");
   POUT,"     save_float(yylvalp->floatval);\n");
   POUT,"     return(_FLOAT);\n");
   POUT,"  case LEXWORD :\n");
   POUT,"     if(noexpand > 0) {	/* recognise no keywords */\n");
   POUT,"        if(noexpand == 1 &&\n");
   POUT,"                  !strcmp(yylvalp->charval,\"{\")) {\n");/*}*/
   POUT,"           if(sm_verbose > 3)\n");
   POUT,"              fprintf(stderr,\"%%d> WORD     '{'\\n\",noexpand);\n");
								   /*}*/
   POUT,"           else {;}\n");
   POUT,"           save_str(\"{\");\n");
   POUT,"           return('{');\n");			           /*}*/
   POUT,"        } else {\n");
   POUT,"           if(sm_verbose > 3)\n");
   POUT,"              if(!strcmp(yylvalp->charval,\"\\n\"))\n");
   POUT,"              fprintf(stderr,\"%%d> WORD     \\\\n\\n\",noexpand);\n");
   POUT,"              else\n");
   POUT,"              fprintf(stderr,\"%%d> WORD     %%s\\n\",noexpand,\n");
   POUT,"           		                          yylvalp->charval);\n");
   POUT,"           else {;}\n");
   POUT,"           save_str(yylvalp->charval);\n");
   POUT,"           return(WORD);\n");
   POUT,"        }\n");
   POUT,"     }\n");
   POUT,"\n");
   POUT,"     switch (yylvalp->charval[0]) {\n");
   POUT,"     case '\\n' :\n");		/* newline in special to printf */
   POUT,"        if(yylvalp->charval[1] == '\\0') {\n");
   POUT,"           if(sm_verbose > 3)\n");
   POUT,"              fprintf(stderr,\"%%d> '\\\\n'     \\\\n\\n\",noexpand);\n");
   POUT,"           else {;}\n");
   POUT,"           save_str(\"\\n\");\n");
   POUT,"           return('\\n');\n");
   POUT,"        }\n");
   POUT,"        break;\n");
/*
 * deal with * and **, < and <<, > and >>
 */
   for(i = 0; sctokens[i] != '\0'; i++) {
      char c = sctokens[i];
      POUT,"     case '%c' :\n", c);
      POUT,"        if(yylvalp->charval[1] == '\\0') {\n");
      POUT,"           if(sm_verbose > 3)\n");
      POUT,"              fprintf(stderr,\"%%d> '%c'      %c\\n\",noexpand);\n"
									,c, c);
      POUT,"           else {;}\n");
      POUT,"           save_str(\"%c\");\n", c);
      POUT,"           return('%c');\n", c);
      POUT,"        } else if(!strcmp(yylvalp->charval,\"%c%c\")) {\n", c, c);
      POUT,"           if(sm_verbose > 3)\n");
      POUT,"              fprintf(stderr,\"%%d> '%c%c'    %c%c\\n\",noexpand);\n",
								   c, c, c, c);
      POUT,"           else {;}\n");
      POUT,"           save_str(\"%c%c\");\n", c, c);
      POUT,"           return(%s_SYMBOL);\n",
 (c=='*' ? "POWER" : (c=='<' ? "LEFT_SHIFT" : (c=='>' ? "RIGHT_SHIFT" : ""))));
      POUT,"        }\n");
      POUT,"        break;\n");
   }

   for(i = 0;ctokens[i] != '\0';i++) {
      char cc[3];
      sprintf(cc,"%c",ctokens[i]);

      if(cc[0] == '%') {
	 cc[1] = '%'; cc[2] = '\0';
      }

      POUT,"     case '%c' :\n",ctokens[i]);
      POUT,"        if(yylvalp->charval[1] == '\\0') {\n");
      POUT,"           if(sm_verbose > 3)\n");
      POUT,"              fprintf(stderr,\"%%d> '%s'     %s\\n\",noexpand);\n",
									cc,cc);
      POUT,"           else {;}\n");
      POUT,"           save_str(\"%c\");\n",ctokens[i]);
      POUT,"           return('%c');\n",ctokens[i]);
      POUT,"        } else\n");
      POUT,"           break;\n");
   }

   for(i = 0,c = 'A';c <= 'Z' && i < num_token;c++) {    /* assumes ascii */
      i0 = i;				/* remember start of this letter */
      POUT,"     case '%c' :\n",c);	/* uppercase first */
      while(token[i].name[0] <= c && i < num_token) {
	 POUT,"        if(!strcmp(yylvalp->charval,\"%s\")) {\n",token[i].name);
	 POUT,"           if(sm_verbose > 3) {\n");
	 POUT,"              fprintf(stderr,\"%%d> %-8s %%s\\n\",\n",
							     token[i].name);
	 POUT,"                 		noexpand,yylvalp->charval);\n");
	 POUT,"           }\n");
	 POUT,"           save_str(yylvalp->charval);\n");
	 POUT,"           return(%d);\n",token[i].val_tok);
	 POUT,"       } else\n");
	 i++;
      }
      POUT,"\t\t\tbreak;\n");

      i = i0;				/* now lowercase */
      POUT,"     case '%c' :\n",tolower(c));
      while(token[i].name[0] <= c && i < num_token) {
	 for(j = 0;token[i].name[j] != '\0';j++) {
	    if(isupper(token[i].name[j])) {
	       token[i].name[j] = tolower(token[i].name[j]);
	    }
	 }
	 POUT,"        if(!overloaded[%d] && ",token[i].val_tok);
	 POUT,"!strcmp(yylvalp->charval,\"%s\")) {\n",token[i].name);
	 POUT,"           if(sm_verbose > 3) {\n");
	 POUT,"              fprintf(stderr,\"%%d> %-8s %%s\\n\",\n",
							     token[i].name);
	 POUT,"                 		noexpand,yylvalp->charval);\n");
	 POUT,"           }\n");
	 POUT,"           save_str(yylvalp->charval);\n");
	 POUT,"           return(%d);\n",token[i].val_tok);
	 POUT,"       } else\n");
	 i++;
      }
      POUT,"\t\t\tbreak;\n");
   }

   POUT,"     default : break;\n");
   POUT,"     }\n");
   POUT,"     save_str(yylvalp->charval);\n");
   POUT,"\n");
   POUT,"     if(sm_verbose > 3) {\n");
   POUT,"        fprintf(stderr,\"%%d> WORD     %%s\\n\",noexpand,\n");
   POUT,"                                       yylvalp->charval);\n");
   POUT,"     }\n");
   POUT,"     return(WORD);\n");
   POUT,"\n");
   POUT,"  case LEXSTRING:\n");
   POUT,"     save_str(yylvalp->charval);\n");
   POUT,"\n");
   POUT,"     if(sm_verbose > 3) {\n");
   POUT,"        fprintf(stderr,\"%%d> STRING     %%s\\n\",noexpand,\n");
   POUT,"                                       yylvalp->charval);\n");
   POUT,"     }\n");
   POUT,"     if(noexpand) {\n");
   POUT,"        return(WORD);\n");
   POUT,"     } else {\n");
   POUT,"        return(STRING);\n");
   POUT,"     }\n");
   POUT,"\n");
   POUT,"  case ENDMARK :\n");
   POUT,"     if(sm_verbose > 3)\n");
   POUT,"        fprintf(stderr,\"%%d> ENDMARK\\n\",noexpand);\n");
   POUT,"     else {;}\n");
   POUT,"     save_str(\"ENDMARK\");\n");
   POUT,"     return(ENDMARK);\n");
   POUT,"  default :\n");
   POUT,"     printf(\"bad code from lexan in %s\\n\");\n",av[3]);
   POUT,"     (void)strcpy(yylvalp->charval,\"unknown_code\");\n");
   POUT,"     save_str(yylvalp->charval);\n");
   POUT,"     return(WORD);\n");
   POUT,"  }\n");
   POUT,"}\n");
   POUT,"\n\n");
   POUT,"void\n");
   POUT,"allow_overload()\n");
   POUT,"{\n");
   POUT,"   overloaded = overload_tab;\n");
   POUT,"}\n");
   POUT,"\n\n");
   POUT,"void\n");
   POUT,"disable_overload()\n");
   POUT,"{\n");
   POUT,"   overloaded = nooverload_tab;\n");
   POUT,"}\n");
   POUT,"\n\n");
   POUT,"void\n");
   POUT,"overload(str,set)\n");
   POUT,"char *str;		/* symbol to overload */\n");
   POUT,"int set;		/* true to overload it */\n");
   POUT,"{\n");
   POUT,"   char *ptr;\n");
   POUT,"   int tok;\n");
   POUT,"   YYSTYPE lval;\n");

   POUT,"\n");
   POUT,"   for(ptr = str;islower(*ptr);ptr++) *ptr=toupper(*ptr);\n");
   POUT,"\n");
   POUT,"   push(str,S_NORMAL);\n");
   POUT,"   tok = %s(&lval,(Void *)NULL);\n",av[3]);
   POUT,"   unsave_str();			/* yylex will have saved it */\n");
   POUT,"\n");
   POUT,"   if(tok == WORD) {\n");
   POUT,"      msg_1s(\"You can't overload %%s -- it isn\'t special\\n\",str);\n");
   POUT,"      return;\n");
   POUT,"   }\n");
   POUT,"   if(tok < 0 || tok > %d) {\n",max_val);
   POUT,"      msg_1d(\"Illegal token to overload: %%d\\n\",tok);\n");
   POUT,"      return;\n");
   POUT,"   }\n");
   POUT,"\n");
   POUT,"   overloaded[tok] = (set) ? \'1\' : \'\\0\';\n");
   POUT,"}\n");

   (void)fclose(outfil);
   exit(EXIT_OK);
   /*NOTREACHED*/
}

/****************************************************/
/*
 * Sort a set of character strings, given the number of elements.
 *
 * The sort is a Shell sort, taken from Press et.al. Numerical Recipes.
 */
#define LN2I 1.442695022		/* 1/ln(e) */
#define TINY 1e-5

static void
sort(array,dimen)
struct token_st *array;			/* array to be sorted */
int dimen;				/* number of elements */
{
   int i,j,m,n,				/* counters */
       lognb2;				/* (int)(log_2(dimen)) */
   struct token_st temp;		/* temp value for sort */

   lognb2 = log((double)dimen)*LN2I + TINY;	/* ~ log_2(dimen) */
   m = dimen;
   for(n = 0;n < lognb2;n++) {
      m /= 2;
      for(j = m;j < dimen;j++) {
         i = j - m;
	 temp = array[j];
	 while(i >= 0 && strcmp(temp.name,array[i].name) < 0) {
	    array[i + m] = array[i];
	    i -= m;
	 }
	 array[i + m] = temp;
      }
   }
}
