#include "copyright.h"
/*
 * This file contains routines to read data form a file. The i/o is
 * line orientated, to mimic our favourite language FORTRAN. Each field
 * is read as a string, then converted to a float. This allows SM
 * to recognise a field beginning with a * as an empty field, and return it as
 * NO_VALUE
 *
 * read_column(file,cols,vectors,nvec,l1,l2)
 *					reads data in nvec columns cols of file
 *					between lines l1 and l2, into vectors
 * read_line(file,l1,i)			returns a pointer to word i of line l1
 *					of file, (or to the line if i <= 0)
 * read_row(file,row,vector)		reads data in row row of file file
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "charset.h"
#include "vectors.h"
#include "sm.h"
#include "sm_declare.h"

#ifdef vms            /*  vms include files for testing the record  */
#include <fab.h>      /* attributes of the file                     */
#include <stat.h>
#endif

#define DSPACE 100			/* initial size of vector->vec.f */
#define LINESIZE 10000			/* maximum size of line to read */
#define NFMT_VECS 10			/* max. number of vecs if fmt != NULL*/
  
#define UNDEFINED  -3			/*  values for static external */
#define FORTRAN_FILE 1			/*  fortran_file, which checks */
#define PROPER_FILE 2			/*  for files written by fortran */

#define FCLOSE(FILE_PTR)						\
   (void)fclose(FILE_PTR);						\
   fortran_file = UNDEFINED;

extern char cchar;			/* comment character to use */
#ifdef vms
static int check_for_fortran_file();
static char *fgets_cr();
#endif
extern int sm_interrupt,			/* has the user hit ^C? */
	   sm_verbose;			/* shall I be chatty? */
static int check_format();
static int compile_format();
static int scan_line P_ARGS(( char *, char *, VECTOR **, int, int, int ));
static int line_ctr = -1;		/* line counter in file */
static int use_seek = -1;		/* seek to old position is possible? */
static int fortran_file = UNDEFINED;
static char *sm_getline P_ARGS(( char *, int, FILE * ));
static char *sm_getline_phys P_ARGS(( char *, int, FILE * ));
static void set_failed P_ARGS(( char *, int, VECTOR **, int, int, int ));
static FILE *fopen_seek P_ARGS(( char *, char *, int ));

int
read_column(readall,data_file,col,vector,nvec,l1,l2,format)
int readall;				/* read all the lines requested, even
					   if some have missing columns */
char data_file[];			/* file to read from */
int col[];				/* which columns to read */
VECTOR *vector[];			/* vectors to read into */
int nvec,				/* number of vectors to read */
    l1,l2;				/* range of lines to use */
