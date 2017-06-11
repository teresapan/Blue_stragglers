%{
#  include "copyright.h"
#  include "options.h"
#  include <math.h>
#  include <stdio.h>
#  include <ctype.h>
#  include <assert.h>
#ifndef INTEGER
#  include "control.h"
#endif
#  include "stack.h"
#  include "vectors.h"
#  include "yaccun.h"
#  include "devices.h"
#  include "sm_declare.h"
#  include "sm.h"
#  if defined(_IBMR2)
#     pragma alloca
#  endif

#define bcopy(S1,S2,N) memcpy(S2,S1,N)	/* use memcpy instead */
#define Pi 3.141592653589793238
/*
 * And some variables of my own:
 * The Auto Variables get moved into yyparse and made auto when control.y
 * is built (under unix, that is. Under vms, this won't happen).
 */
/*Auto Variables*/
   static char buff[MACROSIZE + 1],	/* buffer for reading macros */
	       word[CHARMAX];		/* buffer for storing WORD */
   char variable_name[CHARMAX];		/* name of variable being defined */
/*End Auto Variables*/
   static char data_file[CHARMAX], /* name of current data file */
               table_fmt[CHARMAX]; /* format of current TABLE */
   extern char prompt[],	/* system prompt */
               user_name[];	/* name of user of programme */
   extern FILE *logfil;		/* where to log all terminal input */
   static float xy_range,	/* range for this axis (x or y) */
                x_range = 0,	/* Ranges for the x */
		y_range = 0;	/* and y axes */
   static int in_graphics = 0,	/* are graphics active? */
              graph_mode,	/* terminal is set to graph mode */
	      line_1,line_2,	/* line numbers to use in data_file */
              number_is_int,	/* last <number> was an int */
              subtable = -1;	/* which sub-table to read */
   static LONG intval;		/* value of last <number> if an int */
   extern int abort_plot,	/* abort current plot */
              in_quote,		/* am I in a quoted string? */
   	      sm_interrupt,	/* do you want to interrupt something ? */
   	      noexpand,		/* expand @#$ ? */
	      plength,		/* number of printing chars in prompt */
	      recurse_lvl,	/* how deeply nested are we? */
   	      sm_verbose;	/* produce lots of output */
/*
  This grammar generates 15 Shift/Reduce and 6 Reduce/Reduce conflicts.
  They are (approximately; state numbers may vary):
State 9 contains 2 shift/reduce conflicts.	DELETE . HISTORY/WORD ...
State 25 contains 1 shift/reduce conflicts.	RESTORE . word_or_space
State 26 contains 1 shift/reduce conflicts.	SAVE . word_or_space
State 69 contains 2 shift/reduce conflicts.	DELETE . '!'
State 146 contains 2 reduce/reduce conflicts.   '-' . expr
State 149 contains 1 shift/reduce conflict.	ANGLE expr . '!' ...
State 153 contains 1 shift/reduce conflict.	expr . '!' ...
State 162 contains 1 shift/reduce conflict.	EXPAND expr . '!' ...
State 171 contains 1 shift/reduce conflict.	LEVELS expr . '!' ...
State 207 contains 2 shift/reduce conflicts.	GRID . integer_or_space ...
State 404 contains 2 shift/reduce conflict.	expr . [ IF ... ]
State 418 contains 2 reduce/reduce conflicts.   WORD ( WORD . )
State 500 contains 1 shift/reduce conflict.	plot pexpr pexpr . [ IF ... ]
State 578 contains 1 shift/reduce conflict.	SURFACE INTEGER .
State 593 contains 2 reduce/reduce conflicts.   WORD ( arg_list , WORD . )
 */
%}

%start line				/* parse a complete line */

%pure_parser				/* create a reentrant parser */

%token <charval>     WORD STRING	/* WORD must be first token */
%token <floatval>    _FLOAT
%token <intval>      INTEGER
%token					/* commands */
	ABORT APROPOS BREAK CHDIR DEFINE DELETE
	DO EDIT ELSE FOREACH HELP HISTORY IF KEY LIST LOCAL MACRO OVERLOAD
	PRINT PROMPT QUIT READ RESTORE SAVE SET SNARK
	STANDARD TERMTYPE VERBOSE WHILE WRITE
%token					/* SM */
	ANGLE ASPECT AXIS BOX CONNECT CONTOUR CTYPE CURSOR DATA DEVICE DOT
	DRAW DITHER ERASE ERRORBAR EXPAND FORMAT GRID HISTOGRAM
	IMAGE LABEL LEVELS LIMITS LINES LOCATION LTYPE LWEIGHT MINMAX
	NOTATION PAGE POINTS PTYPE PUTLABEL RANGE RELOCATE RETURN SHADE
	SORT SPLINE SURFACE TABLE TICKSIZE USER VIEWPOINT WHATIS WINDOW
	XLABEL YLABEL
%token					/* Miscellaneous */
	FLOAT META OLD ROW
%token					/* Arithmetic */
	ABS ACOS ASIN ATAN ATAN2 CONCAT COS DIMEN EXP FFT GAMMA INT LG LN
	PI RANDOM SIN SQRT SUM TAN
        LEFT_SHIFT_SYMBOL RIGHT_SHIFT_SYMBOL /* these are written << and >> */
	POWER_SYMBOL			/* this token is entered as "**" */
%token					/* string operators */
	ATOF INDEX LENGTH SPRINTF STRLEN SUBSTR
					/* Precedences, lowest first */
%right  '?' ':'
%left   OR
%left   AND
%left	'|'				/* as in || */
%left	'&'				/* as in && */
%left	'=' '!' '<' '>'			/* as in ==, != etc. */
%left	CONCAT
%left	LEFT_SHIFT_SYMBOL RIGHT_SHIFT_SYMBOL /* << and >> operators */
%left	'+' '-'
%left	'*' '/' '%'
%left	UNARY				/* declare unary operators, - and ! */
%left	POWER_SYMBOL			/* ** operator */

%type <charval>   macro_narg number_or_word something string string_or_space
		  word_or_space
%type <floatval>  number optional s_expr
%type <intval>    exclamation_or_space fill integer integer_or_space plot
		  plus_or_space sign
%type <pairval>   limit_pair
%type <strval>	  delim_str_list prompt str_list
%type <t_list>    delim_list for_list list arg_list
%type <vector>    expr pexpr set_expr

%%					/* start of rules */

line  :
         {
#if defined(YYFAIL)
#  undef  YYERROR			/* work around for Bison bug */
#  define YYERROR YYFAIL		/* must be here, not in preamble */
#endif
	    start_line();
	 }		/* start a new line in tokens.c */
      | line command
      ;

