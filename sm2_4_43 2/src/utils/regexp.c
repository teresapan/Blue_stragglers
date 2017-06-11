#include "copyright.h"
/*
 * Return a match for a Regular Expression.
 *
 * A subset of the usual Unix regular expressions are understood:
 *
 *	.	Matches any character except \n
 *	[...]	Matches any of the characters listed in ... .
 *		  If the first character is a ^ it means use anything except
 *		the range specified (in any other position ^ isn't special)
 *		  A range may be specified with a - (e.g. [0-9]), and if a ]
 *		is to be part of the range, it must appear first (or after
 *		the leading ^, if specified). A - may appear as the [---].
 *	^	Matches only at the start of the string
 *	$	Matches only at the end of a string
 *	\t	Tab
 *	\n	Newline
 *	\.	(any other character) simply escapes the character (e.g. \^)
 *	*	Any number of the preceeding character (or range)
 *	?	One or more of the preceeding character or range
 *
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "sm_declare.h"

#define START '\001'			/* special characters */
#define END   '\002'
#define DOT   '\003'
#define STAR  '\004'
#define QUERY '\005'
#define RANGE '\006'

#define MATCH(P,C,R)			/* Does P match C, for range R? */\
   (((P) == RANGE && (R)[(int)C]) ||					\
    ((P) == DOT && (C) != '\n') ||					\
    (P) == (C))
#define RANGE_SIZE 128			/* number of chars allowed in ranges */

static char cpattern[50],		/* compiled pattern */
	    *_match_pattern(),
	    *s_range = NULL;		/* storage for range of characters */
static int have_pattern = 0,		/* do we have a compiled pattern? */
  	   case_fold_search = 0,	/* case insensitive? */
	   compile_pattern();
static unsigned int nrange = 0;		/* number of range's allocated */

/******************************************************************/
/*
 * Match a pattern, the beginning of the match is returned, or NULL.
 * If the pattern is given as NULL the same pattern as last time will
 * be used.
 * If end is not NULL it serves two purposes:
 * 1/ On entry, it marks the end of the region to search (specifically,
 *    it marks the last point where a match can start; matches can
 *    extend past end).
 * 2/ On return it will point to the end of a succesfull match, or the
 *    start of the string if unsuccessful.
 *
 * Note that this code is NOT reentrant, and that only one call can be
 * active at a time.
 */
char *
match_pattern(str,pattern,end)
char *str,				/* string to search */
     *pattern,				/* pattern to look for */
     **end;				/* end of pattern, if matched */
{
   char *eend;

   if(pattern != NULL) {
      if(compile_pattern(pattern) < 0) {
	 return(NULL);
      }
      if(*str == '\0') {
	 return("");			/* allow setting/checking of pattern */
      }
   } else if(!have_pattern) {
      (void)fprintf(stderr,"There is no current pattern to match\n");
      return(NULL);
   }

   if(end == NULL) {			/* provide storage for end if needed */
      end = &eend;
      *end = NULL;
   }

   if(cpattern[0] == START) {
      return(_match_pattern(str,&cpattern[1],(char *)NULL,end,1));
   } else {
      return(_match_pattern(str,cpattern,(char *)NULL,end,0));
   }
}

/*******************************************************/
/*
 * Return start of match (already compiled). This function IS reentrant,
 * unlike match_pattern
 */
static char *
_match_pattern(str,cpatt,range0,end,match_at_start)
char *str,				/* string to search */
     *cpatt,				/* compiled pattern */
     *range0,				/* current range */
     **end;				/* end of match */