char *format;				/* format to use; may be "" */
{
   char line[LINESIZE],			/* buffer for one line of file */
   	*lptr,				/* pointer to line */
	*sptr;				/* pointer to start of field */
   FILE *fil;				/* descriptor for data_file */
   double value;				/* value to read */
   int c,				/* column counter in line */
       i,j,k,
       n_elem = 0,			/* number of elements read */
       nfmt,				/* number of formats in format string*/
       warned;				/* we've warned about this line */
   unsigned size;			/* current size of vector->vec.f */
   VECTOR *vtemp;			/* used in sorting */

#ifdef vms
   check_for_fortran_file( data_file );
#else
   if(fortran_file != UNDEFINED) {	/* make compilers happy by
					   using fortran_file */
      msg("Impossible condition in read_column\n");
   }
#endif

   if(*format == '\0') {
/*
 * Sort col and vector arrays.
 * The sort is a straight insertion, fine for a few columns
 */
      for(i = 1;i < nvec;i++) {
	 k = col[i];
	 vtemp = vector[i];
	 j = i - 1;
	 while(j >= 0 && col[j] > k) {
	    col[j+1] = col[j];
	    vector[j+1] = vector[j];
	    j--;
	 }
	 col[j+1] = k;
	 vector[j+1] = vtemp;
      }
   } else {
      if((nfmt = compile_format(format)) < 0) {
	 return(-1);
      }
      if(nfmt != nvec) {
	 msg("Number of formats in string doesn't equal number of vectors\n");
	 return(-1);
      }
      if(check_format(format,vector) < 0) { /* also set vector->type */
	 return(-1);
      }
   }
/*
 * open file for reading, and seek to old position
 */
   if((fil = fopen_seek(data_file,"r",l1)) == NULL) {
      msg_1s("Can't open %s\n",data_file);
      return(-1);
   }
/*
 * We have now opened the file, so allocate space for vectors that we are going
 * to read. Allocate space in multiples of DSPACE.
 */
   if(l2 > l1) {
      size = l2 - l1 + 1;
   } else {
      size = DSPACE;
   }

   for(i = 0;i < nvec;i++) {
      if(vec_malloc(vector[i],size) < 0) {
	 msg_1d("Can't get space for vector %d in read_column\n",i);
	 nvec = i;
	 break;
      }
   }
/*
 * Finally read data
 */
   while(sm_getline(line,LINESIZE,fil) != NULL) {
      warned = 0;
      if(sm_interrupt) {				/* respond to ^C */
	 saved_seek_ptr = -1;
         FCLOSE(fil);
         for(j = 0;j < nvec;j++) {
	    vec_free(vector[j]);
	 }
	 return(-1);
      }
      if(line_ctr < l1) {		/* line not in desired range */
	 continue;
      }

      if(col[0] <= 0 || line[0] == '!') {	/* just print data */
         msg_1s("%s\n",line);
      } else if(line[0] == cchar) {	/* also a comment */
         if(sm_verbose > 1) msg_1s("%s\n",line);
      } else {				/* read value from line */
	 if(*format == '\0') {
	    lptr = line;
	    for(c = 1,j = 0;;c++) {	/* count through columns */
	       while(isspace(*lptr)) lptr++;
	       if(*lptr == ',') {	/* pass one comma */
		  lptr++;
		  while(isspace(*lptr)) lptr++;
	       }
	       if(*lptr == '\0') {
		  if(readall) { /* ignore error */
		     if(sm_verbose > 1) {
			msg_1d("Error reading from line %d\n",line_ctr);
		     }
		     warned = 1;
		  } else {
		     if(sm_verbose > 0 && l2 > 0) { /* upper limit to lines */
			msg_1d("couldn't read column %d ",col[j]);
			msg_1d("of line %d\n",line_ctr);
		     }
		     l2 = line_ctr - 1;
		     goto end_read;	/* break out of nested loop */
		  }
	       }

	       if(c == col[j]) {		/* read this column */
		  if(vector[j]->type == VEC_FLOAT) {
		     if(*lptr == '*' || *lptr == ',') { /* An `empty' field */
			if(sm_verbose > 1) {
			   msg_1d("Column %d ",col[j]);
			   msg_1d("in line %d ",line_ctr);
			   msg_1s("of File %s is starred\n",data_file);
			}
			value = 1.001*NO_VALUE;
		     } else {
			value = atof2(lptr);
			if(value == 0) { /* may not be numeric at all */
			   sptr = lptr;	/* pointer to start of field */
			   while(*lptr != '\0' && !isspace(*lptr) &&
				 *lptr != ',') {
			      lptr++;	/* we'd have to do this later anyway */
			   }
			   *lptr = '\0';	/* terminate sptr */
			   if((k = num_or_word(sptr)) != 1 && k != 2) {
			      if(readall) { /* ignore error */
				 if(!warned && sm_verbose > 1) {
				    msg_1d("Error reading from line %d\n",
					   line_ctr);
				 }
				 value = 1.001*NO_VALUE;
			      } else {
				 if(l2 > 0 && /* upper limit set */
				    sm_verbose > 0) {
				    msg_1d("couldn't read column %d ",col[j]);
				    msg_1d("of line %d\n",line_ctr);
				 }
				 l2 = line_ctr - 1;
				 goto end_read; /* break out of nested loop */
			      }
			   }
			   *lptr = ' ';	/* something for while() to eat */
			}
		     }

		     vector[j++]->vec.f[n_elem] = value;
		  } else if(vector[j]->type == VEC_LONG) {
		     LONG ivalue;
		     if(*lptr == '*' || *lptr == ',') { /* An `empty' field */
			if(sm_verbose > 1) {
			   msg_1d("Column %d ",col[j]);
			   msg_1d("in line %d ",line_ctr);
			   msg_1s("of File %s is starred\n",data_file);
			}
			ivalue = 1 << (4*sizeof(LONG) - 1);
		     } else {
			char *field_end;
			ivalue = atoi2(lptr, 0, &field_end);
			if(ivalue == 0) { /* may not be numeric at all */
			   if(*lptr != '0') {
			      if(readall) { /* ignore error */
				 if(!warned && sm_verbose > 1) {
				    msg_1d("Error reading from line %d\n",
					   line_ctr);
				 }
				 ivalue = 1 << (4*sizeof(LONG) - 1);
			      } else {
				 if(l2 > 0 && /* upper limit set */
				    sm_verbose > 0) {
				    msg_1d("couldn't read column %d ",col[j]);
				    msg_1d("of line %d\n",line_ctr);
				 }
				 l2 = line_ctr - 1;
				 goto end_read; /* break out of nested loop */
			      }
			   }
			   lptr = field_end;
			   *lptr = ' ';	/* something for while() to eat */
			}
		     }
		     
		     vector[j++]->vec.l[n_elem] = ivalue;
		  } else {
		     sptr = lptr;	/* pointer to start of field */
		     while(*lptr != '\0' && !isspace(*lptr) && *lptr != ',') {
			lptr++;		/* we'd have to do this later anyway */
		     }
		     *lptr = '\0';	/* terminate sptr */
		     (void)strncpy(vector[j++]->vec.s.s[n_elem],sptr,
				   VEC_STR_SIZE);
		     *lptr = ' ';	/* something for while() to eat */
		  }
		  if(j >= nvec) break;
	       }
	       while(*lptr != '\0' && !isspace(*lptr) && *lptr != ',') lptr++;
	    }
	 } else {
	    if((j = scan_line(line,format,vector,n_elem,readall,line_ctr))
								      < nvec) {
	       if(readall) {		/* ignore error */
		  ;
	       } else  {
		  if(sm_verbose > 0 && l2 > 0) { /* upper limit to lines set */
		     msg_1d("Error reading from line %d\n",line_ctr);
		  }
		  l2 = line_ctr;
		  break;
	       }
	    }
	 }

         if(++n_elem >= size) {			/* need more space on vecs */
	    size *= 2;
	    for(j = 0;j < nvec;j++) {
	       if(vec_realloc(vector[j],(int)size) < 0) {
	          while(--j >= 0) {
	             vec_free(vector[j]);
	          }
	          FCLOSE(fil);
	          return(-1);
	       }
	    }
	 }
      }
      if(l2 > 0 && line_ctr >= l2) {	/* we've read our last line */
	 break;
      }
   }
   l2 = line_ctr;

end_read:		/* Target of two breaks from nested loop above */

   FCLOSE(fil);
   if(n_elem > 0) {		/* we read some data */
/*
 * recover un-wanted storage
 */
      for(j = 0;j < nvec;j++) {
	 if(vec_realloc(vector[j],n_elem) < 0) {
            msg("Can't reallocate space in read_column\n");
	    for(j = nvec;j < NFMT_VECS;j++) {
	       vec_free(vector[j]);
	    }
            return(-1);
         }
         vector[j]->dimen = n_elem;
      }
      if(sm_verbose > 0) {
	 msg_1d("Read lines %d ",l1 <= 0 ? 1 : l1);
	 msg_1d("to %d ",l2 <= 0 ? line_ctr : l2);
	 msg_1s("from %s\n",data_file);
      }
      return(nvec);
   } else {
      if(col[0] > 0 || sm_verbose > 0) {
	 msg_1s("No lines read from %s\n",data_file);
      }
      return(-1);
   }
}