command : '\n'
         {
	    fflush(stdout); fflush(stderr);
	    if(buff_is_empty()) {	/* no characters on input buffer */
	       if(graph_mode) {
		  IDLE();		/* out of graphics mode */
		  graph_mode = 0;
	       }
	       return(1);
	    }
	    start_line();		/* start a new line in tokens.c */
	 }
      | '!'
         {
	    char cmd[200];		/* command to execute */
	    char *shell;		/* the shell to use */

	    reset_term();
	    (void)mgets(cmd,MACROSIZE);	/* read rest of line */
	    if(*(shell = print_var("SHELL")) != '\0' &&
						strcmp(shell,"/bin/sh") != 0) {
	       sprintf(buff,"%s -c \"%s\"",shell,cmd);
	       set_exit_status(system(buff));
	    } else {
	       set_exit_status(system(cmd));
	    }
	    set_term();
	 }
      | ABORT				/* abort a plot */
	 {
	    abort_plot = 1;
	    if(graph_mode) { IDLE(); graph_mode = 0; }
	    CLOSE(); stg_close();
	    devnum = NODEVICE;
	    SETUP("");
	    if(in_graphics) { ENABLE(); graph_mode = 1; }
	    set_dev();
	    abort_plot = 0;
	 }
      | ANGLE expr
	 {
	    vec_convert_float(&$2);
	    set_angle($2.vec.f,$2.dimen);
	    vec_free(&$2);
	 }
      | APROPOS 		/* list macros matching WORD */
         {
	    (void)mgets(buff,CHARMAX);	/* read rest of line */
	    help_apropos(&buff[1]);
	    macro_apropos(&buff[1]);
	 }
      | ASPECT s_expr
	 { set_aspect( $2 ); }
      | CHDIR WORD
	 {
	    if($2[0] == '~') {
	       if(getenv("HOME") == NULL) {
		  msg("can't expand ~  : HOME is not defined\n");
		  (void)strcpy(buff,$2);
	       }
	       (void)sprintf(buff,"%s%s",getenv("HOME"),&$2[1]);
	    } else {
	       (void)strcpy(buff,$2);
	    }
#if 0 && defined(VMS)			/* let's see if this works */
	    msg("Chdir is not currently provided, due to a VMS bug\n");
	    break;
#endif
#if defined(__MSDOS__)
	    if(buff[1] == ':') {
	       setdisk(isupper(buff[0]) ? tolower(buff[0]) : buff[0]);
	       if(buff[2] == '\0') break;
	    }
#endif
	    if(chdir(buff) != 0) {
	       perror("Error in CHDIR ");
	    }
	 }
      | CURSOR '\n'			/* n.b. not in graphics */
	 {
	    int key;

	    if(sm_verbose) {
	       msg("Hit 'e' to exit, 'q' to quit\n");
	    }
	    key = sm_cursor(0);
	    sm_disable();
            (void)unput('\n');

	    if(key == 'e') {
	       (void)sprintf(buff,"relocate %g %g",
			     (xp - ffx)/fsx, (yp - ffy)/fsy);
	       replace_last_command(buff);
	    } else {
	       delete_last_history(1);
	    }
	 }
      | CURSOR READ			/* n.b. not in graphics */
	 {
	    sm_cursor(-1);
	    sm_disable();
	 }
      | CURSOR WORD WORD		/* n.b. not in graphics */
	 {
	    if(sm_verbose) {
	       msg_2s("%s%s\n", "Enter a `e' or `m' to mark a point, ",
		      "`p' to mark a point and exit, `q' to quit");
	    }
	    set_cursor_vectors($2,$3,(char *)NULL);
	    sm_disable();
	 }
      | DATA WORD
         {
#if defined(HAVE_ACCESS)
	    if(access($2, R_OK) != 0) {
	       msg_1s("Denied access to open %s\n",$2);
	    }
#else
	    FILE *fil;

	    if((fil = fopen($2,"r")) == NULL) {
	       msg_1s("Can't open %s\n",$2);
	    } else {
	       (void)fclose(fil);
	    }
#endif
	    (void)strncpy(data_file,$2,CHARMAX); /* the file to read data */
	    line_1 = line_2 = 0;	/* no lines specified */
	    subtable = -1;		/* we aren't reading a table */
	    set_data_file($2);
	 }
      | DEFINE variable variable_value
         { ; }
      | LOCAL DEFINE variable { if(variable_name[0] != '\0') make_local_variable(variable_name); } variable_value
         { ; }
      | DELETE				/* delete last history command */
         {
	    delete_last_history(1);	/* forget this command */
	    delete_last_history(1);	/* and the last one too */
	 }
      | DELETE integer			/* delete command from history */
         { delete_history($2,$2,1); }
      | DELETE integer integer		/* delete commands from history */
         { delete_history($2,$3,1); }
      | DELETE HISTORY			/* delete last history command */
         { delete_last_history(0); }
      | DELETE HISTORY '!'		/* really delete it, guaranteed */
         { delete_last_history(1); }
      | DELETE HISTORY integer		/* delete command from history */
         { delete_history($3,$3,0); }
      | DELETE HISTORY integer integer	/* delete commands from history */
         { delete_history($3,$4,0); }
      | DELETE WORD			/* free a vector */
         { (void)free_vector($2); }
      | DEVICE INTEGER			/* Set current device to INTEGER */
	   {
	      if(graph_mode) IDLE();
	      CLOSE(); stg_close();
	      devnum = NODEVICE;

	      (void)mgets(word,CHARMAX); /* read rest of line */
	      if($2 >= 0 && $2 < ndev) {
		 if((*devices[$2].dev_setup)(word) >= 0) {
		    devnum = $2;
		 } else {
		    msg_1s("No such device %s\n",word);
		 }
	      } else {
	         msg_1d("No such device %d\n",$2);
	      }
	      ENABLE(); graph_mode = 1;
	      set_dev();
	   }
      | DEVICE META WORD		/* open/close metafile WORD */
	   {
	      if(strcmp($3,"close") == 0 || strcmp($3,"CLOSE") == 0) {
		 close_metafile();
	      } else {
		 open_metafile($3);
	      }
	   }
      | DEVICE WORD			/* Set current device to WORD */
	   {
    	      int len;

	      if(graph_mode) IDLE();

	      (void)sprintf(buff,"%s",$2);
	      len = strlen(buff);
	      (void)mgets(&buff[len],MACROSIZE - len); /* read rest of line */
	      
	      set_device(buff);

	      ENABLE(); graph_mode = 1;
	      set_dev();
	   }
      | DITHER WORD WORD number number integer
         {
	    if(sm_dither($2, $3, $4, $5, $6) < 0) {
	       break;
	    }
	 }
      | DO variable '=' s_expr ',' s_expr optional '{' str_list '}'
         {
	    if(variable_name[0] == '\0' || $9.start == NULL) {
	       if($9.start != NULL) free($9.start);
	       break;
	    }

	    push_dstack(variable_name,$4,$6,$7); /* set up DO variables */
	    if(next_do() == 0) {	/* set intial value of DO variable */
	       push($9.start,S_DO);	/* push body of DO loop */
	    }
         }
      | EDIT { (void)mscanstr($<charval>$,CHARMAX); } something
	 { define_map($<charval>2,$3); }
      | error				/* error handling state */
         {
	    lexflush();			/* flush lex buffer */
            yyerrok;
            yyclearin;			/* reset the parser */
	    start_line();		/* start a new line in tokens.c */
	    if(graph_mode) {
	       IDLE();
	       graph_mode = 0;
	    }
	    if(recurse_lvl) {
	       if(recurse_lvl > 1) {	/* keep on popping */
		  push("SNARK ",S_NORMAL);
	       }
	       recurse_lvl--;
	       return(1);
	    }
	    if(buff_is_empty()) {	/* no characters on input buffer */
	       return(1);
	    }
         }
      | EXPAND expr
	 {
	    vec_convert_float(&$2);
	    set_expand($2.vec.f,$2.dimen);
	    vec_free(&$2);
	 }
      | FFT number pexpr pexpr WORD WORD
	 {
	    VECTOR *temp_r,*temp_i;
	    
	    if($2 != 1 && $2 != -1) {
	       msg_1f("You must specify 1 or -1 for an FFT, not %g\n",$2);
	       vec_free(&$3);
	       vec_free(&$4);
	       break;
	    }
	    if($3.dimen != $4.dimen) {
	       msg_2s("Vectors %s and %s have different dimensions\n",
		      $3.name,$4.name);
	       vec_free(&$3);
	       vec_free(&$4);
	       break;
	    }
	    if((temp_r = make_vector($5,$3.dimen,VEC_FLOAT)) == NULL) {
	       vec_free(&$3);
	       vec_free(&$4);
	       break;
	    }
	    if((temp_i = make_vector($6,$3.dimen,VEC_FLOAT)) == NULL) {
	       vec_free(&$3);
	       vec_free(&$4);
	       free_vector($5);
	       break;
	    }
	    (void)fft($3.vec.f,$4.vec.f,temp_r->vec.f,temp_i->vec.f,$3.dimen,
								      (int)$2);
	    vec_free(&$3);
	    vec_free(&$4);
	 }
      | FOREACH variable for_list '{' str_list '}'
         {
	    if(variable_name[0] == '\0' || $3 == NULL || $5.start == NULL) {
	       freelist($3);
	       if($5.start != NULL) free($5.start);
	       break;
	    }
	    if($3->nitem <= 0) {	/* Null list */
	       msg_1s("FOREACH %s variable list is empty\n",variable_name);
	       freelist($3);
	       free($5.start);
	       break;
	    }
	    push_fstack(variable_name,$3);
	    (void)next_foreach();
	    push($5.start,S_FOREACH);	/* $3 and $5 will be freed later */
         }
      | FORMAT number_or_word number_or_word
	 { sm_format( $2, $3 ); }
      | FORMAT '\n'
	 {
	    sm_format("1","1");
	    (void)unput('\n');
	 }
      | {
	   in_graphics = 1;
	   if(!graph_mode) { ENABLE(); graph_mode = 1; }
        }
	graphic
	{ in_graphics = 0; GFLUSH(); }
      | HELP				/* HELP! */
         {
	    char *var;
	    int i,j;

	    if(graph_mode) { IDLE(); graph_mode = 0; }
            (void)mgets(word,CHARMAX);	/* will have leading ' ' */
	    for(j = strlen(word) - 1;j >= 0 && isspace(word[j]);j--) {
	       word[j] = '\0';
	    }
	    for(i = 0;i < j && isspace(word[i]);i++) continue;
	    if(word[i] == '\0') {
	       (void)strcpy(word,"help");
	       i = 0;
	    }
            if((j = help(&word[i])) < 0) {	/* in help directory? */
	       break;
	    }
            j += (what_is(&word[i]) == 0);	/* maybe it's a macro */
	    var = print_var(&word[i]);
	    j += (var[0] != '\0');		/* or a variable? */
	    if(var[0] != '\0') {
	       (void)printf("Variable : %s\n",var);
	    }
	    j += (help_vector(&word[i]) == 0);	/* maybe a vector? */
	    if(j == 0) {			/* nothing we know about */
	       (void)fprintf(stderr,"I'm sorry %s, %s\n",user_name,
					  "but I can't help you with that.");
            }
	    fflush(stdout);
         }
      | HISTORY				/* list history commands */
         { history_list(1); }
      | HISTORY	'-'			/* list history commands backwards */
         { history_list(-1); }
      | IF '(' s_expr ')' '{' str_list '}' '\n'
	 {
	    (void)unput('\n');
	    if($6.start == NULL) break;
	    if($3) {
	       push($6.start,S_TEMP);
	    } else {
	       free($6.start);
	    }
	 }
      | IF '(' s_expr ')' '{' str_list '}' ELSE '{' str_list '}'
	 {
	    char *if_body;
	    
	    if($6.start == NULL) {
	       if($10.start != NULL) free($10.start);
	       break;
	    }
	    if($10.start == NULL) {
	       free($6.start);
	       break;
	    }
	    
	    if($3) {
	       free($10.start);
	       if_body = $6.start;
	    } else {
	       free($6.start);
	       if_body = $10.start;
	    }

	    push(if_body,S_TEMP);
	 }
      | IMAGE CURSOR '\n'		/* this mustn't be in `graphics' */
         {
	    sm_cursor(1);
	    sm_disable();
            (void)unput('\n');
	 }
      | IMAGE CURSOR WORD WORD WORD		/* n.b. not in graphics */
	 {
	    set_cursor_vectors($3,$4,$5);
	    sm_disable();
            (void)unput('\n');
	 }
      | IMAGE DEFINE variable number_or_word /* set a variable */
         { if(variable_name[0] != '\0') set_image_variable(variable_name,$4); }
      | IMAGE DELETE
	 { (void)sm_delimage(); }
      | IMAGE integer_or_space WORD
	 { (void)read_image($3,$2,0.0,0.0,0.0,0.0, -1, -1, -1, -1); }
      | IMAGE integer_or_space WORD '[' '*' ':' '*' ']'
	 { (void)read_image($3,$2,0.0,0.0,0.0,0.0, -1, -1, -1, -1); }
      | IMAGE integer_or_space WORD '[' INTEGER ',' INTEGER ':' '*' ']'
	 { (void)read_image($3,$2,0.0,0.0,0.0,0.0, $5, $7, -1, -1); }
      | IMAGE integer_or_space WORD '[' '*' ':' INTEGER ',' INTEGER ']'
	 { (void)read_image($3,$2,0.0,0.0,0.0,0.0, -1, -1, $7, $9); }
      | IMAGE integer_or_space WORD '[' INTEGER ',' INTEGER ':' INTEGER ',' INTEGER ']'
	 { (void)read_image($3,$2,0.0,0.0,0.0,0.0, $5, $7, $9, $11); }
      | IMAGE integer_or_space WORD number number number number
	 { (void)read_image($3,$2,$4,$5,$6,$7, -1,-1,-1,-1); }
      | IMAGE '(' number ',' number ')'
	 { create_image((int)($3 + 1e-5),(int)($5 + 1e-5),
			0.0,(float)$3 - 1,0.0,(float)$5 - 1); }
      | IMAGE '(' number ',' number ')' number number number number
	 { create_image((int)($3 + 1e-5),(int)($5 + 1e-5),$7,$8,$9,$10); }
      | LEVELS expr			/* levels for contouring */
         {
	    vec_convert_float(&$2);
	    sm_levels($2.vec.f,(int)$2.dimen);
	    vec_free(&$2);
         }
      | LIMITS { xy_range = x_range; } limit_pair { xy_range = y_range; } limit_pair
	 { (void)sm_limits( $3.a, $3.b, $5.a, $5.b ); }
      | KEY			/* Define a key to execute a command */
  	 { key_macro(); }
      | LINES INTEGER INTEGER
         {
	    line_1 = $2;
	    line_2 = $3;
	    sprintf(buff,"%d",line_1); setvar("_l1",buff);
	    sprintf(buff,"%d",line_2); setvar("_l2",buff);
	 }
      | LIST { disable_overload(); } something_to_list { allow_overload(); }
      | LOCATION INTEGER INTEGER INTEGER INTEGER
	 { (void)sm_location( $2, $3, $4, $5); }
      | LWEIGHT number
	 {
	    if($2 < 0) {
	       msg("LWEIGHT cannot be negative; setting to zero\n");
	       $2 = 0;
	    }
	    sm_lweight($2);
	 }
      | MACRO WORD macro_narg delim_str_list /* define macro */
         {
	    if($4.start == NULL) break;
            *($4.end) = '\0';		/* remove '\n' */

            (void)define_s($2,$4.start,$3);

            free($4.start);
         }
      | MACRO WORD DELETE		/* delete a macro */
	 { (void)define($2,"delete",0,0,0); }
      | MACRO WORD integer integer	/* define macro from history */
         { (void)define_history_macro($2,$3,$4);}
      | MACRO EDIT WORD			/* edit macro WORD */
         { (void)macro_edit($3); }
      | MACRO READ WORD			/* read macros from file WORD */
         {
	    FILE *fil;

	    if((fil = fopen($3,"r")) == NULL) {
	       msg_1s("can't open %s\n",$3);
	       break;
	    }
	    (void)read_macros(fil);
	    (void)fclose(fil);
	 }
      | MACRO DELETE WORD		/* delete macros in file WORD */
         { (void)undefine_macros($3); }
      | MACRO WRITE WORD '\n'		/* write all macros to file WORD */
         {
	    FILE *fil;

            (void)unput('\n');
	    if(would_clobber($3)) {
	       break;
	    }
	    if((fil = fopen($3,"w")) == NULL) {
	       msg_1s("can't open %s\n",$3);
	       break;
	    }
            (void)write_macros(fil,1);	/* write all macros */
	    (void)fclose(fil);
         }
      | MACRO WRITE WORD plus_or_space WORD	/* write word1 to file word2 */
	 { (void)write_one_macro($3,$5,($4 == '+')); }
      | META READ WORD
	 { playback_metafile($3); }
      | MINMAX WORD WORD
	 {
	    float min,max;

	    sm_minmax(&min,&max);
	    (void)sprintf(buff,"%.10g",min);
	    setvar($2,buff);
	    (void)sprintf(buff,"%.10g",max);
	    setvar($3,buff);
	 }
      | NOTATION number number number number
	 { sm_notation( $2, $3, $4, $5 ); }
      | OVERLOAD variable integer	/* allow "variable" to be macro name */
	 { overload(variable_name,$3); }
      | PRINT plus_or_space word_or_space string_or_space delim_list
         {
	    int append = ($2 == '+');  
	    int dimen = 0,i,nvec;
	    VECTOR **temp;

	    if(!append && would_clobber($3)) {
	       break;
	    }
	    if(graph_mode) { graph_mode = 0; IDLE(); }

	    nvec = $5->nitem;
	    if((temp = (VECTOR **)malloc(nvec*sizeof(VECTOR *))) == NULL) {
	       msg("Can't allocate storage for temp\n");
	       break;
	    }

	    for(i = 0;i < nvec && !sm_interrupt;i++) {	/* get vectors */
	       if((temp[i] = get_vector_ptr($5->i_list[i]->str)) == NULL) {
		  break;
	       }
	       if(i == 0) {
		  dimen = temp[0]->dimen;
	       } else if(temp[i]->dimen != dimen) {
		  if(sm_verbose > 1)
		     msg("Vectors are different sizes\n");
		  if(dimen < temp[i]->dimen) {
		     dimen = temp[i]->dimen;
		  }
	       }
	    }
	    if(i < nvec) {
	       free((char *)temp);
               freelist($5);
	       break;
	    }

	    print_vec($3,append ? "a" : "w",$4,nvec,temp,dimen);
	    
	    free((char *)temp);
            freelist($5);
	 }
      | PROMPT				/* change prompt */
         {
	    int i;

	    (void)mgets(buff,100);
	    for(plength = 0,i = 0;buff[i + 1] != '\0';i++) {
	       prompt[i] = buff[i + 1];
	       if(prompt[i] == '*') prompt[i] = '\007'; /* replace * with ^G */
	       if(isprint(prompt[i])) plength++;
	    }
	    plength++;			/* allow for ' ' */
	    prompt[i] = '\0';
         }
      | PTYPE INTEGER INTEGER
	 {
	    REAL pstyle[1];

	    pstyle[0] = 10*$2 + $3;
	    sm_ptype( pstyle, 1 );
	 }
      | PTYPE WORD
	 {
	    VECTOR *tptr;

	    if((tptr = get_vector_ptr($2)) == NULL) {
	       break;
	    }
	    switch (tptr->type) {
	     case VEC_FLOAT:
	       sm_ptype(tptr->vec.f,tptr->dimen);
	       break;
	     case VEC_LONG:
	       vec_convert_float(tptr);
	       sm_ptype(tptr->vec.f,tptr->dimen);
	       break;
	     case VEC_STRING:
	       ptype_str(tptr->name,tptr->vec.s.s_s,tptr->dimen);
	       break;
	     default:
	       msg_1s("%s is an ",$2);
	       msg_1d("unknown type of vector: %d\n",tptr->type);
	       break;
	    }
	 }
      | PTYPE '(' expr ')'
	 {
	    if($3.type == VEC_STRING) {
	       msg_1s("%s is not an arithmetic expression\n",$3.name);
	       break;
	    }
	    
	    vec_convert_float(&$3);
	    sm_ptype( $3.vec.f, $3.dimen );
	    vec_free(&$3);
	 }
      | PTYPE delim_list		/* define symbol for points */
	 {
	    if($2 == NULL) break;
	    if($2->nitem <= 0) {	/* Null symbol list */
	       msg("Symbol definition list is empty\n");
	       freelist($2);
	       break;
	    }	       

	    setsym($2);
	    freelist($2);
	 }
      | QUIT
	 {
	    if(graph_mode) IDLE();
	    CLOSE(); stg_close();
	    return(-1);
	 }				/* NOTREACHED */
      | RANGE number number
	 {
	    x_range = $2;
	    y_range = $3;
	 }
      | READ EDIT WORD			/* Read a set of keymaps */
         { (void)read_map($3); }
      | READ OLD WORD WORD		/* Read a Mongo file */
         { read_old($3,$4); }
      | READ ROW WORD number_or_word	/* read a row from a file */
         {
	    int col;
	    VECTOR temp;

	    col = atoi($4);
	    temp.type = parse_qualifier($4);
	    temp.name = $3;
	    if(read_row(data_file,col,&temp) == 0) {
	       copy_vector(temp.name,temp);
	    }
	 }
      | READ WORD number_or_word	/* read a column from a file */
         {
	    int col;
	    VECTOR *tptr,temp;

	    tptr = &temp;
	    col = atoi($3);
	    temp.name = $2;
	    temp.type = parse_qualifier($3);
	    if(read_column(0,data_file,&col,&tptr,1,line_1,line_2,"") == 1) {
	       copy_vector(temp.name,temp);
	    }
	 }
      | READ exclamation_or_space delim_list /* read columns from a file */
         {
	    int *cols,			/* desired columns */
	        extra,			/* extra i_list items processed */
	        i,j,k,
	        ncol,nvec,
	        n1,n2,			/* range of columns for a vector */
	        size_cols;		/* size of cols, temp */
	    VECTOR **temp, *temp_s;	/* temporary vectors */

	    $2 = ($2 == '!') ? 1 : 0;
	    if($3 == NULL) break;
	    ncol = $3->nitem;

	    size_cols = ncol;		/* initially assume no ranges */
	    if((cols = (int *)malloc(size_cols*sizeof(int))) == NULL) {
	       msg("Can't allocate space for columns list\n");
	       break;
	    }
	    if((temp = (VECTOR **)malloc(size_cols*sizeof(VECTOR *))) == NULL){
	       msg("Can't allocate storage for temp\n");
	       free((char *)cols);
	       break;
	    }
	    if((temp_s = (VECTOR *)malloc(size_cols*sizeof(VECTOR))) == NULL){
	       msg("Can't allocate storage for temp_s\n");
	       free((char *)temp);
	       free((char *)cols);
	       break;
	    }
	    for(i = 0;i < size_cols;i++) {
	       temp[i] = temp_s + i;
	       temp[i]->name = temp[i]->descrip + 1;
	    }

	    for(i = j = 0;i < ncol;i += 2 + extra) {
	       if(i + 1 == ncol) {
		  msg_1s("Ignoring extra field at end of READ list: %s\n",
			 $3->i_list[i]->str);
		  break;
	       }
	       extra = 0;
	       if(strchr($3->i_list[i + 1]->str,'-') != NULL) {
		  char *ptr = $3->i_list[i + 1]->str;
		  if(*ptr == '-') {	/* a single -ve number */
		     n1 = n2 = atoi(ptr);
		  } else {
		     n1 = atoi(ptr); while(*ptr != '-') ptr++;
		     ptr++;
		     if(isdigit(*ptr)) {
			n2 = atoi(ptr);
		     } else {
			msg_1s("Malformed column specification: %s\n",
			       $3->i_list[i + 1]->str);
			n2 = 0;
		     }
		  }
	       } else if(i + 2 < ncol && $3->i_list[i + 2]->str[0] == '-') {
		  n1 = atoi($3->i_list[i + 1]->str);
		  n2 = atoi($3->i_list[i + 2]->str + 1);
		  extra = 1;		/* we parsed an extra field */
	       } else {
		  n1 = n2 = atoi($3->i_list[i + 1]->str);
	       }
	       
	       for(k = n1;k <= n2;k++) {
		  if(j >= size_cols) {	/* we need more space */
		     int l;
		     
		     if(sm_verbose > 2) {
			msg("Reallocating space in READ { ... }\n");
		     }
		     size_cols += 10;
		     if((cols = (int *)realloc((char *)cols,
					     size_cols*sizeof(int))) == NULL) {
			msg("Can't allocate space for columns list\n");
			free((char *)temp_s); free((char *)temp);
			break;
		     }
		     if((temp_s = (VECTOR *)realloc((char *)temp_s,
			   		  size_cols*sizeof(VECTOR))) == NULL) {
			msg("Can't reallocate storage for temp_s\n");
			free((char *)temp);
			free((char *)cols);
			cols = NULL;
			break;
		     }
		     free((char *)temp);
		     if((temp = (VECTOR **)malloc(size_cols*sizeof(VECTOR *)))
								     == NULL) {
			msg("Can't reallocate storage for temp\n");
			free((char *)cols);
			free((char *)temp_s);
			cols = NULL;
			break;
		     }
		     for(l = 0;l < size_cols;l++) {
			temp[l] = temp_s + l;
			temp[l]->name = temp[l]->descrip + 1;
		     }
		  }

		  temp[j]->type = parse_qualifier($3->i_list[i + 1]->str);
		  if(n1 == n2) {
		     sprintf(temp[j]->descrip,"\001%s",$3->i_list[i]->str);
		  } else {
		     if(temp[j]->type != VEC_FLOAT) {
			msg_1s("Multi-column vectors must be numeric (%s)\n",
			       $3->i_list[i]->str);
			continue;
		     }
		     if(k == n1) {
			sprintf(temp[j]->descrip,"\001+%s",$3->i_list[i]->str);
		     } else {
			sprintf(temp[j]->descrip,"\001|%s%d",
				$3->i_list[i]->str,k - n1);
		     }
		  }
		  cols[j++] = k;
	       }
	       if(cols == NULL) {	/* realloc failed */
		  break;
	       }
	    }
	    freelist($3);
	    if(cols == NULL) {		/* realloc failed */
	       break;
	    }

	    nvec = read_column($2,data_file,cols,temp,j,line_1,line_2,"");
	    free((char *)cols);

	    for(i = 0;i < nvec;i++) {
	       if(*temp[i]->name != '+') {
		  copy_vector(temp[i]->name,*temp[i]);
	       } else {
		  int nmerge;
		  VECTOR merged;

		  for(j = i + 1;j < nvec && *temp[j]->name == '|';j++)continue;
		  nmerge = j - i;
		  merged.type = temp[i]->type;
		  
		  if(vec_malloc(&merged,nmerge*temp[i]->dimen) != 0) {
		     msg_1s("Can't get space for %s\n",temp[i]->name + 1);
		  } else {
		     for(j = 0;j < nmerge;j++) {
			if(merged.type == VEC_FLOAT) {
			   for(k = 0;k < temp[i]->dimen;k++) {
			      merged.vec.f[j + nmerge*k] =
							 temp[i + j]->vec.f[k];
			   }
			} else if(merged.type == VEC_LONG) {
			   for(k = 0;k < temp[i]->dimen;k++) {
			      merged.vec.l[j + nmerge*k] =
							 temp[i + j]->vec.l[k];
			   }
			} else if(merged.type == VEC_STRING) {
			   for(k = 0;k < temp[i]->dimen;k++) {
			      strcpy(merged.vec.s.s[j + nmerge*k],
						      temp[i + j]->vec.s.s[k]);
			   }
			}
		     }
		     copy_vector(temp[i]->name + 1,merged);
		  }
		  
		  vec_free(temp[i++]);
		  while(i < nvec && *temp[i]->name == '|') {
		     vec_free(temp[i]);
		  }
	       }
	    }
	    free((char *)temp_s);
	    free((char *)temp);
	 }
      | READ exclamation_or_space string delim_list /* read data from a file */
         {
	    int i,*cols,nvec;
	    int null_fmt = ($3[0] == '\0') ? 1 : 0;
	    char *ptr;
	    VECTOR **vecs, *s_vecs;

	    $2 = ($2 == '!') ? 1 : 0;
	    if($4 == NULL) break;

	    if((vecs = (VECTOR **)malloc($4->nitem*sizeof(VECTOR *)))== NULL ||
	       (s_vecs = (VECTOR *)malloc($4->nitem*sizeof(VECTOR))) == NULL) {
	       msg("Can't allocate storage for vecs in READ string { ... }\n");
	       if(vecs != NULL) free((char *)vecs);
	       freelist($4);
	       break;
	    }
	    nvec = 0;
	    for(i = 0;i < $4->nitem;i++) {
	       int vec_type = null_fmt ? VEC_FLOAT : VEC_NULL;
	       if(null_fmt && strcmp($4->i_list[i]->str, ".") == 0) {
		  vecs[nvec++] = NULL; /* skip this column */
		  continue;
	       } else if(isdigit($4->i_list[i]->str[0])) {
		  msg_1s("%s isn't a valid name for a vector\n",$4->i_list[i]->str);
		  continue;
	       }
	       if((ptr = strchr($4->i_list[i]->str, '.')) != NULL) {
		  if(null_fmt) {
		     vec_type = parse_qualifier(ptr);
		     *ptr = '\0';
		  } else {
		     msg_1s("%s isn't a valid name for a vector\n",
			    $4->i_list[i]->str);
		     continue;
		  }
	       }
	       
	       vecs[nvec] = s_vecs + nvec;
	       vecs[nvec++]->name = $4->i_list[i]->str; /* type will come from fmt*/
	       if(null_fmt) {
		  vecs[i]->type = vec_type;
	       }
	    }

	    if((cols = (int *)malloc(nvec*sizeof(int))) == NULL) {
	       msg("Can't allocate space for columns list\n");
	       break;
	    }
/*
 * Special format '' reads all specified vectors; columns to be skipped
 * are specified as ., which has become a NULL vector at this point
 */
	    if(null_fmt) {
	       int ii = 0;
	       for(i = 0; i < nvec; i++) {
		  if(vecs[i] != NULL) {
		     cols[ii] = i + 1;
		     vecs[ii++] = vecs[i];
		  }
	       }
	       nvec = ii;
	    } else {
	       cols[0] = 1;		/* not 0; used by read_column() */
	    }
	    nvec = read_column($2,data_file,cols,vecs,nvec,line_1,line_2,$3);

	    for(i = 0;i < nvec;i++) {
	       copy_vector(vecs[i]->name,*vecs[i]);
	    }
	    free((char *)cols); free((char *)vecs); free((char *)s_vecs);
            freelist($4);
	 }
      | READ TABLE string_or_space delim_list /* read data from a table */
         {
	    int i, j;
	    int nvec;			/* number of vector's names */
	    int nextra;			/* number of extra vectors implied
					   by names like sky[0-3] */
	    VECTOR **vecs, *s_vecs;

	    if($4 == NULL) break;

	    nextra = nvec = 0;
	    if(strcmp($3,"bycolumn") == 0) { /* a list of WORD id ... */
	       for(i = 0;i < $4->nitem - 1;i += 2) {
		  if(isdigit($4->i_list[i]->str[0])) {
		     msg_1s("%s isn't a valid name for a vector\n",
			    $4->i_list[i]->str);
		  } else {
		     nvec++;
		  }
	       }
	       if(i != $4->nitem) {
		  msg_1s("Ignoring junk at end of READ TABLE list: %s\n",
			 $4->i_list[$4->nitem - 1]->str);
	       }
	    } else {
	       for(i = 0;i < $4->nitem;i++) {
		  if(isdigit($4->i_list[i]->str[0])) {
		     msg_1s("%s isn't a valid name for a vector\n",
								$4->i_list[i]->str);
		  } else {
		     char *val0;	/* pointer to "0" in "name[0-1]" */
		     char *val1;	/* pointer to "1" in "name[0-1]" */
		     if((val0 = strchr($4->i_list[i]->str,'[')) != NULL) {
			val1 = ++val0;
			if(*val1 == '-') val1++;
			while(isdigit(*val1)) val1++;
			if(*val1 == '-') {
			   val1++;
			   if(isdigit(*val1)) {	/* OK, a range */
			      int n = atoi(val1) - atoi(val0);
			      if(n < 0 || nextra > 100) {
				 msg_1d("I don't believe that you want %d",
								   n + 1);
				 msg_1s(" vectors from %s\n",$4->i_list[i]->str);
			      } else {
				 nextra += n;
			      }
			   }
			}
		     }
		     nvec++;
		  }
	       }
	    }

	    if((vecs = (VECTOR **)malloc((nvec + nextra)*sizeof(VECTOR *))) ==
									NULL ||
	       (s_vecs = (VECTOR *)malloc((nvec + nextra)*sizeof(VECTOR))) ==
									NULL) {
	       msg("Can't allocate storage for vecs in READ TABLE {...}\n");
	       if(vecs != NULL) free((char *)vecs);
	       freelist($4);
	       break;
	    }
	    for(j = 0;j < nvec + nextra;j++) {
	       vecs[j] = s_vecs + j;
	       vecs[j]->name = vecs[j]->descrip; /* use descrip as scratch */
	       *vecs[j]->name = '\0';
	    }
	    
	    if(strcmp($3,"bycolumn") == 0) { /* a list of WORD id ... */
	       for(i = j = 0;i < $4->nitem - 1;i += 2) {
		  if(isdigit($4->i_list[i]->str[0])) {
		     continue;
		  }
		  vecs[j]->type = 0;	/* real type will come from table */
		  strncpy(vecs[j]->descrip,$4->i_list[i]->str,VEC_DESCRIP_SIZE);
		  j++;
	       }
	    } else {
	       for(i = j = 0;i < $4->nitem;i++) {
		  if(isdigit($4->i_list[i]->str[0])) { /* we already warned them */
		     continue;
		  }
		  vecs[j]->type = 0;	/* real type will come from table */
		  strncpy(vecs[j]->descrip,$4->i_list[i]->str,VEC_DESCRIP_SIZE);
		  j++;
	       }
	    }

	    nvec = read_table(data_file, subtable, vecs, nvec, nextra,
			      line_1, line_2, $3, table_fmt);
	    for(i = 0;i < nvec;i++) {
	       copy_vector(vecs[i]->name, *vecs[i]);
	    }

	    free((char *)vecs); free((char *)s_vecs);
            freelist($4);
	 }
      | RESTORE word_or_space
	 { restore($2); }
      | RETURN
	 { mac_return(); }
      | SAVE word_or_space
	 { save($2,0); }
      | SET DIMEN '(' WORD ')' '=' number_or_word
	 {
	    int dimension,i;
	    VECTOR *temp;

	    dimension = (int)(atof($7) + 0.5);
	    i = parse_qualifier($7);
	    if((temp = make_vector($4,dimension,i)) != NULL) {
	       if(temp->type == VEC_FLOAT) {
		  for(i = 0;i < dimension;i++) {
		     temp->vec.f[i] = 0;
		  }
	       } else if(temp->type == VEC_LONG) {
		  for(i = 0;i < dimension;i++) {
		     temp->vec.l[i] = 0;
		  }
	       } else {
		  for(i = 0;i < dimension;i++) {
		     temp->vec.s.s[i][0] = '\0';
		  }
	       }
	    }
	 }
      | SET HELP WORD			/* set a help string for a vector */
	 {
	    if(*mgets(word,CHARMAX) != '\0') {
	       set_help_vector($3,&word[1]);
	    }
	 }
      | SET IMAGE '(' expr ',' expr ')' '=' expr
	 {
	    if(sm_verbose) {
	       msg("\"set image(expr,expr) = expr\" is obsolete; ");
	       msg("please say \"image[expr,expr]\"\n");
	    }
	    vec_set_image_from_index(&$4,&$6,&$9);
	 }
      | SET IMAGE '[' expr ',' expr ']' '=' expr
	 { vec_set_image_from_index(&$4,&$6,&$9); }
      | SET IMAGE '[' '*' ',' expr ']' '=' expr
	 { vec_set_image_from_index(NULL,&$6,&$9); }
      | SET IMAGE '[' expr ',' '*' ']' '=' expr
	 { vec_set_image_from_index(&$4,NULL,&$9); }
      | SET IMAGE '[' '*' ',' '*' ']' '=' expr
	 { vec_set_image_from_index(NULL,NULL,&$9); }
      | SET RANDOM number		/* initialise random numbers */
	 { set_random((long)$3); }
      | SET WORD LOCAL			/* make a vector local */
         { make_local_vector($2); }
      | SET WORD '=' set_expr		/* assignment */
	 { (void)copy_vector($2,$4); }