int match_at_start;			/* match must be at start of string? */
{
   char c,
   	*mptr,				/* where the current match has got to*/
        *next_range = NULL,		/* range for next RANGE in pattern
					   (initialised to NULL to placate
					   flow-analysing compilers) */
   	*range,				/* current range */
        *range1,			/* a spare range */
        *pptr,				/* where we are in pattern */
        *sptr;				/* where the current match starts */

   for(sptr = str;*sptr != '\0' && (*end == NULL || sptr < *end);sptr++) {
      for(mptr = sptr, pptr = cpatt, range = range0;;mptr++, pptr++) {
	 if(*mptr == '\0') {
	    if(*pptr == END || *pptr == '\0') {
	       *end = mptr;
	       return(sptr);
	    } else {
	       *end = str;
	       return(NULL);
	    }
	 }

	 switch(*pptr) {
	  case '\0':			/* reached end of pattern -- Success */
	    if(sptr == mptr) {		/* trivial match */
	       goto no_match;
	    }
	    *end = mptr;
	    return(sptr);
	  case END:
	    goto no_match;
	  case QUERY:
	  case STAR:
	  case START:
	    (void)fprintf(stderr,"You can't get here! (In _match_pattern)\n");
	    break;
	  case RANGE:
	    if(range == NULL) {
	       range = (range0 == NULL) ? s_range : range;
	    } else {
	       range += RANGE_SIZE;
	    }				/* fall through to default */
	  case DOT:			/* fall through to default */
	  default:
	    if(*(pptr + 1) == QUERY || *(pptr + 1) == STAR) {
	       int nmatch;
	       
	       if(*pptr == RANGE) {	/* we may need start it again */
		  range1 = (range == s_range) ? NULL : range - RANGE_SIZE;
	       } else {
		  range1 = range;
	       }
	       if(*(pptr + 2) == RANGE) {
		  if(range == NULL) {
		     next_range = (range0 == NULL) ? s_range : range;
		  } else {
		     next_range = range + RANGE_SIZE;
		  }
	       }

	       for(nmatch = 0;*mptr != '\0';mptr++,nmatch++) {
		  c = (case_fold_search && isupper(*mptr)) ?
		    				      tolower(*mptr) : (*mptr);
		  if(!MATCH(*pptr,c,range)) {
		     break;
		  }
		  if(MATCH(*(pptr + 2),c,next_range)) {
		     char *end1 = NULL,*end2 = NULL,
		          *start1,*start2;
		     
		     start1 = _match_pattern(mptr + 1,pptr,range1,&end1,1);
		     start2 = _match_pattern(mptr,pptr + 2,range,&end2,1);
		     if(start1 == NULL) {
			if(start2 == NULL) {
			   goto no_match;
			} else {
			   if(*(pptr + 1) == QUERY && nmatch == 0)  {
			      goto no_match;
			   }
			   *end = end2;
			   return(sptr);
			}
		     } else if(start2 == NULL) {
			*end = end1;
			return(sptr);
		     } else {		/* neither failed */
			*end = end1 > end2 ? end1 : end2;
			return(sptr);
		     }
		  }
	       }
	       mptr--;
	       if(*++pptr == QUERY && nmatch == 0)  {
		  goto no_match;
	       }
	    } else {
	       c = (case_fold_search && isupper(*mptr)) ?
		    				      tolower(*mptr) : (*mptr);
	       if(!MATCH(*pptr,c,range)) {
		  goto no_match;
	       }
	    }
	    break;
	 }
      }
      no_match: ;
      if(match_at_start) break;
   }
   *end = str;
   return(NULL);
}

/******************************************************************/
/*
 * Convert a string into an internal format, ready for pattern matching
 */