/*****************************************************************************/
#define NCHAR 128			/* number of valid characters */
#define NO_WIDTH '\377'			/* format doesn't specify a width */
#define NRANGE 5			/* number of %[ formats allowed */
#define IS_FMT(C) ((C) & '\200')	/* it's a format character */

typedef char RANGE[NCHAR];
static RANGE ranges[NRANGE];

/********************************************************/
/*
 * Pre-process a format string, checking and re-formatting %-formats
 */
static int
compile_format(fmt)
char *fmt;
{
   int except;				/* if true, %[^...] */
   char *fmt0 = fmt;			/* initial value of fmt */
   int i;
   int nfmt = 0;			/* number of fields to convert */
   int nrange = -1;			/* which range is in use? */
   char *ptr;				/* output pointer */
   int skip;
   int width;				/* width of % format */

   for(ptr = fmt;*fmt != '\0';) {
      if(*fmt == '%') {
	 if(*(fmt + 1) == '%') {	/* not a %-format */
	    *ptr++ = '%';
	    fmt += 2;
	 } else {
	    fmt++;			/* skip the % */
	    skip = (*fmt == '*' ? 1 : 0);
	    if(skip) {
	       fmt++;
	    } else {
	       nfmt++;
	    }
	    width = 0;
	    while(isdigit(*fmt)) width = 10*width + *fmt++ - '0';
	    if(width == 0) {		/* we'll skip whitespace anyway,
					   so don't require that it be
					   matched exactly */
	       while(ptr >= fmt0 && isspace(*(ptr - 1))) ptr--;
	    }
	    *ptr++ = (width == 0 ? NO_WIDTH : ('\200' | width));
	    if(*fmt == 'l' || *fmt == 'h') fmt++; /* ignore such things */
	    if(skip) {			/* ignore the field */
	       *ptr++ = '*';
	    }
	    switch (*fmt++) {
	     case '\0':
	       *fmt = *ptr = '\0';
	       while(*--fmt != '%') continue;	/* find start of format */
	       msg_1s("Unfinished %%-format at \"%s\"\n",fmt);
	       return(-1);
	     case 'd':
	     case 'e':
	     case 'f':
	     case 'g':
	     case 'o':
	     case 's':
	     case 'x':
	       *ptr++ = *(fmt - 1); break;
	     case 'n':
	       if(skip) {
		  msg("You cannot specify a %%*n format; it's meaningless\n");
		  return(-1);
	       }
	       *ptr++ = *(fmt - 1);
	       break;
	     case '[':
	       if(++nrange >= NRANGE) {
		  msg_1d("Only %d %%[ formats are allowed; sorry\n",NRANGE);
		  return(-1);
	       }
	       *ptr++ = *(fmt - 1);
	       if(*fmt == '^') {
		  except = 1; fmt++;
	       } else {
		  except = 0;
	       }
	       for(i = 0;i < NCHAR;i++) ranges[nrange][i] = except;
	       ranges[nrange][0] = 0;	/* never match '\0' */
	       if(*fmt == '-') ranges[nrange][(int)(*fmt++)] = !except;
	       for(;;) {
		  if(*fmt == '\0') {
		     *ptr = '\0';
		     while(*--fmt != '%') continue; /* find start of format */
		     msg_1s("Unfinished %%-format at \"%s\"\n",fmt);
		     return(-1);
		  }
		  if(*fmt == ']' && !(except && *(fmt - 1) == '^')) {
		     fmt++;
		     break;
		  } else {
		     if(*(fmt + 1) == '-') { /* a range */
			for(i = *fmt;i <= *(fmt + 2);i++) {
			   ranges[nrange][i] = !except;
			}
			fmt += 3;
		     } else {
			ranges[nrange][(int)(*fmt++)] = !except;
		     }
		  }
	       }
	       break;
	     default:
	       *fmt = *ptr = '\0';
	       while(*--fmt != '%') continue;	/* find start of format */
	       msg_1s("Unknown %%-format: \"%s\"\n",fmt);
	       return(-1);
	    }
	 }
      } else {
	 *ptr++ = *fmt++;
      }
   }
   *ptr = '\0';
   return(nfmt);
}