| LOCAL SET WORD { make_local_vector($3); strcpy($<charval>$, $3); } '=' set_expr
         {
	    (void)copy_vector($3,$6);
	    assert(&$4 == &$4);		/* stop yacc complaining about $4 not being used */
	 }
      | SET WORD '[' expr ']' '=' expr
         {
	    VECTOR *x;

	    if((x = get_vector_ptr($2)) == NULL) {
	       vec_free(&$4); vec_free(&$7);
	       break;
	    }
	    vec_subscript(x,&$4,&$7);
	 }
      | SET WORD '[' s_expr ',' s_expr ']' '=' expr
         {
	    VECTOR ind;			/* indices */
	    VECTOR *x;			/* vector $2 */

	    if(make_anon_vector(&ind, 0, VEC_LONG) != 0 ||
						 vec_do(&ind, $4, $6, 1) < 0) {
	       vec_free(&$9);
	       break;
	    }
	    vec_convert_long(&ind);
	    
	    if((x = get_vector_ptr($2)) == NULL) {
	       vec_free(&ind); vec_free(&$9);
	       break;
	    }
	    if(x->dimen <= ind.vec.l[ind.dimen-1]) {
	       if(vec_realloc(x, ind.vec.l[ind.dimen-1] + 1) < 0) {
		  msg_1s("Can't extend %s\n", x->name);
		  vec_free(&ind); vec_free(&$9);
		  break;
	       }
	    }
	    vec_subscript(x, &ind, &$9);

	    vec_free(&ind); vec_free(&$9);
	 }
      | SNARK				/* from my very first yacc parser */
         {
	    int c;
	    
	    if((c = input()) != ' ') unput(c);
	    if(sm_verbose > 3) msg("Just the place for a snark!\n");
	    if(recurse_lvl == 0) {
	       break;
	    }
	    if(sm_interrupt && recurse_lvl > 1) {	/* keep on popping */
	       int save_interrupt = sm_interrupt;
	       sm_interrupt = 0;
	       start_line();		/* start a new line in tokens.c */
	       push("SNARK ",S_NORMAL);
	       sm_interrupt = save_interrupt;
	    }
	    recurse_lvl--;
	    return(1);
	 }
      | SORT delim_list
         {
	    int dimen = 0,
	    	i,k,nvec,
	        n_float = 0,		/* number of float vectors */
	        n_long = 0,		/* number of long vectors */
	        n_string = 0,		/* number of string vectors */
		*sorted;		/* sorted indices */
	    VECTOR **temp;
	    VECTOR temp_f,temp_l,temp_s; /* temp vectors for rearranging */

	    if((nvec = $2->nitem) == 0) {
	       freelist($2);
	    } else {
	       if((temp = malloc(nvec*sizeof(VECTOR *))) == NULL) {
		  msg("malloc returns NULL for SORT\n");
		  break;
	       }
	    }
	    

	    for(i = 0;i < nvec && !sm_interrupt;i++) {	/* get vectors */
	       if((temp[i] = get_vector_ptr($2->i_list[i]->str)) == NULL) {
		  break;
	       }

	       if(temp[i]->type == VEC_FLOAT) {
		  n_float++;
	       } else if(temp[i]->type == VEC_LONG) {
		  n_long++;
	       } else if(temp[i]->type == VEC_STRING) {
		  n_string++;
	       } else {
		  msg_1d("Unknown vector type: %d",temp[i]->type);
		  msg_1s(" for vector %s\n",temp[i]->name);
	       }
	       if(i == 0) {
		  dimen = temp[0]->dimen;
	       } else if(temp[i]->dimen != dimen) {
		  msg("Vectors are different sizes\n");
		  break;
	       }
	    }
            freelist($2);
	    if(i < nvec) {
	       free((char *)temp);
	       break;
	    }

	    if(n_float == 1 && n_string == 0 && n_long == 0) { /*special case*/
	       sort_flt(temp[0]->vec.f, dimen);
	       free((char *)temp);
	       break;
	    }
	    
	    if((sorted = (int *)malloc((unsigned)dimen*sizeof(int))) == NULL) {
	       msg("malloc returns NULL for SORT\n");
	       free((char *)temp);
	       break;
	    }
	    if(n_float > 0) {
	       temp_f.type = VEC_FLOAT;
	       if(vec_malloc(&temp_f,dimen) == -1) {
		  msg("malloc returns NULL for SORT\n");
		  free((char *)temp);
		  free((char *)sorted);
		  break;
	       }
	    }
	    if(n_long > 0) {
	       temp_l.type = VEC_LONG;
	       if(vec_malloc(&temp_l,dimen) == -1) {
		  msg("malloc returns NULL for SORT\n");
		  free((char *)temp);
		  free((char *)sorted);
		  vec_free(&temp_f);
		  break;
	       }
	    }
	    if(n_string > 0) {
	       temp_s.type = VEC_STRING;
	       if(vec_malloc(&temp_s,dimen) == -1) {
		  msg("malloc returns NULL for SORT\n");
		  free((char *)temp);
		  free((char *)sorted);
		  vec_free(&temp_l);
		  vec_free(&temp_f);
		  break;
	       }
	    }
	    if(temp[0]->type == VEC_FLOAT) {
	       sort_flt_inds(temp[0]->vec.f,sorted,dimen);
	    } else if(temp[0]->type == VEC_LONG) {
	       sort_long_inds(temp[0]->vec.l,sorted,dimen);
	    } else if(temp[0]->type == VEC_STRING) {
	       sort_str_inds(temp[0]->vec.s.s,sorted,dimen);
	    } else {
	       msg("You can't get here. SORT.\n");
	    }

	    for(k = 0;!sm_interrupt && k < nvec;k++) {	/* for each vector */
	       if(temp[k]->type == VEC_FLOAT) {
		  (void)memcpy((Void *)temp_f.vec.f,
			     (Const Void *)temp[k]->vec.f,dimen*sizeof(REAL));

		  for(i = 0;i < dimen;i++) {		/* restore it sorted */
		     temp[k]->vec.f[i] = temp_f.vec.f[sorted[i]];
		  }
	       } else if(temp[k]->type == VEC_LONG) {
		  (void)memcpy((Void *)temp_l.vec.l,
			     (Const Void *)temp[k]->vec.l,dimen*sizeof(LONG));

		  for(i = 0;i < dimen;i++) {		/* restore it sorted */
		     temp[k]->vec.l[i] = temp_l.vec.l[sorted[i]];
		  }
	       } else {
		  (void)memcpy((Void *)temp_s.vec.s.s_s,
			  (Const Void *)temp[k]->vec.s.s_s,dimen*VEC_STR_SIZE);

		  for(i = 0;i < dimen;i++) {		/* restore it sorted */
		     strcpy(temp[k]->vec.s.s[i],temp_s.vec.s.s[sorted[i]]);
		  }
	       }
	    }

	    free((char *)temp);
	    free((char *)sorted);
	    if(n_float > 0) vec_free(&temp_f);
	    if(n_long > 0) vec_free(&temp_l);
	    if(n_string > 0) vec_free(&temp_s);
	 }
      | SPLINE WORD WORD WORD WORD
         { (void)spline($2,$3,$4,$5); }
      | TABLE DELETE
	 { (void)sm_deltable(); }
      | TABLE integer '\n'
	 {
	    (void)unput('\n');
	    
 	    line_1 = line_2 = 0;	/* no lines specified */
	    subtable = $2;
	 }
      | TABLE integer_or_space string_or_space WORD
         {
	    FILE *fil;
	    
	    line_1 = line_2 = 0;	/* no lines specified */
	    if((fil = fopen($4,"r")) == NULL) {
	       msg_1s("Can't open %s\n",$4);
	    } else {
	       (void)fclose(fil);
	       set_data_file($4);
	       
	       read_table_header($4,$2);
	    }
	    (void)strncpy(data_file,$4,CHARMAX); /* the file to read data */
	    (void)strncpy(table_fmt,$3,CHARMAX);
	    
	    subtable = $2;
	 }
      | TICKSIZE number number number number
	 { sm_ticksize( $2, $3, $4, $5); }	  
      | TERMTYPE WORD integer_or_space
	 { (void)set_term_type($2,$3); }
      | USER ABORT
	 {
	    static int abort_depth = 0;	/* depth of nesting abort handlers */
	    char *ptr;
	    if((ptr = mgets(word,CHARMAX)) == NULL || *ptr == '\0') {
	       ptr = "USER ABORT";
	    } else {
	       ptr++;			/* skip initial space */
	    }

	    if(abort_depth++ == 100) {
	       msg("abort handler depth too great (100 max)\n");
	       abort_depth = 0;
	       start_line();

	       msg_1s("%s\n",ptr);
	       save_str("<user abort>");
	       YYERROR;
	    }

	    if(get_macro("abort_handler",
			 (int *)NULL,(int *)NULL,(int *)NULL) != NULL) {
	       push("\n",S_NORMAL);
	       push(ptr,S_NORMAL);
	       push("abort_handler ",S_NORMAL);
	       if(sm_verbose > 2) {
		  msg_1s(">> macro : abort_handler %s\n",ptr);
	       }
	       (void)yyparse();
	       push("\n",S_NORMAL);
	    } else {
	       start_line();
	       msg_1s("%s\n",ptr);
	       save_str("<user abort>");
	       YYERROR;
	    }
	 }
      | USER integer			/* user defined */
	  {
	     word[1] = '\0';
	     if(mgets(word,CHARMAX) != NULL) {
		int tmp = $2;
	        userfn(&tmp, &word[1]);
	     }
	  }
      | VERBOSE number			/* set talkative mode */
         {
	    if($2 < 0) {
#if defined(YYDEBUG)
	       yydebug = 1 - yydebug;	/* toggle yydebug */
#endif
	       $2 = -$2;
	    }
	    sm_verbose = $2;
	 }
      | VIEWPOINT number number number
	 { sm_set_viewpoint($2,$3,$4); }
      | WHILE delim_str_list '{' str_list '}'
         {
	    char *cmd;
	    char *fmt = "IF(%s) { %s } ELSE { BREAK }\n ";
	    int len2, len4;

	    len2 = strlen($2.start); $2.start[len2 - 1] = '\0'; len2--;
	    len4 = strlen($4.start); $4.start[len4 - 1] = '\0'; len4--;

	    if((cmd = malloc(len2 + len4 + (strlen(fmt) - 4) + 1)) == NULL) {
	       msg("Can't allocate space for while loop\n");
	       free((char *)$2.start); free((char *)$4.start);
	       break;
	    }

	    sprintf(cmd,fmt,$2.start,$4.start);
	    push(cmd,S_WHILE);

	    free((char *)$2.start); free((char *)$4.start);
	 }
      | BREAK
         { end_while(); }
      | WINDOW integer integer integer integer /* set plot window */
	 { sm_window( $2, $3, $4, $5, $4, $5); }
      | WINDOW integer integer integer integer ':' integer
	 { sm_window( $2, $3, $4, $5, $4, $7); }
      | WINDOW integer integer integer ':' integer integer
	 { sm_window( $2, $3, $4, $7, $6, $7); }
      | WINDOW integer integer integer ':' integer integer ':' integer
	 { sm_window( $2, $3, $4, $7, $6, $9); }
      | WORD				/* not a keyword */
         {
            if(execute($1) == 0) {	/* it's a macro */
	       start_line();		/* start a new line in tokens.c */
	       if(sm_verbose > 2) msg_1s(">> macro : %s\n",$1);
	    } else {
	       if(!strcmp($1,"\n")) {	/* probably in quotes by mistake */
		  msg("\"\\n\" is not a valid macro\n");
		  in_quote = noexpand = 0;
		  YYERROR;
	       } else {
		  if(get_macro("macro_error_handler",
			       (int *)NULL,(int *)NULL,(int *)NULL) == NULL) {
		     msg_1s("%s is not a macro\n",$1);
		  } else {
		     static char name[60];
		     strcpy(name,$1);
		     noexpand = 0;	/* we must be able to recognise the
					   error handler */
		     push(name,S_NORMAL);
		     push("macro_error_handler NOT_FOUND ",S_NORMAL);
		     if(sm_verbose > 2) {
			msg_1s(">> WORD : %s\n",name);
			msg(">> macro : macro_error_handler NOT_FOUND\n");
		     }
		     (void)yyparse();
		     push("\n",S_NORMAL);
		  }
	       }
	       break;
	    }
         }
      | WRITE HISTORY WORD		/* write a macro into history */
         { (void)read_history_macro($3); }
      | WRITE IMAGE WORD		/* write the current image to a file */
         { write_image($3); }
      | WRITE STANDARD			/* write to terminal */
         {
	    (void)mgets(buff,MACROSIZE);
	    msg_1s("%s\n",&buff[*buff == '\0' ? 0 : 1]); /* skip ' ' */
         }
      | WRITE TABLE plus_or_space WORD delim_list
         {
	    int append = ($3 == '+');  
	    int dimen = 0, i, nvec;
	    Const VECTOR **temp;

	    if(graph_mode) { graph_mode = 0; IDLE(); }

	    if(!append && would_clobber($4)) {
	       break;
	    }

	    nvec = $5->nitem;
	    if((temp = (Const VECTOR **)malloc(nvec*sizeof(VECTOR *))) == NULL){
	       msg("Can't allocate storage for temp\n");
	       break;
	    }

	    for(i = 0;i < nvec && !sm_interrupt;i++) {	/* get vectors */
	       if((temp[i] = get_vector_ptr($5->i_list[i]->str)) == NULL) {
		  break;
	       }
	       if(i == 0) {
		  dimen = temp[0]->dimen;
	       } else if(temp[i]->dimen != dimen) {
		  if(sm_verbose > 1)
		     msg("Vectors are different sizes\n");
		  if(dimen < temp[i]->dimen) {
		     dimen = temp[i]->dimen;
		  }
	       }
	    }
	    if(i < nvec) {
	       free((char *)temp);
               freelist($5);
	       break;
	    }

	    write_table($4, append ? "a" : "w", nvec, temp, dimen);
	    
	    free((char *)temp);
            freelist($5);
	 }
      | WRITE plus_or_space WORD	/* write to a file */
         {
	    static char file[80] = "";	/* name of file */
	    FILE *fil;

	    (void)mgets(buff,MACROSIZE);
	    if($2 == '+') {		/* treat file as old and append */
	       (void)strcpy(file,$3);
	    }
	    if(strcmp(file,$3) == 0) {	/* append to old file */
	       if((fil = fopen(file,"a")) == NULL) {
		  msg_1s("Can't reopen %s\n",file);
		  break;
	       }
	    } else {
	       if((fil = fopen($3,"w")) == NULL) {
		  fprintf(stderr,"Can't open %s\n",$3);
		  break;
	       }
	       (void)strcpy(file,$3);
	    }
	    fprintf(fil,"%s\n",&buff[*buff == '\0' ? 0 : 1]);	/* skip ' ' */
	    fclose(fil);
         }
      ;

