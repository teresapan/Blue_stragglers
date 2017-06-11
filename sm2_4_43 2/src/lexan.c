#include "copyright.h"
/*
 * This programme parses an input stream into integers, floats, words
 * (defined as anything else), and strings in ''. Tokens are separated by
 * white space, taken to be ' ', tab or linefeed, or by !, ., :, ?, {, },
 * (, ), a comma, or an arithmetical operator.
 * All of these (except ' ' and tab) are returned as tokens.
 * The symbols { and } are used to bracket strings of tokens that are not to
 * be expanded in any way.
 *
 * The classification of strings is carried out by num_or_word()
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "vectors.h"
#include "yaccun.h"
#include "lex.h"
#include "sm_declare.h"

#define IF_TOKEN	/* see if we have just terminated a token */	\
   if(bptr != yylvalp->charval) {           /* something to parse */	\
      *bptr = '\0';                       /* null terminate symbol */	\
      (void)unput(c);                     /* return that white space */	\
      if(in_quote || noexpand > 0) {					\
	 return(LEXWORD);						\
      } else {								\
	 return(parse(yylvalp));	/* and parse it */		\
      }									\
   }

extern int in_quote,		/* We are within a "string" */
	   sm_interrupt,		/* respond to ^C */
	   noexpand;		/* variable to control recognition of keywords
				   and expansion of variables */
static int parse();

int
lexan(yylvalp)
YYSTYPE *yylvalp;
{
   char *bptr_max = yylvalp->charval + CHARMAX - 2; /* 2: \ and '\0' */
   register char  *bptr = yylvalp->charval;
   register int   c;

   while((c = input()) != EOF) {
      switch (c) {		/* check for token-terminating symbols */
       case ' ':
       case '\t':
	 if(in_quote) {
	    *bptr++ = c;
	 } else {
	    IF_TOKEN;
	 }
	 break;
       case '\n':
       case '\r':
	 IF_TOKEN;
	 (void)strcpy(yylvalp->charval,"\n");
	 return(LEXWORD);
       case '{':
	 if(in_quote) {
	    *bptr++ = c;
	    break;
	 }
	 IF_TOKEN;
	 noexpand++;				/* turn off expansion */
	 (void)strcpy(yylvalp->charval,"{");
	 return(LEXWORD);
       case '}':
	 if(in_quote) {
	    *bptr++ = c;
	    break;
	 }
	 IF_TOKEN;
	 noexpand--;				/* turn on expansion */
	 if(noexpand < 0) {			/* check for matching { */
	    msg("Unbalanced }\n");
	    noexpand = 0;
	 }
	 (void)strcpy(yylvalp->charval,"}");
	 return(LEXWORD);
       case ',':
       case '=':
       case '+':
       case '-':
       case '/':
       case '(':
       case ')':
       case '[':
       case ']':
       case '!':
       case ':':
       case '?':
       case '&':
       case '|':
       case '%':
         if(noexpand || in_quote) {		/* don't terminate token */
	    *bptr++ = c;
	    break;
	 } else {
	    IF_TOKEN;
	    (void)sprintf(yylvalp->charval,"%c",c);
	    return(LEXWORD);
	 }
       case '*':			/* check for special cases **,<<,>> */
       case '<':
       case '>':
         if(noexpand || in_quote) {	/* don't terminate token */
	    *bptr++ = c;
	    break;
	 } else {
	    IF_TOKEN;
	    if(sm_peek() == c) {
	       (void)input();
	       (void)sprintf(yylvalp->charval,"%c%c", c, c);
	    } else {
	       (void)sprintf(yylvalp->charval,"%c",c);
	    }
	    return(LEXWORD);
	 }
       case '\'':			/* start of a string */
	 if(bptr == yylvalp->charval) {	/* starting a new charval */
	    *bptr++ = '\'';
	    while((c = input()) != EOF) {
	       if(sm_interrupt || c == '\'') { /* end of string */
		  *bptr++ = '\'';
		  *bptr = '\0';
		  return(LEXSTRING);
	       } else if(c == '\\') {
		  *bptr++ = c;
		  if((c = input()) == EOF) break;
	       }
	       *bptr++ = c;
	       if(bptr >= bptr_max) {
		  msg_1s("String is too long in lexan: %s",yylvalp->charval);
		  yyerror("show where");
		  *bptr = '\0';
		  return(LEXWORD);
	       }
	    }
	 } else {
	    *bptr++ = '\'';
	 }
	 break;
       default:
	 *bptr++ = c;
	 break;
      }
      if(bptr >= bptr_max) {
	 msg_1s("Token is too long in lexan: %s",yylvalp->charval);
	 yyerror("show where");
	 *bptr = '\0';
	 return(LEXWORD);
      }
   }
   return(ENDMARK);
}

/***********************************************/
/*
 * parse a character string into LEXWORDs, LEXINTs and LEXFLOATs
 */
static int
parse(yylvalp)
YYSTYPE *yylvalp;
{
   char *bptr,			/* pointer to buff */
	buff[CHARMAX],		/* buffer to accumulate float from part */
        *num;
   int c,
       i;

   num = yylvalp->charval;
   switch(i = num_or_word(num)) {
   case 0:				/* WORD */
      return(LEXWORD);
   case 1:				/* INTEGER */
      yylvalp->intval = atoi2(num, 0, NULL);
      return(LEXINT);
   case 2:				/* FLOAT */
      yylvalp->floatval = atof2(num);
      return(LEXFLOAT);
   case 3:				/* part of float */
      if((c = input()) == '-' || c == '+') {	/* maybe a float */
	 if(isdigit(sm_peek())) {		/* we have a true float */
	    (void)sprintf(buff,"%s%c",num,c);
	    bptr = buff + strlen(buff);
	    while(isdigit(c = input())) {	/* read exponent */
	       *bptr++ = c;
	    }
	    (void)unput(c);			/* we read one to far */
	    *bptr = '\0';
	    if(noexpand > 0) {
	       (void)strcpy(yylvalp->charval,buff);
	       return(LEXWORD);
	    } else {
	       yylvalp->floatval = atof2(buff);
	       return(LEXFLOAT);
	    }
	 }
      }				/* just a part of a float, so return WORD */
      (void)unput(c);		/* return +- to buffer */
      (void)strcpy(yylvalp->charval,num);
      return(LEXWORD);
   default:
      msg_1d("Illegal return %d from num_or_word in parse\n",i);
      break;
   }
   return(-1);
}