/*****************************************************************************/
/*
 * Check that vectors match format
 */
static int
check_format(fmt,vector)
char *fmt;
VECTOR **vector;
{
   for(;*fmt != '\0';fmt++) {
      if(IS_FMT(*fmt)) {
	 if(*++fmt == '*') continue;
	 switch (*fmt) {
	 case 'e': case 'f': case 'g': case 'n':
	    (*vector)->type = VEC_FLOAT;
	    break;
	  case 'd': case 'o': case 'x':
	    (*vector)->type = VEC_LONG;
	    break;
	  case 's': case '[':
	    (*vector)->type = VEC_STRING;
	    break;
	  default:
	    msg_1s("Vector %s corresponds to an unknown format\n",
		   (*vector)->name);
	    return(-1);
	 }
	 vector++;
      }
   }
   return(0);
}

/*****************************************************************************/
/*
 * Set a new datafile --- called by DATA
 */
long saved_seek_ptr = -1;		/* where we got to in a file */

void
set_data_file(file)
char *file;
{
   setvar("_l1","0"); setvar("_l2","0");
   setvar("data_file",file);
   saved_seek_ptr = -1;
}

/*****************************************************************************/
/*
 * Open a file, and seek to where we were in appropriate
 */
static FILE *
fopen_seek(file,mode,l1)
char *file,*mode;
int l1;
{
   FILE *fil;
   use_seek = (*print_var("save_read_ptr") != '\0'); 
   
   if((fil = fopen(file,mode)) != NULL) {
      if(use_seek) {
	 if(saved_seek_ptr >= 0 && l1 >= line_ctr) { /* we can save time
							by seeking */
	    if(sm_verbose > 1) {
	       msg_1d("Seeking to start of line %d",line_ctr);
	       msg_1s(" of file %s\n",file);
	    }
	    if(fseek(fil,saved_seek_ptr,0) < 0) {
	       msg_1s("Can't seek to old position in %s\n",file);
	       fseek(fil,0,0);
	       line_ctr = 0;
	    } else {
	       line_ctr--;		/* we are about to re-read it */
	    }
	 } else {
	    line_ctr = 0;
	 }
      } else {
	 line_ctr = 0;
      }
   }
   return(fil);
}

/*****************************************************************************/
/*
 * Read the vectors from line using the pre-compiled format string
 */