arg_list :
	 {
	    if(($$ = getlist((TOK_LIST *)NULL)) == NULL) {
	       break;
	    }
	 }
      | WORD
         {
	    if(($$ = getlist((TOK_LIST *)NULL)) == NULL) {
	       break;
	    }

	    $$->i_type[$$->nitem] = TYPE_WORD;
	    (void)strncpy($$->i_list[$$->nitem++]->str,$1,CHARMAX);
	 }
      | arg_list ',' WORD
         {
	    if($1 == NULL) {		/* couldn't get space */
	       $$ = NULL;
	       break;
	    }
            if($1->nitem >= $1->nmax) {	/* get some more space */
	       $$ = getlist($1);
	    } else {
	       $$ = $1;
	    }

	    $$->i_type[$$->nitem] = TYPE_WORD;
	    (void)strncpy($$->i_list[$$->nitem++]->str,$3,CHARMAX);
	 }
      | expr
         {
	    if(($$ = getlist((TOK_LIST *)NULL)) == NULL) {
	       break;
	    }

	    $$->i_type[$$->nitem] = TYPE_VECTOR;
	    $$->i_list[$$->nitem++]->vec = $1;
	 }
      | arg_list ',' expr
         {
	    if($1 == NULL) {		/* couldn't get space */
	       $$ = NULL;
	       break;
	    }
            if($1->nitem >= $1->nmax) {	/* get some more space */
	       $$ = getlist($1);
	    } else {
	       $$ = $1;
	    }

	    $$->i_type[$$->nitem] = TYPE_VECTOR;
	    $$->i_list[$$->nitem++]->vec = $3;
	 }
      ;