static int
compile_pattern(pattern)
char *pattern;
{
   char c1,c2,
        *cptr,				/* pointer to cpattern */
        *range = NULL;
   int i,
       range_value;			/* 0 or 1, for [^...] or [...] */

   case_fold_search = (*print_var("case_fold_search") != '\0');
   
   for(cptr = cpattern;cptr - cpattern < sizeof(cpattern) - 1;
       							cptr++,pattern++) {
      switch(*pattern) {
       case '\0':
	 *cptr = '\0';
	 have_pattern = 1;
	 return(0);
       case '^':
	 if(cptr != cpattern) {
	    (void)fprintf(stderr,
			  "A ^ can only appear at the start of a pattern\n");
	    have_pattern = 0;
	    return(-1);
	 }
	 *cptr = START;
	 break;
       case '$':
	 if(*(pattern + 1) != '\0') {
	    (void)fprintf(stderr,
			  "A $ can only appear at the end of a pattern\n");
	    have_pattern = 0;
	    return(-1);
	 }
	 *cptr = END;
	 break;
       case '.':
	 *cptr = DOT;
	 break;
       case '*':
       case '?':
	 if(cptr == cpattern ||
			(cptr == cpattern + 1 && cpattern[0] == START)) {
	    (void)fprintf(stderr,
		"%c cannot be the first character in a pattern\n",*pattern);
	    have_pattern = 0;
	    return(-1);
	 }
	 *cptr = *pattern == '*' ? STAR : QUERY;
	 break;
       case '\\':
	 if(*++pattern == '\0') {
	    (void)fprintf(stderr,
			  "A \\ cannot be the last character in a pattern\n");
	    have_pattern = 0;
	    return(-1);
	 }
	 if(*pattern == 'n') {
	    *cptr = '\n';
	 } else if(*pattern == 't') {
	    *cptr = '\t';
	 } else {
	    *cptr = *pattern;
	 }
	 break;
       case '[':
	 if(s_range == NULL) {
	    nrange = 5;
	    if((s_range = malloc(RANGE_SIZE*nrange)) == NULL) {
	       (void)fprintf(stderr,"Can't allocate storage for range\n");
	       have_pattern = 0;
	       return(-1);
	    }
	    range = s_range;
	 } else {
	    if(range == NULL) {
	       range = s_range;
	    } else {
	       range += RANGE_SIZE;
	       if(range >= s_range + RANGE_SIZE*nrange) {
		  nrange *= 2;
		  if((s_range = realloc(s_range,RANGE_SIZE*nrange)) == NULL) {
		     (void)fprintf(stderr,
				   "Can't reallocate storage for s_range\n");
		     have_pattern = 0;
		     return(-1);
		  }
		  range = s_range + RANGE_SIZE*(nrange/2);
	       }
	    }
	 }
	 *cptr = RANGE;

	 if(*++pattern == '^') {
	    range_value = 0;
	    pattern++;
	 } else {
	    range_value = 1;
	 }
	 for(i = 0;i < RANGE_SIZE;i++) {
	    range[i] = !range_value;
	 }

	 if(*pattern == ']') {		/* special case: initial ']' */
	    range[']'] = range_value;
	    pattern++;
	 }

	 for(;;pattern++) {
	    if(*pattern == '\0') {
	       (void)fprintf(stderr,"Unclosed range in pattern\n");
	       have_pattern = 0;
	       return(-1);
	    } else if(*pattern == ']') {
	       break;
	    } else if(*pattern == '\\') {
	       if(*++pattern == 'n') {
		  c1 = '\n';
	       } else if(*pattern == 't') {
		  c1 = '\t';
	       } else {
		  c1 = *pattern;
	       }
	    } else {
	       c1 = *pattern;
	    }
	    if(*(pattern + 1) == '-') {
	       pattern += 2;
	       if(*pattern == '\0') {
		  (void)fprintf(stderr,"Unclosed range in pattern\n");
		  have_pattern = 0;
		  return(-1);
	       } else if(*pattern == '-' && *(pattern + 1) == '-') {
		  range[(int)c1] = range_value;
		  if(case_fold_search && isupper(c1)) {
		     range[tolower(c1)] = range_value;
		  }

		  range['-'] = range_value; /* [---] is special */
		  pattern++;
		  continue;
	       }

	       if(*pattern == '\\') {
		  if(*++pattern == 'n') {
		     c2 = '\n';
		  } else if(*pattern == 't') {
		     c2 = '\t';
		  } else {
		     c2 = *pattern;
		  }
	       } else {
		  c2 = *pattern;
	       }
	       for(i = c1;i <= c2;i++) {
		  range[i] = range_value;
		  if(case_fold_search && isupper(i)) {
		     range[tolower(i)] = range_value;
		  }
	       }
	    } else {
	       range[(int)c1] = range_value;
	       if(case_fold_search && isupper(c1)) {
		  range[tolower(c1)] = range_value;
	       }
	    }
	 }
	 break;
       default:
	 c1 = *pattern;
	 *cptr = (case_fold_search && isupper(c1)) ? tolower(c1) : c1;
	 break;
      }
   }
   have_pattern = 1;
   return(0);
}