static int
scan_line(line,fmt,vector,nvec,readall,lineno)
char *line;				/* line to parse */
char *fmt;				/* compiled format */
VECTOR **vector;			/* vectors to fill */
int nvec;				/* element of vectors to set */
int readall;				/* read all of line even on error */
int lineno;				/* which line are we on? */
{
   char c;
   char *end;				/* end of input field */
   REAL fval;				/* an floating value */
   RANGE *is_allowed = ranges;		/* allowed ranges of chars for %[ */
   int i;
   int is_negative;			/* is number -ve? */
   LONG ival;				/* an integer value */
   int ivec = 0;			/* which vector are we up to? */
   int nmatch = 0;			/* number of fields matched */
   int nread = 0;			/* number of fields except %n read */
   char *ptr;				/* pointer to string values */
   char savec = '\0';			/* character at end of field */
   int skip;				/* skip this field? */
   int width;				/* width of field */
   
   while(*line != '\0') {
#if 1
      if(ivec != nmatch) {
	 fprintf(stderr,"ivec != nmatch -- please tell rhl\n");
      }
#endif
      if(IS_FMT(*fmt)) {
	 if((width = *fmt++) == NO_WIDTH) { /* no width was specified */
	    end = NULL;
	 } else {
	    width &= ~'\200';
	    for(end = line;*end != '\0' && width-- > 0;) end++;
	    savec = *end;
	    *end = '\0';
	 }
	 if(*fmt == '*') {
	    skip = 1;
	    fmt++;
	 } else {
	    skip = 0;
	 }
	 switch (c = *fmt++) {
	  case 'd': case 'o': case 'x':
	    while(isspace(*line)) line++;
	    if(*line == '\0') {
	       if(!skip) {
		  vector[ivec++]->vec.l[nvec] = ~0;
	       }
	       set_failed(fmt,nread,vector,ivec,nvec,lineno);
	       return(nmatch);
	    }

	    if(*line == '-') {
	       is_negative = 1;
	       line++;
	    } else if(*line == '+') {
	       is_negative = 0;
	       line++;
	    } else {
	       int bad = 0;
	       if(c == 'x') {
		  if((line[0] == '0' && (line[1] == 'x' || line[1] == 'X')
						      && !isxdigit(line[2])) ||
		     !isxdigit(line[0])) {
		     bad = 1;
		  }
	       } else if(!isdigit(*line)) {
		  bad = 1;
	       }

	       if(bad) {
		  if(!skip) {
		     vector[ivec++]->vec.l[nvec] = ~0;
		  }
		  set_failed(fmt,nread,vector,ivec,nvec,lineno);
		  return(nmatch);
	       }
	       is_negative = 0;
	    }

	    ival = atoi2(line, (c == 'd' ? 10 : c == 'o' ? 8 : 16), &line);
	    if(is_negative) ival = -ival;
	    
	    if(!skip) {
	       vector[ivec++]->vec.l[nvec] = ival;
	       nread++;
	    }
	    break;
	  case 'e': case 'f': case 'g':
	    while(isspace(*line)) line++;
	    if(*line == '\0') {
	       if(!skip) {
		  vector[ivec++]->vec.f[nvec] = 1.001*NO_VALUE;
	       }
	       set_failed(fmt,nread,vector,ivec,nvec,lineno);
	       return(nmatch);
	    }
	    
	    if(*line == '-') {
	       is_negative = 1;
	       line++;
	    } else if(*line == '+') {
	       is_negative = 0;
	       line++;
	    } else {
	       if(!isdigit(*line)) {
		  if(!skip) {
		     vector[ivec++]->vec.f[nvec] = 1.001*NO_VALUE;
		  }
		  set_failed(fmt,nread,vector,ivec,nvec,lineno);
		  return(nmatch);
	       }
	       is_negative = 0;
	    }

	    fval = 0;
	    while(isdigit(*line)) {
	       fval = 10*fval + (*line++ - '0');
	    }
	    if(*line == '.') {
	       REAL frac = 0.0, scale = 0.1;
	       line++;
	       while(isdigit(*line)) {
		  frac = frac + scale*(*line++ - '0');
		  scale *= 0.1;
	       }
	       fval += frac;
	    }

	    if(c == 'e' || c == 'g') {	/* there could be an exponent  */
	       int neg_exp,i;
	       char *save_line = line;
	       
	       if(*line == 'e' || *line == 'E' || *line == 'd' ||*line == 'D'){
		  line++;
		  if(*line == '-') {
		     neg_exp = 1; line++;
		  } else if(*line == '+') {
		     neg_exp = 0; line++;
		  } else {
		     neg_exp = 0;
		  }
		  if(!isdigit(*line)) {
		     line = save_line;
		     msg_1s("Badly formed exponent at \"%s\"\n",line);
		     break;
		  }
		  i = *line++ - '0';
		  while(isdigit(*line)) {
		     i = 10*i + (*line++ - '0');
		  }
		  if(neg_exp) i = -i;
		  fval *= pow(10.0,(double)i);
	       }
	    }

	    if(is_negative) fval = -fval;
	    if(!skip) {
	       vector[ivec++]->vec.f[nvec] = fval;
	       nread++;
	    }
	    break;
	  case 'n':
	    vector[ivec++]->vec.f[nvec] = nread;
	    break;
	  case 's':
	    if(end == NULL) {		/* unknown width */
	       while(isspace(*line)) line++;
	       if(*line == '\0') {
		  if(!skip) {
		     vector[ivec]->vec.s.s[nvec][0] = '\0';
		  }
		  ivec++;
		  set_failed(fmt,nread,vector,ivec,nvec,lineno);
		  return(nmatch);
	       }
	       
	       if(skip) {
		  while(*line != '\0' && !isspace(*line)) line++;
	       } else {
		  for(ptr = vector[ivec]->vec.s.s[nvec], i = 0;
		      i < VEC_STR_SIZE-1 && *line != '\0' && !isspace(*line);){
		     ptr[i++] = *line++;
		  }
		  ptr[i] = '\0';
		  if(i == VEC_STR_SIZE - 1 && *line != '\0') {
		     if(sm_verbose) {
			msg_1d("truncating %d", nvec);
			msg_1s("%s element ", (nvec%10 == 1 ? "st" :
					       nvec%10 == 2 ? "nd" :
					       nvec%10 == 3 ? "rd" : "th"));
			msg_1d("of %d", ivec);
			msg_1s("%s vector: ", (ivec%10 == 1 ? "st" :
					       ivec%10 == 2 ? "nd" :
					       ivec%10 == 3 ? "rd" : "th"));
			msg_1s("%s\n",vector[ivec]->vec.s.s[nvec]);
		     }
		     while(*line != '\0' && !isspace(*line)) line++;
		  }

		  ivec++; nread++;
	       }
	    } else {
	       if(skip) {
		  ;			/* line will be set to end for us */
	       } else {
		  strncpy(vector[ivec]->vec.s.s[nvec],line,VEC_STR_SIZE);
		  nread++;
	       }

	       i = (vector[ivec]->vec.s.s[nvec][VEC_STR_SIZE - 1] != '\0')?1:0;
	       vector[ivec]->vec.s.s[nvec][VEC_STR_SIZE - 1] = '\0';
	       if(i && sm_verbose) {
		  msg_1d("truncating %dth element ", nvec);
		  msg_1d("of %dth vector: ", ivec);
		  msg_1s("%s\n",vector[ivec]->vec.s.s[nvec]);
	       }
	       ivec++;
	    }
	    break;
	  case '[':
	    if(*line == '\0') {
	       if(!skip) {
		  vector[ivec++]->vec.s.s[nvec][0] = '\0';
	       }
	       set_failed(fmt,nread,vector,ivec,nvec,lineno);
	       return(nmatch);
	    }
	    if(skip) {
	       while(*line != '\0' && (*is_allowed)[(int)*line]) line++;
	    } else {
	       for(ptr = vector[ivec]->vec.s.s[nvec], i = 0;
		   i < VEC_STR_SIZE-1 && (*is_allowed)[(int)*line];) {
		  ptr[i++] = *line++;
	       }
	       ptr[i] = '\0';
	       if(i == VEC_STR_SIZE - 1 && *line != '\0') {
		  if(sm_verbose) {
		     msg_1d("truncating %d", nvec);
		     msg_1s("%s element ", (nvec%10 == 1 ? "st" :
					    nvec%10 == 2 ? "nd" :
					    nvec%10 == 3 ? "rd" : "th"));
		     msg_1d("of %d", ivec);
		     msg_1s("%s vector: ", (ivec%10 == 1 ? "st" :
					    ivec%10 == 2 ? "nd" :
					    ivec%10 == 3 ? "rd" : "th"));
		     msg_1s("%s\n",vector[ivec]->vec.s.s[nvec]);
		  }
		  while((*is_allowed)[(int)*line]) line++;
	       }
	       
	       ivec++; nread++;
	    }
	    is_allowed++;
	    break;
	  default:
	    {
	       char tmp[2];
	       tmp[0] = *(fmt - 1); tmp[1] = '\0';
	       msg_1s("SM error: missing %%-format %s in scan_line().\n",tmp);
	       msg("Please file a bug report\n");
	       break;
	    }
	 }
	 if(!skip) nmatch++;
	 if(end != NULL) {
	    line = end;
	    *line = savec;
	 }
      } else {
	 if(*fmt == '\0') {		/* all vectors are read */
	    while(isspace(*line)) line++; /* skip trailing whitespace */
	 }
	 if(*fmt++ != *line++) {
	    set_failed(fmt,nread,vector,ivec,nvec,lineno);
	    return(nmatch);
	 }
      }
   }

   if(*fmt != '\0') {
      set_failed(fmt,nread,vector,ivec,nvec,lineno);
   }
   return(nmatch);
}