expr : delim_list			/* set a vector to a list */
         {
	    int i,j,
	    type = VEC_NULL;		/* type of vector */

	    if($1 == NULL) break;
	    for(i = 0; i < $1->nitem; i++) { /* find type of vector */
	       if(i > 100) {		/* only check first fewish items */
		  break;
	       }

	       if($1->i_list[i]->str[0] == '\n' && $1->i_list[i]->str[1] == '\0') {
		  continue;			/* skip \n */
	       }
	       
	       switch (num_or_word($1->i_list[i]->str)) {
		case 1: if(type == VEC_NULL) {type = VEC_LONG;} break;
		case 2: type = VEC_FLOAT; break;
		default: type = VEC_STRING; break;
	       }
	       if(type == VEC_STRING) {
		  break;		/* if any are string, all are */
	       }
	    }
	    
	    if(make_anon_vector(&$$,$1->nitem,type) != 0) {
	       $$.type = VEC_NULL;
	       j = 0;
	    } else {
	       for(i = j = 0;i < $1->nitem;i++) {
		  if($1->i_list[i]->str[0] == '\n' && $1->i_list[i]->str[1] == '\0') {
		     continue;			/* skip \n */
		  }
		  if(type == VEC_LONG) {
		     $$.vec.l[j++] = atoi($1->i_list[i]->str);
		  } else if(type == VEC_FLOAT) {
		     $$.vec.f[j++] = atof2($1->i_list[i]->str);
		  } else {
		     (void)strncpy($$.vec.s.s[j++],$1->i_list[i]->str,VEC_STR_SIZE);
		  }
	       }
	    }
	    $$.dimen = j;		/* don't bother to realloc storage */
            freelist($1);
	 }
      | expr CONCAT expr
	 { vec_concat(&$1,&$3,&$$); }
      | WORD
         {
	    if($1[0] == '0') {		/* may be a hex or octal constant */
	       LONG val;
	       if((val = atof2($1)) == 0) {
		  if(strncmp($1,"0x0", 3) == 0) { /* the only non-word == 0 */
		     char *ptr = $1 + 3;
		     while(*ptr == '0') ptr++; /* 0x0000 is allowed */
		     if(*ptr != '\0') {
			msg_1s("Illegal vector name: %s\n",$1);
			make_anon_vector(&$$, 0, VEC_FLOAT);
			$$ = *zero_vec();
			break;
		     }
		  } else {
		     msg_1s("Illegal vector name: %s\n",$1);
		     make_anon_vector(&$$, 0, VEC_FLOAT);
		     $$ = *zero_vec();
		     break;
		  }
	       }
	       (void)vec_ivalue(&$$,val);
	    } else if(get_vector($1,&$$) < 0) {
	       break;
	    }
	 }
      | string
	 {
	    if(make_anon_vector(&$$,1,VEC_STRING) == 0) {
	       (void)strncpy($$.vec.s.s[0],$1,VEC_STR_SIZE);
	    } else {
	       $$.type = VEC_NULL;
	    }
	 }
      | WORD '(' arg_list ')'		/* expand a macro function */
         {
	    char *mtext;		/* body of macro */
	    int i,narg_min,narg_max;
	    VECTOR tmp;

	    if((mtext = get_macro($1,&narg_min,&narg_max,(int *)NULL)) ==
									NULL) {
	       msg_1s("Macro %s is not defined\n",$1);
	       $$.type = VEC_NULL;
	       vec_free(&$$);
	       freelist($3);
	       break;
	    } else if($3->nitem < narg_min || $3->nitem > narg_max) {
	       msg_1s("Macro %s expects ",$1);
	       msg_1d("%d <= narg <= ",narg_min);
	       msg_1d("%d, ",narg_max);
	       msg_1d("%d provided\n",$3->nitem);
	       $$.type = VEC_NULL; $$.dimen = 0;
	       $$.name = "(macro())";
	       freelist($3);
	       break;
	    }

	    push("\nSNARK",S_NORMAL);
	    push_mstack($1,0,0,0);
	    
	    for(i = 0;i < $3->nitem;i++) {
	       if($3->i_type[i] == TYPE_WORD) {
		  if(strncmp($3->i_list[i]->str,"__",2) == 0 &&
					 get_vector($3->i_list[i]->str,&tmp) >= 0) {
		     sprintf(buff,"__%s_arg%d",$1, i + 1);
		     make_local_vector(buff);
		     (void)copy_vector(buff,tmp);
		     add_marg(i + 1,buff);
		  } else {
		     add_marg(i + 1,$3->i_list[i]->str);
		  }
	       } else if($3->i_type[i] == TYPE_VECTOR) {
		  if($3->i_list[i]->vec.dimen == 1 &&
		     strcmp($3->i_list[i]->vec.name,"(scalar)") == 0) {
		     if($3->i_list[i]->vec.type == VEC_FLOAT) {
			sprintf(buff,"%.10g",$3->i_list[i]->vec.vec.f[0]);
		     } else if($3->i_list[i]->vec.type == VEC_LONG) {
			char *dl[] = { "", "%d", "%ld" };	/* do we need a %l format? */
			assert(sizeof(LONG)/4 < 3);
			sprintf(buff,dl[sizeof(LONG)/4],$3->i_list[i]->vec.vec.l[0]);
		     } else if($3->i_list[i]->vec.type == VEC_STRING) {
			strcpy(buff,$3->i_list[i]->vec.vec.s.s[0]);
		     } else {
			strcpy(buff,"(null)");
		     }
		  } else {
		     sprintf(buff,"__%s_arg%d",$1, i + 1);
		     make_local_vector(buff);
		     (void)copy_vector(buff,$3->i_list[i]->vec);
		  }
		  add_marg(i + 1,buff);
	       } else {
		  msg_1d("Unknown type on t_list: %d\n",$3->i_type[i]);
		  add_marg(i + 1,"unknown");
	       }
	    }
	    freelist($3);
	    
	    recurse_lvl++;
	    push(mtext,S_MACRO);
	    yyparse();			/* execute macro _now_ */
	    get_vector($1,&$$);
	    free_vector($1);
	 }
      | WORD '[' expr ']'
         {
	    int i,j;
	    VECTOR *temp;

	    if($3.type == VEC_STRING) {
	       msg_1s("Vector %s is not arithmetic\n",$3.name);
	       $$.type = VEC_NULL;
	       vec_free(&$3);
	       $$.name = "(undefined[])";
	       break;
	    }

	    vec_convert_long(&$3);

	    if((temp = get_vector_ptr($1)) == NULL ||
			make_anon_vector(&$$,$3.dimen,temp->type) != 0) {
	       $$.type = VEC_NULL;
	       vec_free(&$$);
	       $$.name = "(undefined[])";
	       vec_free(&$3);
	       break;
	    }

	    for(j = 0;j < $3.dimen;j++) {
	       i = $3.vec.l[j];
	       if(i >= 0 && i < temp->dimen) {
		  if(temp->type == VEC_FLOAT) {
		     $$.vec.f[j] = temp->vec.f[i];
		  } else if(temp->type == VEC_LONG) {
		     $$.vec.l[j] = temp->vec.l[i];
		  } else {
		     strcpy($$.vec.s.s[j],temp->vec.s.s[i]);
		  }
	       } else {
		  msg_1d("Subscript %d ",i);
		  msg_1s("for %s is out of range ",$1);
		  msg_1d("[0,%d]\n",temp->dimen-1);
		  if($$.type == VEC_STRING) {
		     $$.vec.s.s[j][0] = '\0';
		  } else if($$.type == VEC_FLOAT) {
		     $$.vec.f[j] = 0;
		  } else if($$.type == VEC_LONG) {
		     $$.vec.l[j] = 0;
		  }
	       }
	    }
	    vec_free(&$3);
	 }
      | '(' expr ')'
	 { $$ = $2; }
      | '-' expr %prec UNARY
         {
	    VECTOR zero;		/* scalar with value 0 */

	    (void)vec_value(&zero,0.0);	/* set value of zero */
	    vec_subtract(&zero,&$2,&$$);
	 }
      | expr '?' expr ':' expr
	 { vec_ternary(&$1,&$3,&$5,&$$); }
      | ABS '(' expr ')'		/* take absolute value of vector */
	 { vec_abs(&$3,&$$); }
      | ACOS '(' expr ')'		/* take acos() of vector */
	 { vec_acos(&$3,&$$); }
      | ASIN '(' expr ')'		/* take asin() of vector */
	 { vec_asin(&$3,&$$); }
      | ATAN '(' expr ')'		/* take atan() of vector */
	 { vec_atan(&$3,&$$); }
      | ATAN2 '(' expr ',' expr ')'	/* take atan2() of two vectors */
	 { vec_atan2(&$3,&$5,&$$); }
      | COS '(' expr ')'		/* take cos() of vector */
	 { vec_cos(&$3,&$$); }
      | CTYPE '(' ')'			/* return current ctype values */
         {
	    $$.type = VEC_FLOAT;
	    vec_ctype(&$$);
	 }
      | CTYPE '(' STRING ')'		/* return current ctype names */
         {
	    $$.type = VEC_STRING;
	    vec_ctype(&$$);
	 }
      | DO '(' s_expr ',' s_expr ')'	/* create vector as implicit DO loop */
         {
	    if(make_anon_vector(&$$,0,VEC_NULL) != 0) {
	       break;
	    }
	    if(vec_do(&$$, $3, $5, 1) < 0) {
	       break;
	    }
	 }
      | DO '(' s_expr ',' s_expr ',' s_expr ')'	/* implicit DO loop */
         {
	    if(make_anon_vector(&$$,0,VEC_NULL) != 0) {
	       break;
	    }
	    if(vec_do(&$$, $3, $5, $7) < 0) {
	       break;
	    }
	 }
      | EXP '(' expr ')'		/* take exp() of vector */
	 { vec_exp(&$3,&$$); }
      | expr '+' expr
	 { vec_add(&$1,&$3,&$$); }
      | expr '-' expr
	 { vec_subtract(&$1,&$3,&$$); }
      | expr '*' expr
	 { vec_multiply(&$1,&$3,&$$); }
      | expr '/' expr
	 { vec_divide(&$1,&$3,&$$); }
      | expr POWER_SYMBOL expr
	 { vec_power(&$1,&$3,&$$); }
      | expr '%' expr
	 { vec_mod(&$1,&$3,&$$); }
      | expr '&' expr
	 { vec_bitand(&$1,&$3,&$$); }
      | expr '|' expr
	 { vec_bitor(&$1,&$3,&$$); }
      | expr LEFT_SHIFT_SYMBOL expr
	 { vec_bitshift(&$1,&$3,&$$, 1); }
      | expr RIGHT_SHIFT_SYMBOL expr
	 { vec_bitshift(&$1,&$3,&$$, 0); }
      | expr '=' '=' expr
	 { vec_eq(&$1,&$4,&$$); }
      | '!' expr %prec UNARY
         {
	    VECTOR zero;		/* scalar with value 0 */

	    (void)vec_value(&zero,0.0);	/* set value of zero */
	    vec_eq(&zero,&$2,&$$);
	 }
      | expr '!' '=' expr
	 { vec_ne(&$1,&$4,&$$); }
      | expr '&' '&' expr
	 { vec_and(&$1,&$4,&$$); }
      | expr '|' '|' expr
	 { vec_or(&$1,&$4,&$$); }
      | expr '>' expr
	 { vec_gt(&$1,&$3,&$$); }
      | expr '>' '=' expr
	 { vec_ge(&$1,&$4,&$$); }
      | expr '<' expr
	 { vec_gt(&$3,&$1,&$$); }
      | expr '<' '=' expr
	 { vec_ge(&$4,&$1,&$$); }
      | ATOF '(' expr ')'		/* convert vector to floats */
	 { vec_float(&$3,&$$); }
      | FLOAT '(' expr ')'		/* take (float) of vector */
	 { vec_cast_float(&$3, &$$); }
      | GAMMA '(' expr ',' expr ')'	/* incomplete gamma function */
	 { vec_gamma(&$3,&$5,&$$); }
      | HISTOGRAM '(' expr ':' expr ')' /* calculate a histogram */
	 { vec_hist(&$3,&$5,&$$); }
      | IMAGE '(' expr ',' expr ')'	/* interpolate in an image */
	 { vec_interp(&$3,&$5,&$$); }
      | IMAGE '[' expr ',' expr ']'	/* return elements of an image */
	 { vec_get_image_from_index(&$3,&$5,&$$); }
      | IMAGE '[' '*' ',' expr ']'	/* return column of an image */
	 { vec_get_image_from_index(NULL,&$5,&$$); }
      | IMAGE '[' expr ',' '*' ']'	/* return row of an image */
	 { vec_get_image_from_index(&$3,NULL,&$$); }
      | IMAGE '[' '*' ',' '*' ']'	/* return row of an image */
	 { vec_get_image_from_index(NULL,NULL,&$$); }
      | INDEX '(' expr ',' expr ')'	/* interpolate in an image */
	 { vec_index(&$3,&$5,&$$); }
      | INT '(' expr ')'		/* take (int) of vector */
	 { vec_int(&$3,&$$); }
      | LENGTH '(' expr ')'		/* drawn length of vector's elements */
	 { vec_length(&$3,&$$); }
      | LG '(' expr ')'			/* take lg() of vector */
	 { vec_lg(&$3,&$$); }
      | LN '(' expr ')'			/* take ln() of vector */
	 { vec_ln(&$3,&$$); }
      | number
	 {
	    if(number_is_int) {
	       (void)vec_ivalue(&$$, intval);
	    } else {
	       (void)vec_value(&$$, $1);
	    } 
	 }
      | PI				/* set value to pi */
         { (void)vec_value(&$$,Pi); }
      | RANDOM '(' s_expr ')'		/* return a random vector */
	 { vec_random((int)$3,&$$); }
      | SIN '(' expr ')'		/* take sin() of vector */
	 { vec_sin(&$3,&$$); }
      | SORT '(' expr ')'
	 {
	    int dimen = $3.dimen;
	    int i;
	    
	    if($3.type == VEC_LONG) {
	       if(sm_verbose > 1) {
		  msg_1s("You cannot sort on an integer vector %s; converting to arithmetic\n",
			 $3.name);
	       }
	       vec_convert_float(&$3);
	    }
	    if($3.type == VEC_FLOAT) {
	       sort_flt($3.vec.f, $3.dimen);
	    } else if($3.type == VEC_STRING) {
	       int *sorted;
	       VECTOR temp_s;		/* temp vector for rearranging */

	       if((sorted = malloc((unsigned)$3.dimen*sizeof(int))) == NULL) {
		  msg("malloc returns NULL for SORT\n");
		  break;
	       }
	       
	       temp_s.type = VEC_STRING;
	       if(vec_malloc(&temp_s,dimen) == -1) {
		  msg("malloc returns NULL for SORT\n");
		  free((char *)sorted);
		  break;
	       }

	       sort_str_inds($3.vec.s.s,sorted,dimen);

	       (void)memcpy((Void *)temp_s.vec.s.s_s,
			    (Const Void *)$3.vec.s.s_s,dimen*VEC_STR_SIZE);
	       
	       for(i = 0;i < dimen;i++) {		/* restore it sorted */
		  strcpy($3.vec.s.s[i],temp_s.vec.s.s[sorted[i]]);
	       }
	       
	       free((char *)sorted);
	       vec_free(&temp_s);
	    } else if($3.type == VEC_NULL) {
	       msg_1s("I don't know how to sort vector %s of type NULL\n",
		      $3.name);
	    } else {
	       msg("You can't get here. SORT.\n");
	    }

	    $$.dimen = $3.dimen;
	    $$.type = $3.type;
	    $$.name = $3.name;
	    $$.vec = $3.vec;
	 }
      | SPRINTF '(' expr ',' expr ')'	/* sprintf a vector to another */
	 { vec_sprintf(&$3,&$5,&$$); }
      | SQRT '(' expr ')'		/* take sqrt() of vector */
	 { vec_sqrt(&$3,&$$); }
      | STRING '(' expr ')'		/* convert vector to a string */
	 { vec_string(&$3,&$$); }
      | STRING '<' WORD '>'		/* convert vector to a string */
	 {
	    char *q;				/* pointer to a ' */
	    char tmp[2*VEC_STR_SIZE];		/* quoted version of element */
	    char *tptr = tmp;			/* pointer to tmp */
	    char *eptr = $3;

	    if(make_anon_vector(&$$,1,VEC_STRING) != 0) {
	       $$.type = VEC_NULL;
	       break;
	    }

	    if((q = strchr(eptr, '\'')) == NULL) {
	       (void)strcpy($$.vec.s.s[0], $3);
	    } else {			/* quote the quotes */
	       do {
		  strncpy(tptr, eptr, q - eptr);
		  tptr += q - eptr;
		  *tptr++ = '\\';
		  *tptr++ = '\'';
		  *tptr = '\0';
		  
		  eptr = q + 1;
	       } while((q = strchr(eptr, '\'')) != NULL);
	       strcpy(tptr, eptr);	/* get end of string */
	       
	       tmp[VEC_STR_SIZE - 1] = '\0';
 	       (void)strcpy($$.vec.s.s[0], tmp);
	    }
	 }
      | STRLEN '(' expr ')'		/* num. chars in a vector's elements */
	 { vec_strlen(&$3,&$$); }
      | SUBSTR '(' expr ',' expr ',' expr ')' /* extract a substring */
	 { vec_substr(&$3,&$5,&$7,&$$); }
      | TAN '(' expr ')'		/* take tan() of vector */
	 { vec_tan(&$3,&$$); }
      ;

delim_list : '<' list '>'
	 { $$ = $2; }
      | '{' list '}'
	 { $$ = $2; }
      ;

delim_str_list : '<' str_list '>'
	 { $$ = $2; }
      | '{' str_list '}'
	 { $$ = $2; }
      ;

fill  : SHADE				/* shade with what? */
	 { $$ = 1; }
      | SHADE HISTOGRAM
	 { $$ = 2; }
      | SHADE HISTOGRAM ANGLE
	 { $$ = -2; }
      ;

exclamation_or_space : '!'
	 { $$ = '!'; }
      |
	 { $$ =  ' '; }
      ;

for_list : '(' list ')'			/* variable list for FOREACH */
	 { $$ = $2; }
      | delim_list 
	 { $$ = $1; }
      | WORD
	 {
	    int i;
	    VECTOR *v;

	    if(($$ = getlist((TOK_LIST *)NULL)) == NULL) {
	       break;
	    }
	    
	    if((v = get_vector_ptr($1)) == NULL) {
	       break;
	    }
	    if(v->type != VEC_FLOAT && v->type != VEC_LONG &&
	       v->type != VEC_STRING) {
	       msg_1s("Vector %s is neither arithmetic or string valued ",
		      v->name);
	       msg_1d("(it's %d)\n",v->type);
	       break;
	    }
	    
	    for(i = 0;i < v->dimen;i++) {
	       if($$->nitem >= $$->nmax) { /* get some more space */
		  $$ = getlist($$);
	       }
	       
	       if(v->type == VEC_FLOAT) {
		  
		  sprintf($$->i_list[$$->nitem++]->str,"%.10g",v->vec.f[i]);
	       } else if(v->type == VEC_LONG) {
		  char *dl[] = { "", "%d", "%ld" };	/* do we need a %l format? */
		  assert(sizeof(LONG)/4 < 3);

		  sprintf($$->i_list[$$->nitem++]->str,dl[sizeof(LONG)/4],v->vec.l[i]);
	       } else if(v->type == VEC_STRING) {
		  (void)strncpy($$->i_list[$$->nitem++]->str,v->vec.s.s[i],CHARMAX);
	       } else {
		  msg_1s("You cannot get here! (foreach %s ...)\n",v->name);
		  break;
	       }
	    }
	    if(i != v->dimen) { break; } /* error in loop */
	 }
      ;

graphic : AXIS number number number number INTEGER INTEGER INTEGER
								INTEGER INTEGER
	 { (void)sm_axis( $2, $3, $4, $5, $6, $7, $8, $9, $10 ); }
      | AXIS number number pexpr pexpr INTEGER INTEGER INTEGER INTEGER INTEGER
	 { vec_axis( $2, $3, &$4, &$5, (VECTOR *)NULL, $6, $7, $8, $9, $10 ); }
      | AXIS number number pexpr pexpr WORD INTEGER INTEGER INTEGER
								INTEGER INTEGER
	 {
	    VECTOR *lab;
	    
	    if((lab = get_vector_ptr($6)) == NULL) {
	       break;
	    }
	    if(lab->type != VEC_STRING) {
	       msg_1s("Error: %s is not a string vector\n",$6);
	       YYERROR;
	    }
	    if(lab->dimen != $5.dimen && lab->dimen != 1) {
	       msg_2s("The dimensions of %s and %s must be the same\n",
		      $5.name,$6);
	       YYERROR;
	    }
	    vec_axis( $2, $3, &$4, &$5, lab, $7, $8, $9, $10, $11 );
	 }
      | BOX
	   { sm_box(1,2,0,0); }
      | BOX INTEGER INTEGER
	   { sm_box($2,$3,0,0); }
      | BOX INTEGER INTEGER INTEGER INTEGER
	   { sm_box($2,$3,$4,$5); }
      | CONTOUR
	  { (void)sm_contour(); }
      | CTYPE '=' expr
	 {
	    set_color(&$3);
	    vec_free(&$3);
	 }
      | CTYPE INTEGER
	 { sm_ctype_i($2); }
      | CTYPE WORD
	 { sm_ctype($2); }
      | DOT
	 { sm_dot(); }
      | DRAW number number
	  { sm_draw($2,$3); }
      | DRAW '(' number number ')'
	  { gdraw($3,$4); }
      | ERASE
	  {
	     int raise_on_erase;
	     char *roe = print_var("raise_on_erase");
	     raise_on_erase = (*roe == '\0') || atoi(roe);

	     (*devices[devnum].dev_erase)(raise_on_erase); /* saying ERASE() won't work */
	  }
      | ERRORBAR pexpr pexpr expr INTEGER
	  {
	     REAL *temp = NULL;
	     int i,npoint;

	     vec_convert_float(&$4);

	     if($2.dimen < $3.dimen) {
	        msg_2s("%s contains fewer points than %s\n",$2.name,$3.name);
	        npoint = $2.dimen;
	     } else if($3.dimen < $2.dimen) {
	        msg_2s("%s contains fewer points than %s\n",$3.name,$2.name);
	        npoint = $3.dimen;
	     } else {
	        npoint = $2.dimen;
	     }

	     if($4.dimen == 1) {
		if((temp = (REAL *)malloc((unsigned)npoint*sizeof(REAL)))
								== NULL) {
		   msg("Can't allocate space in errorbar\n");
		   vec_free(&$2); vec_free(&$3); vec_free(&$4);
		   break;
		}
		for(i = 0;i < npoint;i++) {
		   temp[i] = $4.vec.f[0];
		}
		free((char *)($4.vec.f));
		$4.vec.f = temp;
	     } else if($4.dimen < npoint) {
		msg_2s("%s is smaller than %s ",$4.name,$2.name);
		msg_1s("or %s, truncating\n",$3.name);
		npoint = $4.dimen;
	     } else if($4.dimen > npoint) {
		msg_2s("%s is larger than %s ",$4.name,$2.name);
		msg_1s("or %s\n",$3.name);
	     }

	     (void)sm_errorbar($2.vec.f,$3.vec.f,$4.vec.f,$5,npoint);
	     vec_free(&$2); vec_free(&$3); vec_free(&$4);
	  }
      | GRID integer_or_space integer_or_space
	  { sm_grid($2,$3); }
      | LABEL
         {
	    if(*mgets(buff,MACROSIZE) != '\0') {
	       sm_label(&buff[1]);
	    } else {
	       msg("Can't get label\n");
	    }
	 }
      | LTYPE number
	 { sm_ltype( (int)$2 ); }
      | LTYPE ERASE
	 { sm_ltype( 10 ); }
      | LTYPE EXPAND number
	 {
	    if($3 <= 0) {
	       msg_1f("Illegal expand ltype: %g\n", $3);
	       break;
	    }
	    sm_set_ltype_expand($3);
	 }
      | PAGE
	  { (*devices[devnum].dev_page)(); }	/* saying PAGE() won't work */
      | HISTOGRAM ANGLE pexpr pexpr
	 {
	    int npoint;

	    if($3.dimen < $4.dimen) {
	       msg_2s("%s contains fewer points than %s\n",$3.name,$4.name);
	       npoint = $3.dimen;
	    } else if($4.dimen < $3.dimen) {	
	       msg_2s("%s contains fewer points than %s\n",$4.name,$3.name);
	       npoint = $4.dimen;
	    } else {
	       npoint = $3.dimen;
	    }

	    (void)sm_histogram( $3.vec.f, $4.vec.f, -npoint );
	 }
      | plot pexpr pexpr
         {
	    int npoint;

	    if($2.dimen < $3.dimen) {
	       msg_2s("%s contains fewer points than %s\n",$2.name,$3.name);
	       npoint = $2.dimen;
	    } else if($3.dimen < $2.dimen) {	
	       msg_2s("%s contains fewer points than %s\n",$3.name,$2.name);
	       npoint = $3.dimen;
	    } else {
	       npoint = $2.dimen;
	    }

	    if($1 == CONNECT) {
	       (void)sm_conn( $2.vec.f, $3.vec.f, npoint );
	    } else if($1 == HISTOGRAM) {
	       (void)sm_histogram( $2.vec.f, $3.vec.f, npoint );
	    } else if($1 == POINTS) {
	       sm_points( $2.vec.f, $3.vec.f, npoint );
	    } else {
	       msg("Unknown plotting command\n");
	    }
	    vec_free(&$2); vec_free(&$3);
	 }
      | plot pexpr pexpr IF '(' expr ')'
         {
	    int npoint;

	    if($2.dimen < $3.dimen) {
	       msg_2s("%s contains fewer points than %s\n",$2.name,$3.name);
	       npoint = $2.dimen;
	    } else if($3.dimen < $2.dimen) {
	       msg_2s("%s contains fewer points than %s\n",$3.name,$2.name);
	       npoint = $3.dimen;
	    } else {
	       npoint = $2.dimen;
	    }

	    if($6.dimen != npoint) {
	       msg("Logical vector differs in length from data\n");
	       if(npoint > $6.dimen) {
		  npoint = $6.dimen;
	       }
	    }

	    if($6.type == VEC_LONG) {
	       vec_convert_float(&$6);
	    }

	    if($1 == CONNECT) {
	       (void)sm_connect_if( $2.vec.f, $3.vec.f, $6.vec.f, npoint );
	    } else if($1 == HISTOGRAM) {
	       (void)sm_histogram_if( $2.vec.f, $3.vec.f, $6.vec.f, npoint );
	    } else if($1 == POINTS) {
	       sm_points_if( $2.vec.f, $3.vec.f, $6.vec.f, npoint );
	    } else {
	       msg("Unknown plotting command\n");
	    }
	    vec_free(&$6); vec_free(&$2); vec_free(&$3);
	 }
      | PUTLABEL INTEGER
         {
	    if(*mgets(buff,MACROSIZE) != '\0') {
	       sm_putlabel($2,&buff[1]);
	    } else {
	       msg("Can't get label\n");
	    }
	  }
      | RELOCATE number number
	  { sm_relocate($2,$3); }
      | RELOCATE '(' number number ')'
	  { sm_grelocate($3,$4); }
      | fill number pexpr pexpr	/* Shade (or fill) a region */
	 {
	    int npoint;

	    npoint = $3.dimen;
	    if(npoint != $4.dimen) {
	       msg("dimension of vectors in shade must be equal\n");
	       npoint = (npoint < $4.dimen) ? npoint : $4.dimen;
	    }
	    (void)sm_shade($2,$3.vec.f,$4.vec.f,npoint,$1);
	    vec_free(&$3); vec_free(&$4);
	 }
      | SURFACE integer number number
	  { sm_draw_surface($2,$3,$4,(REAL *)NULL,0,(REAL *)NULL,0); }
      | SURFACE integer number number WORD WORD
	 {
	    VECTOR *x,*y;
	    
	    if((x = get_vector_ptr($5)) == NULL) {
	       break;
	    }
	    if(x->type == VEC_STRING) {
	       msg_1s("Error: %s is not an arithmetic vector\n",$5);
	       YYERROR;
	    }
	    vec_convert_float(x);

	    if((y = get_vector_ptr($6)) == NULL) {
	       break;
	    }
	    if(y->type == VEC_STRING) {
	       msg_1s("Error: %s is not an arithmetic vector\n",$6);
	       YYERROR;
	    }
	    vec_convert_float(y);

	    sm_draw_surface($2,$3,$4,x->vec.f,x->dimen,y->vec.f,y->dimen);
	 }
      | XLABEL
         {
	    if(*mgets(buff,MACROSIZE) != '\0') {
	       sm_xlabel(&buff[1]);
	    } else {
	       msg("Can't get xlabel\n");
	    }
	 }
      | YLABEL
         {
	    if(*mgets(buff,MACROSIZE) != '\0') {
	       sm_ylabel(&buff[1]);
	    } else {
	       msg("Can't get ylabel\n");
	    }
	 }
      ;