/*****************************************************************************/
/*
 * set the values of the vectors that we failed to read from an input
 * line. We can't simply set them to invalid as there might be some
 * %n fields that must be honoured.
 */
static void
set_failed(fmt,nmatch,vector,ivec,nvec,lineno)
char *fmt;				/* compiled format */
int nmatch;				/* how many vectors we've read */
VECTOR **vector;			/* vectors to fill */
int ivec;				/* which vector to set next */
int nvec;				/* element of vectors to set */
int lineno;				/* current line number */
{
   if(sm_verbose > 1) {
      msg_1d("Error reading from line %d\n",lineno);
   }

   for(;*fmt != '\0';fmt++) {
      if(IS_FMT(*fmt)) {
	 fmt++;				/* skip width */
	 if(*fmt == '*') {		/* no corresponding vector */
	    continue;
	 }
	 switch (*fmt) {
	  case 'd': case 'o': case 'x':
	    vector[ivec]->vec.l[nvec] = ~0;
	    break;
	  case 'e': case 'f': case 'g':
	    vector[ivec]->vec.f[nvec] = 1.001*NO_VALUE;
	    break;
	  case 'n':
	    vector[ivec]->vec.f[nvec] = nmatch;
	    break;
	  case 's': case '[':
	    vector[ivec]->vec.s.s[nvec][0] = '\0';
	    break;
	  default:
	    msg("SM error: missing %%-format in set_failed().\n");
	    msg("Please file a bug report\n");
	    break;
	 }
	 ivec++;
      }
   }
}

/***********************************************/

char *
read_variable(data_file,l1,field)
char data_file[];			/* file to read from */
int l1,					/* lines to read */
    field;				/* which field to read */
{
   static char line[LINESIZE];		/* buffer for one line of file */
   FILE *fil;				/* descriptor for data_file */
   int i,j;				/* counters */
   
#ifdef vms
   check_for_fortran_file(data_file );
#endif
/*
 * open file for reading
 */
   if((fil = fopen_seek(data_file,"r",l1)) == NULL) {
      msg_1s("Can't open %s\n",data_file);
      return(NULL);
   }
/*
 * Read file
 */
   while(sm_getline(line,LINESIZE,fil) != NULL) {
      if(sm_interrupt) {				/* respond to ^C */
         FCLOSE(fil);
	 return(NULL);
      }
      if(line_ctr < l1) {		/* line not in desired range */
	 continue;
      } else {
	 FCLOSE(fil);
	 j = 0;					/* index of string to return */
	 if(line[j] == cchar) j++;		/* skip comment character */
	 if(field > 0) {			/* look for a sub-field */
	    for(i = 1;line[j] != '\0' && i < field;i++) {  /* find field */
	       while(isspace(line[j])) {
		  j++;
	       }
	       while(line[j] != '\0' && !isspace(line[j])) { /* skip field */
		  j++;
	       }
	    }
	    while(isspace(line[j])) {		/* skip to start of field */
	       j++;
	    }
	    for(i = j;line[i] != '\0' && !isspace(line[i]);i++) continue;
	    line[i] = '\0';			/* null terminate field */
	    if(line[j] == '\0' && sm_verbose) {
	       msg_1d("Can't find field %d\n",field);
	    }
	 }
	 return(&line[j]);
      }
   }

   FCLOSE(fil);
   if(sm_verbose > 0) {
      msg_1d("Unable to read line %d\n",l1);
   }
   return(NULL);
}