integer : INTEGER
	 { $$ = $1; }
      | '-' INTEGER
	 { $$ = -$2; }
      ;

integer_or_space : integer
	 { $$ = $1; }
      |
	 { $$ = 0; }
      ;

limit_pair : pexpr
	 {
	    int i;
	    float min,max;
	    VECTOR *tmp = &$1;

	    vec_convert_float(tmp);

	    min = 1e30; max = -1e30;
	    for(i = 0;i < tmp->dimen;i++) {
	       if(tmp->vec.f[i] < NO_VALUE) { /* not a bad point */
		  if(tmp->vec.f[i] > max) {
		     max = tmp->vec.f[i];
		  }
		  if(tmp->vec.f[i] < min) {
		     min = tmp->vec.f[i];
		  }
	       }
	    }

	    if(fabs(xy_range) < 1e-20) { /* no range is set for this axis */
	       float diff = max - min;
	       
	       if(diff == 0) {
		  diff = min;
	       }
	       diff *= 0.05;
	       
	       $$.a = min - diff;
	       $$.b = max + diff;
	    } else if(fabs(max - min) < fabs(xy_range)) { /* max - min < range */
	       float mean = (min + max)/2;
	       $$.a = mean - xy_range/2;
	       $$.b = mean + xy_range/2;
	    } else {			/* we'll need to find the median */
	       float median;
	       
	       sort_flt(tmp->vec.f,tmp->dimen);
	       
	       for(i = tmp->dimen - 1;i >= 0;i--) { /* ignore bad points */
		  if(tmp->vec.f[i] < NO_VALUE) break;
	       }
	       if(i < 0) {
		  msg("No valid values to find limits from");
		  $$.a = $$.b = 0.0;
		  break;
	       }

	       if(i%2 == 0) {
		  median = tmp->vec.f[i/2];
	       } else {
		  median = (tmp->vec.f[i/2] + tmp->vec.f[i/2 + 1])/2;
	       }

	       $$.a = median - xy_range/2;
	       $$.b = median + xy_range/2;
	    }
	    
	    vec_free(&$1);
	 }
      | number number
         {
	    if(fabs(xy_range) < 1e-20) { /* no range specified */
	       $$.a = $1;
	       $$.b = $2;
	    } else {
	       float mean = ($1 + $2)/2;
	       $$.a = mean - xy_range/2;
	       $$.b = mean + xy_range/2;
	    }
	 }
      ;

list :					/* empty */
         { $$ = getlist((TOK_LIST *)NULL); }
      | list number_or_word
         {
	    if($1 == NULL) {		/* couldn't get space */
	       break;
	    }
            if($1->nitem >= $1->nmax) {	/* get some more space */
	       $$ = getlist($1);
	    } else {
	       $$ = $1;
	    }
	    (void)strncpy($$->i_list[$$->nitem]->str,$2,CHARMAX);
	    $$->nitem++;
	 }
      | list string
         {
	    if($1 == NULL) {		/* couldn't get space */
	       break;
	    }
            if($1->nitem >= $1->nmax) {	/* get some more space */
	       $$ = getlist($1);
	    } else {
	       $$ = $1;
	    }
	    (void)strncpy($$->i_list[$$->nitem]->str,$2,CHARMAX);
	    $$->nitem++;
	 }
      | list '\n'
         {
	    if($1 == NULL) {		/* couldn't get space */
	       break;
	    }
            if($1->nitem >= $1->nmax) {	/* get some more space */
	       $$ = getlist($1);
	    } else {
	       $$ = $1;
	    }
	    (void)strncpy($$->i_list[$$->nitem]->str,"\n",CHARMAX);
	    $$->nitem++;
	 }
      ;

number : sign _FLOAT			/* accept any positive number */
         {
	    number_is_int = 0;
	    $$ = $1 ? -$2 : $2;
	 }
      | sign INTEGER
         {
	    number_is_int = 1;
	    $$ = intval = $1 ? -$2 : $2;
	 }
      | DIMEN '(' expr ')'		/* Dimension of vector */
         {
	    number_is_int = 1;
	    $$ = intval = $3.dimen;
	    vec_free(&$3);
	 }
      | SUM '(' expr ')'		/* summation of elements of a vector */
	 {
	    int i;
	    double val;			/* $$ is a float */

	    number_is_int = 0;
	    val = 0;
	    if($3.type == VEC_STRING) {
	       msg_1s("Vector %s is string valued\n", $3.name);
	       vec_free(&$3);
	       break;
	    } else if($3.type == VEC_LONG) {
	       for(i = 0;i < $3.dimen;i++) {
		  val += $3.vec.l[i];
	       }
	    } else {
	       for(i = 0;i < $3.dimen;i++) {
		  val += $3.vec.f[i];
	       }
	    }
	    $$ = val;
	    vec_free(&$3);
	 }
      | WHATIS '(' something ')'	/* What is this? */
	 {
	    int i,tok,vtype;
	    YYSTYPE lval;

	    i = 0;
	    if($3[0] == '-') {		/* could be part of -number */
	       if($3[1] == '.' || isdigit($3[1])) {
		  i = 1;		/* assume it is, this could be wrong */
	       }
	    }
	    push(&$3[i],S_NORMAL);
#if defined(YYBISON)		/* a bison later than 1.14 */
	    tok = yylex(&lval);		/* convert word to a token */
#else
	    tok = yylex(&lval,(Void *)NULL); /* convert word to a token */
#endif
	    unsave_str();		/* yylex will have saved it */
	    if(tok == INTEGER || tok == _FLOAT) {
	       i = 0;
	    } else {					/* a word */
	       i = 01;
	       if(get_macro($3,(int *)NULL,(int *)NULL,(int *)NULL)
			         != NULL) i |= 02;	/* a macro */
	       if(exists_var($3)) i |= 04;	/* a variable */
	       if((vtype = is_vector($3)) != 0) {
		  i |= 010;		/* a vector */
		  if(vtype == VEC_FLOAT) {
		     i |= 040;
		  } else if(vtype == VEC_LONG) {
		     i |= 0200;
		  } else if(vtype == VEC_STRING) {
		     i |= 0100;
		  }
	       }
	       if(tok != WORD) i |= 020;		/* a keyword */
	    }
	    number_is_int = 1;
	    $$ = intval = i;
	 }
      ;

number_or_word : number
         { (void)sprintf($$,"%.10g",$1); }
      | WORD
         { (void)strcpy($$,$1); }
      ;

optional :				/* optional number */
	 { $$ = 1; }
      | ',' s_expr
         { $$ = $2; }
      ;

pexpr : WORD
	 {
	    int type;

	    if((type = get_vector($1,&$$)) < 0) {
	       break;
	    } else if(type == VEC_STRING) {
	       msg_1s("%s is not an arithmetic vector\n",$1);
	       break;
	    }
	    vec_convert_float(&$$);
	 }
      | '(' expr ')'
	 {
	    
	    $$ = $2; $$.name = "(expr)";
	    vec_convert_float(&$$);
	 }
      ;

plot  : CONNECT
	 { $$ = CONNECT; }
      | HISTOGRAM
	 { $$ = HISTOGRAM; }
      | POINTS
	 { $$ = POINTS; }
      ;

plus_or_space : '+'
	 { $$ = '+'; }
      |
	 { $$ =  ' '; }
      ;

prompt :
	 {
	    if(($$.start = malloc(1)) != NULL) {
	       $$.start[0] = '\0';
	       $$.end = $$.start;
	    }
	 }
      | delim_str_list
	 { $$ = $1; }
      ;

sign  : '-'	/* must be first, as we'll get a reduce/reduce conflict */
	 { $$ = 1; }
      | '+'
	 { $$ = 0; }
      |
	 { $$ = 0; }
      ;

s_expr : expr			/* a scalar expression */
	 {
	    if($1.dimen <= 0) {
	       msg_1s("Vector %s has dimension <= 0\n",$1.name);
	       YYERROR;
	    } else if($1.dimen != 1 && sm_verbose > 1) {
	       msg_1s("Vector %s must be a scalar\n",$1.name);
	    }
	    if($1.type == VEC_FLOAT) {
	       $$ = $1.vec.f[0];
	    } else if($1.type == VEC_LONG) {
	       $$ = $1.vec.l[0];
	    } else {
	       if(sm_verbose > 1) {
		  msg_1s("Error: %s is not an arithmetic vector\n",$1.name);
		  vec_free(&$1);
		  YYERROR;
	       }
	    }
	    vec_free(&$1);
	 }
      | s_expr AND {
	 int c;
	 int lhs = $1;
	 int plevel = 1;		/* level of nesting of parentheses */
	 if(!lhs) {
	    save_str("(skipping expr)");
	    if(sm_verbose > 3) {
	       msg_1d("%d> (skipping expr)\n",noexpand);
	    }
	    
	    noexpand++;
	    while(!sm_interrupt) {
	       c = input();
	       if(c == '(') {
		  plevel++;
	       } else if(c == ')') {
		  plevel--;
		  if(plevel == 0) {
		     push("0) ", S_NORMAL);
		     break;
		  }
	       }
	    }
	    noexpand--;
	 }
      } s_expr
         {
	    $$ = $1 && $4;
	 }
      | s_expr OR {
	 int c;
	 int lhs = $1;
	 int plevel = 1;		/* level of nesting of parentheses */
	 if(lhs) {
	    save_str("(skipping expr)");
	    if(sm_verbose > 3) {
	       msg_1d("%d> (skipping expr)\n",noexpand);
	    }
	    noexpand++;
	    while(!sm_interrupt) {
	       c = input();
	       if(c == '(') {
		  plevel++;
	       } else if(c == ')') {
		  plevel--;
		  if(plevel == 0) {
		     push("1) ", S_NORMAL);
		     break;
		  }
	       }
	    }
	    noexpand--;
	 }
      } s_expr
         {
	    $$ = $1 || $4;
	 }
      ;

set_expr : expr
	{ $$ = $1; }
      | expr IF '(' expr ')' /* conditional assignment */
         {
	    int i,j;

	    if($4.dimen != $1.dimen && $4.dimen != 1) {
	       msg_1s("Setting %s: ",$<charval>-1);
	       msg_2s("Logical vector %s has a different length from %s\n",
		      $4.name, $1.name);
	    }

	    if($4.type == VEC_STRING) {
	       msg_1s("Vector %s is not arithmetical\n", $4.name);
	    }
	    vec_convert_long(&$4);

	    if($4.dimen == 1) {
	       if($4.vec.l[0]) {
		  j = $1.dimen;		/* Keep everything */
	       } else {
		  j = 0;		/* Keep nothing */
	       }
	    } else {
	       for(i = 0,j = 0;i < $1.dimen;i++) {
		  if(i >= $4.dimen || $4.vec.l[i]) { /* copy desired elements*/
		     if (i != j) {
			if($1.type == VEC_FLOAT) {
			   $1.vec.f[j] = $1.vec.f[i];
			} else if($1.type == VEC_LONG) {
			   $1.vec.l[j] = $1.vec.l[i];
			} else if($1.type == VEC_STRING) {
			   (void)strncpy($1.vec.s.s[j],$1.vec.s.s[i],VEC_STR_SIZE);
			}
		     }
		     j++;
		  }
	       }
	    }
	    vec_free(&$4);

	    if(($1.dimen = j) == 0 && sm_verbose > 0) {
	       msg_1s("Vector %s has no elements\n",$<charval>-1);
	    }
	    if(vec_realloc(&$1,$1.dimen) < 0) {
	       msg_1s("Can't reclaim storage for %s\n",$<charval>-1);
	       break;
	    }
	    $$ = $1;
	 }
      | s_expr ',' s_expr optional
	 {
	    if(make_anon_vector(&$$,0,VEC_NULL) != 0) {
	       break;
	    }
	    if(vec_do(&$$, $1, $3, $4) < 0) {
	       break;
	    }
	 }
      ;

something :			/* get something, not including [)\n] */
         {
	    char c;
	    int i;
	    
	    while(c = input(), isspace(c)) continue;
	    (void)unput(c);			/* one too far */

	    for(i = 0;i < CHARMAX - 1;) {
	       if((c = input()) != ')' && !isspace(c)) {
	          $$[i++] = c;
	       } else {
		  (void)unput(c);
		  break;
	       }
	    }
	    $$[i] = '\0';
	    if(i == 0) {
	       msg("Error: empty name\n");
	       YYERROR;
	    }
	    save_str($$);
            if(sm_verbose > 3) {
               msg_1d("%d> WORD     ",noexpand);
               msg_1s("%s\n",$$);
	    }
	 }
      ;

something_to_list : DEFINE '|'		/* list all internal variables */
         {
	    (void)more((char *)NULL);	/* initialise more */
	    list_internal_vars();
         }
      | DEFINE IMAGE			/* IMAGE variables */
	 {
	    (void)more((char *)NULL);	/* initialise more */
	    list_image_vars();
	 }
      | DEFINE '\n'			/* list all defined variables */
         {
	    (void)more((char *)NULL);	/* initialise more */
	    if(sm_verbose > 1) {
	       more("Internal variables:\n");
	       list_internal_vars();
	       more("\nUser variables:\n");
	    }
            (void)varlist(0,255);
            (void)unput('\n');
         }
      | DEFINE WORD WORD		/* list in range [WORD-WORD] */
	 {
	    (void)more((char *)NULL);	/* initialise more */
	    (void)varlist($2[0],$3[0]);
	 }
      | DEVICE
	 {
	    char *ptr;
	    
	    (void)mgets(buff,CHARMAX);	/* read rest of line */
	    for(ptr = buff;*ptr != '\0' && isspace(*ptr);ptr++) {
	       continue;
	    }
	    stg_list(ptr);
	 }
      | EDIT
	 { list_edit(); }
      | MACRO '\n'			/* list all defined macros */
         {
            (void)macrolist('\0','\177');
            (void)unput('\n');
         }
      | MACRO WORD WORD			/* list in range [WORD-WORD] */
         { (void)macrolist($2[0],$3[0]); }
      | TABLE				/* list columns in a table */
         {
	    if(subtable < 0) {
	       msg("You don't have a current table\n");
	    } else {
	       if(subtable == 0) {
		  *word = '\0';
	       } else {
		  sprintf(word,"[%d]",subtable);
	       }
	       sprintf(buff,"Table %s%s, format %s:\n\n",
		       data_file,word,table_fmt);
	       more((char *)NULL);
	       more(buff);

	       list_table_cols(table_fmt);
	    }
	 }
      | SET				/* list all defined vectors */
	 { list_vectors(); }
      ;