#ifdef vms
static
check_for_fortran_file(data_file)
char data_file[];
{
      struct stat file_descriptor;      /*  holds file attributes  */
/*
 * if this is vms, first call stat to get the information about the file
 * (see the Vax C manual description of stat for more information)
 */
   if(fortran_file == UNDEFINED ) {
      if(stat(data_file, &file_descriptor ) == -1) {
         msg_1s("stat can't get record attributes for %s\n",data_file );
      }
  
      if(file_descriptor.st_fab_rat == FAB$M_FTN) {
         fortran_file = FORTRAN_FILE;
      } else {
         fortran_file = PROPER_FILE;
      }
   }
}
#endif

/*****************************************************************************/
/*
 * Read a logical line from the file, allowing for \ continuation lines
 */
static char *
sm_getline( line, line_size, fil )
char *line;
int line_size;
FILE *fil;
{
   int len;

   if(use_seek) saved_seek_ptr = ftell(fil); /* remember start of line */
   
   if(sm_getline_phys(line,line_size,fil) == NULL) {
      saved_seek_ptr = -1;
      return(NULL);
   }
/*
 * Check whether the newline was escaped with a \, and if so discard both the
 * \ and the \n and read the next line as a continuation line
 */
   len = strlen(line);
   while(line[len - 2] == '\\' && line[len - 1] == '\n') {
      char *ret = sm_getline_phys(&line[len - 2],line_size-len+2,fil);
      len += strlen(&line[len]);
      if(ret == NULL) {
	 line[len - 2] = '\n';
	 line[len - 1] = '\0';
      }
   }

   line_ctr++;
   
   if(line[len - 1] != '\n') {
      int c,c_old;
      
      if(len >= line_size - 1) {
	 msg_1d("Line %d is too long\n",line_ctr);
      } else {
	 msg_1d("Line %d has no terminating newline\n",line_ctr);
      }

      for(c = '\0';;c_old = c) {	/* read rest of long line */
	 c_old = c;
	 if((c = getc(fil)) == EOF) {
	    break;
	 } else if(c == '\n') {
	    if(c_old != '\\') {
	       break;
	    }
	 }
      }
   }

   if(line[len - 1] == '\n') {
      line[len - 1] = '\0';
   }

   return(line);
}

/*****************************************************************************/
/*
 * Read a physical line, don't process \-escaped newlines
 *
 * If not VMS, then simply use fgets.
 *
 * If this is vms, the file may have "fortran carriage control attributes",
 * which means that the O/S has taken each record and pre/appended various
 * combinations of ^J^L^M as a response to the Hollerith Control Chars.
 *
 * Specifically, we consider the cases:
 *   NUL 	-->	<record>^M		Treat like <record>\n
 *   0   	-->	^J^J<record>^M		Treat like \n<record>\n
 *   1  	-->	^L<record>^M		Treat like \n\n<record>\n
 *   other   	-->	^J<record>^M		Treat like <record>\n
 */
static char *
sm_getline_phys( line, line_size, file_ptr )
char *line;
int line_size;
FILE *file_ptr;
{
#ifdef vms
   char c;
#endif
#ifndef vms
#if 0				/* use fgets; doesn't handle \r correctly */
   return fgets(line,line_size,file_ptr);
#else
   char *end = line + line_size - 1;
   char *ptr = line;
   while (ptr < end) {
      int c = getc(file_ptr);
      if (c == EOF) {
	 break;
      }
      *ptr++ = c;
      
      if (c == '\n') {
	 break;
      } else if (c == '\r') {	/* might be \r\n */
	 char nc = getc(file_ptr);
	 if (nc != '\n') {
	    ungetc(nc, file_ptr);
	 }
	 break;
      }
   }

   if (ptr <= line) {
       return NULL;
   } else if (*(ptr - 1) != '\n') {
       if (ptr < end) {
	   *ptr++ = '\n';
       }
   }
   *ptr = '\0';
   
   return line;
#endif
#else
   if(fortran_file == FORTRAN_FILE) {
      if((c = getc(file_ptr)) == EOF) {
         return(NULL);
      } else if(c == CTRL_J) {
         if((c = getc(file_ptr)) == CTRL_J) {	/* mimic blank line */
            line[0] = '\n';
            line[1] = '\0';
         } else {			/* just one ^J, so read rest of line */
            (void)ungetc(c,file_ptr);
            if(fgets_cr(line,line_size,file_ptr) == NULL) {
	       return( NULL );
            }
         }
      } else if(c == CTRL_L) {			/* really a new page */
         (void)ungetc(CTRL_M,file_ptr);		/* but 2 blank lines will do */
         line[0] = '\n';
         line[1] = '\0';
      } else {
         (void)ungetc(c,file_ptr);
         if(fgets_cr(line,line_size,file_ptr) == NULL) {
 	    return( NULL );
         }
      }
   } else {
      if(fgets(line,line_size,file_ptr) == NULL) {
	return( NULL );
      }
   }
   return( line );
#endif
}

/****************************************************************/
/*
 * read a row from a file into a vector
 */
int
read_row(data_file,row,vector)
char data_file[];			/* file to read from */
int row;				/* which row to read */
VECTOR *vector;				/* vector to read into */
{
   char line[LINESIZE],			/* buffer for one line of file */
	*lptr,				/* pointer to line */
        *sptr;				/* pointer to start of field */
   FILE *fil;				/* descriptor for data_file */
   double value;			/* value to read */
   int j,
       n_elem = 0;			/* number of elements on vector */
   unsigned size;			/* current size of vector->vec.f */

#ifdef vms
   check_for_fortran_file( data_file );
#endif
/*
 * open file for reading
 */
   if((fil = fopen_seek(data_file,"r",row)) == NULL) {
      msg_1s("Can't open %s\n",data_file);
      vector->dimen = 0;
      vector->type = VEC_NULL;
      return(-1);
   }
/*
 * We have now opened the file, so allocate space for vector that we are going
 * to read. Allocate space in multiples of DSPACE.
 */
   if(vec_malloc(vector,DSPACE) < 0) {
      msg("Can't allocate space to read vector in read_column\n");
      vector->dimen = 0;
      vector->type = VEC_NULL;
      return(-1);
   }
   size = DSPACE;
/*
 * Finally read data
 */
   while(sm_getline(line,LINESIZE,fil) != NULL) {
      if(sm_interrupt) {				/* respond to ^C */
         FCLOSE(fil);
	 vec_free(vector);
	 vector->dimen = 0;
	 return(-1);
      }
	
      if(line_ctr == row) {
         lptr = line;

	 n_elem = 0;
         for(;;) {
	    while(isspace(*lptr)) lptr++;
	    if(*lptr == ',') {		/* pass one comma */
	       lptr++;
	    }
	    while(isspace(*lptr)) lptr++;
      	    if(*lptr == '\0') break;

	    if(vector->type == VEC_FLOAT || vector->type == VEC_LONG) {
	       if(*lptr == '*' || *lptr == ',') { /* An `empty' field */
		  if(sm_verbose > 1) {
		     msg_1d("Column %d ",n_elem + 1);
		     msg_1d("of row %d ",row);
		     msg_1s("of File %s is starred\n",data_file);
		  }
		  value = 1.001*NO_VALUE;
	       } else {
		  value = atof2(lptr);
		  if(value == 0) {		/* may not be numeric at all */
		     sptr = lptr;		/* pointer to start of field */
		     while(*lptr != '\0' && !isspace(*lptr) && *lptr != ',') {
			lptr++;		/* we'd have to do this later anyway */
		     }
		     *lptr = '\0';		/* terminate sptr */
		     if((j = num_or_word(sptr)) != 1 && j != 2) {
			if(sm_verbose > 0) {
			   msg_1d("couldn't read column %d ",n_elem + 1);
			   msg_1d("of row %d\n",row);
			}
			break;
		     }
		     *lptr = ' ';	/* something for while() to eat */
		  }
	       }

	       if(vector->type == VEC_LONG) {
		  vector->vec.l[n_elem] = value;
	       } else {
		  vector->vec.f[n_elem] = value;
	       }
	    } else {
	       sptr = lptr;		/* pointer to start of field */
	       while(*lptr != '\0' && !isspace(*lptr) && *lptr != ',') {
		  lptr++;		/* we'd have to do this later anyway */
	       }
	       *lptr = '\0';		/* terminate sptr */
	       (void)strncpy(vector->vec.s.s[n_elem],sptr,VEC_STR_SIZE);
	       *lptr = ' ';		/* something for while() to eat */
	    }
	    while(*lptr != '\0' && !isspace(*lptr) && *lptr != ',') lptr++;

	    if(++n_elem >= size) {	/* need more space on vecs */
	       size *= 2;
	       if(vec_realloc(vector,(int)size) < 0) {
		  FCLOSE(fil);
		  return(-1);
	       }
	    }
	 }
         break;
      }
   }

   FCLOSE(fil);
   if(n_elem > 0) {		/* we read some data */
/*
 * recover un-wanted storage
 */
      if(vec_realloc(vector,n_elem) < 0) {
         msg("Can't reallocate space in read_row\n");
         vector->dimen = 0;
         return(-1);
      }
      if(sm_verbose > 0) {
	 msg_1d("Read %d ",vector->dimen);
	 msg_1d("columns from row %d ",row);
	 msg_1s("of %s\n",data_file);
      }
      return(0);
   } else {
      if(sm_verbose > 0) {
	 msg_1s("No columns read from %s\n",data_file);
      }
      return(-1);
   }
}

#ifdef vms
/*
 * This function is the equivalent of fgets, but it reads to CR not LF.
 * It returns a closing LF, however, for compatibility
 */
static char *
fgets_cr(buff,n,fil)
char *buff;
int n;
FILE *fil;
{
   register char *end,
   		 *ptr;
   register int c;
   
   ptr = buff;
   end = buff + n - 1;			/* leave room for NUL */
   
   while(ptr < end) {
      if((c = getc(fil)) == EOF) {
         if(ptr == buff) {
	    return(NULL);
	 } else {
	    *ptr = '\0';
	    return(buff);
	 }
      } else if(c == CTRL_M) {
         *ptr++ = '\n';
	 *ptr = '\0';
	 return(buff);
      } else {
         *ptr++ = c;
      }
   }
   *ptr = '\0';
   return(buff);
}
#endif