str_list :				/* a list stored as a string */
	 {
	    char *ptr;			/* pointer to $$ */
	    int brace_level,		/* nesting of braces */
	        l_delim,r_delim;	/* left and right delimiters */
	    unsigned int size;		/* size allocated for $$ */

	    if((l_delim = *($<charval>0)) == '{') {
	       r_delim = '}';
	    } else if(l_delim == '<') {
	       r_delim = '>';
	    } else {
	       msg_1d("Unknown delimiter %c\n",l_delim);
	       $$.start = NULL;
	       break;
	    }

	    size = 500;
	    if(($$.start = malloc(size + 1)) == NULL) { /* extra 1 for '\0' */
	       msg("Can't allocate storage for list\n");
	       break;
	    }

	    for(brace_level = 0,ptr = $$.start;!sm_interrupt;ptr++) {
	       *ptr = input();
	       if(*ptr == r_delim) {
		  if(brace_level == 0) {
		     break;
		  } else {
		     brace_level--;
		  }
	       } else if(*ptr == l_delim) {
		  brace_level++;
	       }
	       if(ptr >= $$.start + size - 1) {
		  size *= 2;
		  if(($$.start = realloc($$.start,size + 1)) == NULL) {
		     msg("Can't reallocate storage for do loop\n");
		     goto failed;
		  }
		  ptr = $$.start + size/2 - 1;
	       }
	    }
	    $$.end = ptr;
	    *($$.end) = '\n';
	    *($$.end + 1) = '\0';

	    (void)unput(r_delim);
	    save_str("(str_list)");
	    if(sm_verbose > 3) {
	       if(sm_verbose > 4) {
		  msg_1d("%d> (str_list :\n",noexpand);
		  msg_1s("\t%s",$$.start);
	       } else {
		  msg_1d("%d> (str_list)\n",noexpand);
	       }
	    }
	    break;
	    failed: ;			/* break out after failed realloc */
	 }

string : STRING
	 {
	    char *ptr,*str;
	    
	    str = $1; str++;		/* strip opening ' */
	    for(ptr = $$;*str != '\0';) {
	       if(*str == '\\') {
		  str++;
		  switch (*str) {
		   case '\\': *ptr++ = '\\'; break;
		   case '\'': *ptr++ = '\''; break;
		   case 'n': *ptr++ = '\n'; break;
		   case 'r': *ptr++ = '\r'; break;
		   case 't': *ptr++ = '\t'; break;
		   default:
		     msg_1d("Unknown escape sequence in format: \\%c\n",*str);
		     *ptr++ = *str;
		     break;
		  }
		  if(*str != '\0') str++;
	       } else {
		  *ptr++ = *str++;
	       }
	    }
	    *(ptr - 1) = '\0';		/* strip closing ' */
	 }
      | HELP '(' WORD ')'		/* a vector's help string */
	 {
	    VECTOR *temp;
		     
	    if((temp = get_vector_ptr($3)) == NULL) {
	       break;
	    }
	    $$[CHARMAX - 1] = '\0';
	    strncpy($$,temp->descrip,CHARMAX - 1);
	 }
      ;

string_or_space :
	 { $$[0] = '\0'; }
      | string
	 { (void)strcpy($$,$1); }
      ;

variable :			/* define a variable */
         {
	    char c;
	    int i;
	    
	    while(c = input(), isspace(c)) continue;
	    (void)unput(c);			/* one too far */

	    for(i = 0;i < CHARMAX - 1;) {
	       if((c = input()) == '_' || isalnum(c)) {
	          variable_name[i++] = c;
	       } else {
		  (void)unput(c);
		  break;
	       }
	    }
	    variable_name[i] = '\0';
	    if(i == 0) {
	       msg("Error: empty variable name\n");
	       YYERROR;
	    }
	    save_str(variable_name);
            if(sm_verbose > 3) {
               msg_1d("%d> WORD     ",noexpand);
               msg_1s("%s\n",variable_name);
	    }
	 }
      ;

variable_value : DELETE			/* delete a variable */
         { if(variable_name[0] != '\0') setvar(variable_name,"delete"); }
      | LOCAL				/* make a variable local */
         { if(variable_name[0] != '\0') make_local_variable(variable_name); }
      | delim_str_list			/* set a variable to a string */
         {
	    if($1.start == NULL) break;
	    *($1.end) = '\0';		/* remove '\n' */
	    if(variable_name[0] != '\0') setvar(variable_name,$1.start);
            free($1.start);
	 }
      | delim_str_list '[' integer ',' integer ']' /* set part of a string */
         {
	    if($1.start == NULL) break;
	    *($1.end) = '\0';		/* remove '\n' */
	    if(variable_name[0] != '\0') {
	       int slen = strlen($1.start);
	       int i0 = $3, len = $5;
	       if(i0 < 0) { i0 = slen - i0; }
	       if(len <= 0) { len = (slen - i0) + len; }

	       if(i0 < 0 || i0 > len || len < 0 || i0 + len > slen) {
		  msg_1d("Illegal range i0:len == %d:", i0);
		  msg_1d("%d", len);
		  msg_2s(" for define %s <%s>", variable_name, $1.start);
		  msg_1d("[%d, ", $3);
		  msg_1d("%d]\n", $5);
		  
		  free($1.start);
		  break;
	       }

	       memmove($1.start, &$1.start[i0], len);
	       $1.start[len] = '\0';
	       
	       setvar(variable_name,$1.start);
	    }
            free($1.start);
	 }
      | IMAGE				/* set a variable from an image file */
	 {
	    if(variable_name[0] != '\0') {
	       setvar(variable_name,read_image_variable(variable_name));
	    }
	 }
      | READ INTEGER			/* read a variable from file */
         {
	    char *temp;

	    if(variable_name[0] == '\0' ||
			      (temp = read_variable(data_file,$2,0)) == NULL) {
	       break;
	    }
	    setvar(variable_name,temp);
	 }
      | READ INTEGER INTEGER		/* read a variable from file */
         {
	    char *temp;

	    if(variable_name[0] == '\0' ||
			     (temp = read_variable(data_file,$2,$3)) == NULL) {
	       break;
	    }
	    setvar(variable_name,temp);
	 }
      | number_or_word			/* set a variable */
         { if(variable_name[0] != '\0') setvar(variable_name,$1); }
      | ':'				/* set a variable from env file */
	 {
	    char *temp;

	    if(variable_name[0] != '\0') {
	       if((temp = get_val(variable_name)) != NULL) {
		  setvar(variable_name,temp);
	       } else {
		  if(sm_verbose > 1)
		     msg_1s("%s is not defined in the environment file\n",
								variable_name);
	       }
	    }
	 }
      | '?' prompt			/* set a variable from stdin */
	 {
	    char *oldval;		/* old value of variable */

	    if(variable_name[0] == '\0' || $2.start == NULL) {
	       if($2.start != NULL) free($2.start);
	       break;
	    }
	    *($2.end) = '\0';		/* remove '\n' */
	    
	    if(graph_mode) { IDLE(); graph_mode = 0; }
	    
	    oldval = print_var(variable_name);
	    for(;;) {
	       if($2.start[0] == '\0') {
		  printf("%s ? [%s] ",variable_name,oldval);
	       } else {			/* use prompt specified */
		  printf("%s [%s] ",$2.start,oldval);
	       }
	       fflush(stdout);
	       (void)fgets(word,CHARMAX,stdin); word[strlen(word) - 1] = '\0';
	       if(logfil != NULL) fprintf(logfil,"%s\n",word);
	       if(word[0] != '\0') {	/* new value */
		  setvar(variable_name,word);
		  break;
	       } else if(oldval[0] == '\0') {
		  printf("There is no default value for %s\n",variable_name);
		  fflush(stdout);
	       } else {			/* keep old value */
		  break;
	       }
	    }
	    free($2.start);
	    if(in_graphics) { ENABLE(); graph_mode = 1; }
	 }
      | '|'				/* set a variable from SM itself */
	 {
	    int i;
	    
	    if(variable_name[0] != '\0') {
	       if((i = index_variable(variable_name)) < 0) {
		  msg_1s("%s is not a valid internal variable\n",variable_name);
		  break;
	       }
	       setvar(variable_name,get_variable(i));
	    }
	 }
      | '(' expr ')'			/* set a variable from an expression */
         {
	    if(variable_name[0] != '\0') {
	       if($2.dimen <= 0 || $2.type == VEC_NULL) {
		  msg_1s("vector %s has no value\n",$2.name);
		  break;
	       }
	       if($2.type == VEC_FLOAT) {
		  (void)sprintf(buff,"%.10g",$2.vec.f[0]);
		  setvar(variable_name,buff);
	       } else if($2.type == VEC_LONG) {
		  char *dl[] = { "", "%d", "%ld" };	/* do we need a %l format? */
		  assert(sizeof(LONG)/4 < 3);
		  (void)sprintf(buff,dl[sizeof(LONG)/4],$2.vec.l[0]);
		  setvar(variable_name,buff);
	       } else if($2.type == VEC_STRING) {
		  setvar(variable_name,$2.vec.s.s[0]);
	       } else {
		  msg_1s("vector %s has unknown type",$2.name);
		  msg_1d("%d\n",$2.type);
	       }
	       vec_free(&$2);
	    }
	 }
      ;

word_or_space :
	 { $$[0] = '\0'; }
      | WORD
	 { (void)strcpy($$,$1); }
      ;

macro_narg :
	 { $$[0] = '\0'; }
      | INTEGER
      { sprintf($$, "%d", (int)$1); }
      | WORD
	 { (void)strcpy($$,$1); }
      ;

%%					/* start of programmes */

void
yyerror(s)
char s[];
{
   char *cmacro,			/* which macro are we in? */
	*lline;				/* pointer to last line */
   int i,
       offset;				/* offset of last token in line */

   if(strcmp(s,"syntax error") != 0 &&			/* Yacc message */
		 	strcmp(s,"parse error") != 0 &&	/* Bison message */
		 	strcmp(s,"misc error") != 0 &&	/* our message */
		 	strcmp(s,"show where") != 0) {	/* display command */
      msg_1s("%s in control.y\n",s);
   } else {				/* Syntax Error */
      if(graph_mode) { IDLE(); graph_mode = 0; }
      lline = get_line(&offset);
      if(!strcmp(s,"misc error")) {	/* a private (not yacc) message */
	 fprintf(stderr,"Error at \"%s\"",&lline[offset]);
      } else if(!strcmp(s,"show where")) { /* just print current command */
	 if(!sm_verbose) {
	    fprintf(stderr,"\n");
	    return;
	 }
	 fprintf(stderr," at \"%s\"",&lline[offset]);
      } else if(strcmp(lline,"<user abort>") == 0) { /* a deliberate error */
	 if(*print_var("traceback") != '\0') {
	    dump_stack();
	 }
	 return;
      } else {
	 fprintf(stderr,"Syntax error at \"%s\"",&lline[offset]);
      }
      if((cmacro = current_macro()) == NULL) {
	 fprintf(stderr,"\n");
      } else {
	 fprintf(stderr," in macro %s\n",cmacro);
      }
      fprintf(stderr,"%s\n",lline);
      for(i = 0;i < offset;i++) (void)putc(' ',stderr);
      while(lline[i++] != '\0') (void)putc('^',stderr);
      (void)putc('\n',stderr);
      if(*print_var("traceback") != '\0') {
 	 dump_stack();
      }
      if(in_graphics) { ENABLE(); graph_mode = 1; }
   }
}

/*****************************************************************************/
/*
 * enable/disable graphics, setting graph_mode and in_graphics correctly
 *
 * Judicious use of these functions can significantly reduce mode switching
 */
void
sm_enable()
{
   if(!graph_mode) {
      graph_mode = 1;
      in_graphics = 1;
      ENABLE();
   }
}

void
sm_disable()
{
   if(graph_mode) {
      graph_mode = 0;
      in_graphics = 0;
      IDLE();
   }
}

/****************************************************/
/*
 * Print a string on stdout, after dealing with graph mode
 */
void
msg(fmt)
Const char *fmt;			/* message to print */
{
   if(graph_mode) { IDLE(); graph_mode = 0; }
   puts(fmt); fflush(stdout);
   if(in_graphics) { ENABLE(); graph_mode = 1; }
}

/*
 * Print a string on stdout, with one string argument
 */
void
msg_1s(fmt,s)
Const char *fmt,			/* message to print */
     *s;
{
   if(graph_mode) { IDLE(); graph_mode = 0; }
   printf(fmt,s); fflush(stdout);
   if(in_graphics) { ENABLE(); graph_mode = 1; }
}


/*
 * Print a string on stdout, with two string arguments
 */
void
msg_2s(fmt,s1,s2)
Const char *fmt,			/* message to print */
     *s1,*s2;
{
   if(graph_mode) { IDLE(); graph_mode = 0; }
   printf(fmt,s1,s2); fflush(stdout);
   if(in_graphics) { ENABLE(); graph_mode = 1; }
}

/*
 * Print a string on stdout, with one int argument
 */
void
msg_1d(fmt,i)
Const char *fmt;			/* message to print */
int i;
{
   if(graph_mode) { IDLE(); graph_mode = 0; }
   printf(fmt,i); fflush(stdout);
   if(in_graphics) { ENABLE(); graph_mode = 1; }
}

/*
 * Print a string on stdout, with one float argument
 */
void
msg_1f(fmt,x)
Const char *fmt;			/* message to print */
double x;
{
   if(graph_mode) { IDLE(); graph_mode = 0; }
   printf(fmt,x); fflush(stdout);
   if(in_graphics) { ENABLE(); graph_mode = 1; }
}

/*
 * Parse a qualifer: e.g. `2.s' -> VEC_STRING
 */
int
parse_qualifier(str)
char *str;
{
   char *ptr;
   
   if((ptr = strrchr(str,'.')) == NULL) {
      for(ptr = str; isdigit(*ptr); ptr++) continue;
      
      if(*ptr == '\0') {
	 return(VEC_FLOAT);
      } else if(*ptr == '-') {
	 if(parse_qualifier(ptr + 1) == VEC_FLOAT) {
	    return(VEC_FLOAT);
	 }
      }
   } else {
      switch (*(ptr + 1)) {
       case 'd': case 'i': case 'o': case 'x':
	 return(VEC_LONG);
       case 'f': case 'g':
	 return(VEC_FLOAT);
       case 's':
	 return(VEC_STRING);
       default:
	 break;
      }
   }

   msg_1s("unknown type specification for vector: %s\n",str);
   return(VEC_FLOAT);
}
