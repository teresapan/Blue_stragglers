#include "copyright.h"
/*
 * main programme for parser.
 * syntax: sm [-f environment_file] [-l logfile]
 *			[-m macrofile] [-s] [-S] [-u name] [-v#] commands
 *
 *    Note that the macro library directory is specified as the variable
 * macro in the environment file. For Unix, it should contain a trailing /,
 * for VMS it should end in a : (so that a filename can be directly appended).
 */
#include "options.h"
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include "edit.h"
#include "stack.h"
#include "devices.h"
#include "sm_declare.h"

#define SIZE 100

char prompt[SIZE],			/* system prompt */
     user_name[30];			/* first name of user */
FILE *logfil = NULL;		/* where to log all terminal input */
int expand_vars = 0,		/* expand variables everywhere */
    sm_interrupt = 0,		/* This is the I-Have-Caught-A-^C flag */
    in_quote = 0,		/* Are we in a quoted string? */
    no_editor,			/* don't use history/line editor? */
    noexpand = 0,		/* if > 0, then don't expand \$#@ and
				   return (almost) all tokens as WORD */
    plength = 2,		/* number of printing chars in ": " */
    recurse_lvl = 0,		/* how deeply nested are we? */
    sm_image_cursor = 0,	/* Is there an image displayed? */
    sm_verbose = 1;		/* shall I produce lots of output ? */
#if defined(DEBUG)
   extern int yydebug;		/* used for debugging yacc grammars */
#endif
static int jmp_ready = 0,	/* are we ready for a longjmp? */
	   saving = 0;		/* Am I currently doing a panic_save? */
static jmp_buf env;		/* for longjmp */
static void init_version();	/* initialise the version string */
static void usage();		/* print the usage summary */

#define NARGS 20		/* max number of command line args */

int
sm_parser(args)
char *args;
{
   char *av[NARGS];
   int ac,
       ret;

   if((av[0] = malloc(strlen(args) + 1)) == NULL) {
      msg("Can't allocate space for argument list\n");
      return(-1);
   }
   strcpy(av[0],args);
   if((av[1] = next_word(av[0])) == NULL) {
      ac = 1;
   } else {
      for(ac = 2;ac < NARGS;ac++) {
	 if(*(av[ac] = next_word((char *)NULL)) == '\0') {
	    break;
	 }
      }
   }

   ret = sm_main(ac,av);
   free(av[0]);

   return(ret);
}

int
sm_main(ac, av)
int ac;
char **av;
{
   char c,
	*cl_macro = NULL,		/* possible macro on command line */
	*command = NULL,		/* buffer for reading commands */
	*defaults_file = NULL,
        *font_file = NULL,		/* name of fonts file */
	full_file[SIZE],		/* full path for default macro file */
	*home_dir = NULL,		/* user's home directory */
	macro_dir[SIZE],		/* directory for startup macros */
	*ptr,
        *termtype_arg = NULL;		/* value for -t option */
   FILE *fil;
   int command_size = 0,		/* length of command buffer */
       greeting = 1,			/* greet user on startup */
       i,
       startup = 1;			/* run the startup macro? */

   no_editor = (isatty(0) == 0) ? 2 : 0;
   init_version();			/* need this beforee processing -V */

   while(ac > 1) {
      if(av[1][0] == '-') {
	 switch (av[1][1]) {
	  case 'd':		/* debug */
	    msg("Use the -v flag (e.g. -v-4) instead of -d\n");
	    break;
	  case 'f':		/* choose name of defaults_file */
	    if(ac <= 2) {
	       msg("You must specify a file with -f\n");
	    } else {
	       defaults_file = av[2];
	       ac--;
	       av++;
	    }
	    break;
	  case 'F':		/* choose name of fonts file */
	    if(ac <= 2) {
	       msg("You must specify a file with -F\n");
	    } else {
	       font_file = av[2];
	       ac--;
	       av++;
	    }
	    break;
	  case 'h':
	    usage();
	    return(0);
	  case 'l':		/* specify log file */
	    if(ac <= 2) {
	       msg("You must specify a file with -l\n");
	    } else {
	       if((logfil = fopen(av[2],"w")) == NULL) {
		  fprintf(stderr,"Can't open %s\n",av[2]);
	       }
	       ac--;
	       av++;
	    }
	    break;
	  case 'm':			/* read then execute a macro */
	    if(--ac <= 1) {
	       msg("You must specify a name with -m\n");
	       continue;
	    }
	    cl_macro = av[2];	/* this is the command line macro */
	    av++;
	    break;
	  case 'n':			/* don't run startup */
	    startup = 0;
	    break;
	  case 'q':			/* don't say Hello */
	    greeting = 0;
	    break;
	  case 's':			/* stupid terminal (or pipe) */
	    no_editor = 1;		/* disable history editor */
	    break;
	  case 'S':			/* stupid terminal (or pipe) */
	    no_editor = 2;		/* 's' + no prompt */
#if 0
	    setbuf(stdout,(char *)NULL);
	    setbuf(stderr,(char *)NULL);
#endif
	    break;
	  case 't':
	    if(ac <= 2) {
	       msg("You must specify terminal_type[:nlines] with -t\n");
	    } else {
	       termtype_arg = av[2];
	       ac--;
	       av++;
	    }
	    break;
	  case 'u':			/* run someone else's .sm file */
	    if(--ac <= 1) {
	       msg("You must specify a name with -u\n");
	       continue;
	    }
	    if(strcmp(av[2],"null") == 0) {
	       home_dir = "/dev/null";	/* there's no .sm file there! */
	    } else {
	       if((home_dir = get_home(av[2])) == NULL) {
		  msg_1s("I can't find a home directory for %s\n",av[2]);
	       }
	    }
	    av++;
	    break;
	  case 'V':			/* print version */
	    version();
	    msg("\n");
	    exit(EXIT_OK);
	  case 'v':
	    if(av[1][2] == '\0') {
	       if(--ac <= 1 || !isdigit(*(++av)[1])) {
		  msg("You must specify a number with -v\n");
		  continue;
	       }
	       sm_verbose = atoi(av[1]);
	    } else {
	       sm_verbose = atoi(&av[1][2]);
	    }
	    if(sm_verbose < 0) {
	       sm_verbose = -sm_verbose;
#ifndef DEBUG
	       msg("Control.y should have been compiled with DEBUG defined\n");
#else
	       yydebug = 1;
	       msg("In interpreting debugging output, yacc -dv may help\n");
#endif
	    }
	    break;
	  default:
	    msg_1s("Illegal option %s\n",av[1]);
	    usage();
	    break;
	 }
      } else {
	 break;
      }
      ac--;
      av++;
   }
#if defined(MALLOC_DEBUG)
#  if defined(sgi)
      mallopt(100,1);		/* make alloc check heap on each transaction */
#  endif
#  if defined(sun)
      malloc_debug(1);		/* make alloc check heap on each transaction */
#  endif
#endif

   init_stack();			/* initialise I/O stack */
   init_vectors();			/* and vectors */
   init_colors();			/* and colour */
   set_all_internal();			/* set all the internal variables */
   set_defaults_file(defaults_file,home_dir);
   set_help_path();

   if((ptr = get_val("macro")) == NULL) {	/* not defined in .sm */
      msg("Macro directory is not defined\n");
      macro_dir[0] = '\0';
   } else {
      (void)strncpy(macro_dir,ptr,SIZE);
   }
/*
 * Now two functions to persuade the loader to get some definitions from a
 * library
 */
   declare_devs();			/* device definitions */
   declare_vars();			/* various variables */
/*
 * Read the fonts, then install signal handlers
 */   
   (void)load_font(font_file);

#if defined(SIGBUS)
   (void)signal(SIGBUS,hand_acc);	/* catch bus errors */
#endif
   (void)signal(SIGSEGV,hand_acc);	/* catch access violations */
   if(no_editor < 2) {
      (void)signal(SIGINT,hand_C);	/* catch a ^C with routine hand_C */
   }
   (void)signal(SIGFPE,(void (*)P_ARGS((int)))hand_flt);
					/* catch floating point exceptions */
#if defined(SIGABRT)
   (void)signal(SIGABRT,hand_q);	/* catch a ^\ with routine hand_q */
#endif
#if defined(SIGQUIT)
	(void)signal(SIGQUIT,hand_q);	/* catch a ^\ with routine hand_q */
#endif
   (void)signal(SIGILL,(void (*)P_ARGS((int)))hand_ill);
					/* catch an illegal instruction */
#if defined(SIGWINCH)
   (void)signal(SIGWINCH,hand_winch);	/* window changed size */
#endif

   command_size = HSIZE;
   if ((command = malloc(command_size)) == NULL) {
       fprintf(stderr,"I can't even allocate space for the command line; bye\n");
       return(1);
   }

   (void)strcpy(prompt,":");		/* initialise prompt */
   if(termtype_arg != NULL) {
      char term[100];
      int nline;
      switch (sscanf(termtype_arg, "%[^:]:%d", term, &nline)) {
       case 0:
	 break;
       case 1:
	 (void)set_term_type(term, 0);
	 break;
       case 2:
	 (void)set_term_type(term, nline);
	 break;
      } 
   } else if(!no_editor) {
      if((ptr = get_val("term")) != NULL) { /* term is defined in .sm */
	 sprintf(command,"TERMTYPE %s\n",ptr);
	 assert(strlen(command) < command_size); /* should be safe */
	 push(command,S_NORMAL);
	 (void)yyparse();			/* shouldn't QUIT! */
      } else if(set_term_type(getenv("TERM"),0) < 0) { /* try environment */
	 (void)set_term_type("dumb",20);	/* term and TERM failed */
      }
   }
/*
 * allow local modifications to the SM startup. This should usually be
 * done in the startup macro, but if someone wants to (e.g.) control
 * access it must be done in the executable. Here's the place.
 */
#  include "local.c"
/*
 * Execute startup macro, and deal with command line requests. Note that
 * because they are going onto a stack, they'll be executed in the opposite
 * order to that in which they appear here
 *
 * Read command line, if it isn't empty, and execute it
 */
   if(ac > 1) {
      ptr = command;
      while(ac > 1) {
	 if (ptr - command > command_size - strlen(av[1]) - 2) { /* 2: 1 for the space; 1 for the NUL */
	     int offset = ptr - command;
	     command_size *= 2;
	     if ((command = realloc(command, command_size)) == NULL) {
		 fprintf(stderr,"Unable to extend command buffer to %d bytes\n", command_size);
		 return(1);
	     }
	     ptr = command + offset;
	 }
	 (void)sprintf(ptr,"%s ",av[1]);
	 ptr += strlen(av[1]) + 1;
	 ac--; av++;
      }
      *(ptr - 1) = '\n';
      *ptr = '\0';
      push(command,S_NORMAL);		/* try to execute (actually push) it */
   }
/*
 * If there was a command-line macro, push it next
 */
   if(cl_macro != NULL) {
      if((fil = fopen(cl_macro,"r")) != NULL) {
	 (void)read_macros(fil);
	 (void)fclose(fil);
      }
      
      for(i = strlen(cl_macro) - 1;i >= 0 && cl_macro[i] != '.';i--) continue;
      if(cl_macro[i] == '.') cl_macro[i--] = '\0';	/* delete suffix */
      for(;i >= 0 && ((c = cl_macro[i]) == '_' || isalnum(c));i--) continue;
      if(i > 0) cl_macro += i + 1;			/* delete path */
      
      push("\n",S_NORMAL);		/* terminate macro */
      push(cl_macro,S_NORMAL);		/* try to execute it */
   }
/*
 * Read and push default startup macro
 */
   (void)sprintf(full_file,"%sdefault",macro_dir);
   if((fil = fopen(full_file,"r")) == NULL) {
      msg_1s("Can't open %s\n",full_file);
   } else {
      (void)read_macros(fil);
      (void)fclose(fil);
   }
   read_hist((FILE *)NULL);		/* read in history from file */

   push("\n",S_NORMAL);
   if(startup) {
      push("startup",S_NORMAL);		/* try to execute macro startup */
   } 
/*
 * Now actually execute those commands
 */				
   if(yyparse() <= 0) {			/* call the command interpreter */
      return(-1);
   }
   sm_interrupt = 0;

   (void)strncpy(user_name,get_name(),20);
   if(greeting && no_editor < 2) {
      (void)printf("Hello %s, please give me a command\n",user_name);
   }

   (void)setjmp(env);			/* prepare for longjmp */
   jmp_ready = 1;
   for(;;) {
      (void)sprintf(command,"%s\n",edit_hist());
      push(command,S_NORMAL);

      if(yyparse() <= 0) {		/* call the command interpreter */
	 write_hist();			/* save history list */
	 (void)set_term_type("dumb",0);	/* reset terminal */
	 return(0);
      }
      
      while(sm_interrupt) {
	 sm_interrupt = 0;
	 if(get_macro("error_handler",
		      (int *)NULL,(int *)NULL,(int *)NULL) != NULL) {
	    push("error_handler\n",S_NORMAL);
	    if(sm_verbose > 2) msg(">> macro : error_handler\n");
	    (void)yyparse();
	 }
      }
      saving = 0;
   }
}

/****************************************/

#include "../AA_Version.h"
char *version_string = NULL;

static void
fix_rcs_string(str)
char *str;
{
   char *ptr = str;
   char *str0 = str;
   
   while(*str != '\0' && *str != '$') *ptr++ = *str++;
   while(*str != '\0' && *str++ != ':') continue;
   while(isspace(*str)) str++;
   while(*str != '\0' && *str != '$') *ptr++ = *str++;
   ptr--;
   while(ptr >= str0 && isspace(*ptr)) ptr--;
   *++ptr = '\0';
}

static void
init_version()
{
   version_string = version_string_;
}

void
version()
{
   if(version_string == NULL) {
      init_version();
      if(version_string == NULL) {
	 return;
      }
   }
   msg_1s("%s",version_string);
}

/**************************************************/

static void
usage()
{
   int i;
   static char *msgs[] = {
      "-f file         Specify the environment (`.sm') file",
      "-F file         Specify the fonts file (overrides .sm fonts)",
      "-h              Print this summary",
      "-l file         Specify a log file",
      "-m name         Read macros from file name, then execute macro name",
      "-n              Don't execute `startup' macro",
      "-q              Disable `Hello' message",
      "-s              (`Stupid') Disable history and command editor",
      "-S              Like -s, but also suppress the prompt",
      "-t termtype[:nline] Specify termtype and (optionally) number of lines",
      "-u user         Intepret ~ in .sm search path as user's home directory",
      "-V              Print the version string",
      "-v#             Set the initial verbosity to #",
      NULL,
   };

   msg("Syntax: sm [ options ] commands, where your options are:\n");
   for(i = 0;msgs[i] != NULL;i++) {   
      msg_1s("	%s\n",msgs[i]);
   }
   msg("\n");
}

/****************************************/
/*
 * Disable signal handling while signal's being handled, as SM may
 * not be feeling to cooperative at this point. We only do this on
 * ansi systems to avoid problems with the type of signal handlers,
 * but that should cover most places these days
 */
void
hand_q(i)				/* we've found a ^\ */
int i;					/* unused */
{
   char ans[10];
#if defined(__STDC__)
   void (*sig_q)();
#endif
   ans[0] = i;				/* quieten compilers */

#if defined(__STDC__)
   sig_q = signal(SIGABRT,SIG_DFL);
   (void)signal(SIGQUIT,SIG_DFL);
#endif

   (void)get1char(EOF);
   msg("Do you want to return to the SM prompt? y or n ");
   (void)scanf("%s",ans);
   if(ans[0] == 'y') {
#if defined(__STDC__)			/* reinstall handlers */
   (void)signal(SIGABRT,sig_q);
   (void)signal(SIGQUIT,sig_q);
#endif
      if(jmp_ready) {
	 longjmp(env,1);
      } else {
	 msg("I'm not ready for you yet, wait a bit and try again\n");
	 return;
      }
   }

   msg("Do you want a core dump? y or n ");
   (void)scanf("%s",ans);
   if(ans[0] == 'y')
      abort();
   else
      exit(EXIT_BAD);
   /*NOTREACHED*/
}

/****************************************/
void
#if defined(sun) && !defined(solaris)
hand_flt(sig,code,scp)		/* we've found a floating point exception */
int sig,				/* the signal */
    code;				/* and associated code */
struct sigcontext *scp;			/* needed by a sun */
#else
hand_flt(sig,code)		/* we've found a floating point exception */
int sig,				/* the signal */
    code;				/* and associated code */
#endif
{
   (void)signal(SIGFPE,(void (*)P_ARGS((int)))hand_flt); /* reset the trap */

   switch (code) {
#ifdef hp9000s300
   case 0:
	msg("Software floating point exception\n");
	break;
   case 5:
	msg("Integer divide by zero\n");
	break;
#endif
#ifdef FPE_FPA_ERROR
   case FPE_FPA_ERROR:
#if !defined(sun386)			/* The fpa uses this signal so as
					   to ask help from the 68881 */
      if(fpa_handler(scp) != 0) {
	 return;
      }
#endif
      break;
#endif
#ifdef FPE_INTOVF_TRAP
   case FPE_INTOVF_TRAP:
      msg("Integer overflow trap\n");
      break;
#endif
#ifdef FPE_INTDIV_TRAP
   case FPE_INTDIV_TRAP:
      msg("Integer division by zero trap\n");
      break;
#endif
#ifdef FPE_FLTOVF_TRAP
   case FPE_FLTOVF_TRAP:
      msg("Floating point overflow trap\n");
      break;
#endif
#ifdef FPE_FLTDIV_TRAP
   case FPE_FLTDIV_TRAP:
      msg("Floating point/decimal division by zero trap\n");
      break;
#endif
#ifdef FPE_FLTUND_TRAP
   case FPE_FLTUND_TRAP:
      msg("Floating point underflow trap\n");
      break;
#endif
#ifdef FPE_DECOVF_TRAP
   case FPE_DECOVF_TRAP:
      msg("Decimal overflow trap\n");
      break;
#endif
#ifdef FPE_SUBRNG_TRAP
   case FPE_SUBRNG_TRAP:
      msg("Subscript range trap\n");
      break;
#endif
#ifdef FPE_FLTOVF_FAULT
   case FPE_FLTOVF_FAULT:
      msg("Floating point overflow fault\n");
      break;
#endif
#ifdef FPE_FLTDIV_FAULT
   case FPE_FLTDIV_FAULT:
      msg("Floating point divide by zero fault\n");
      break;
#endif
#ifdef FPE_FLTUND_FAULT
   case FPE_FLTUND_FAULT:
      msg("Floating point underflow fault\n");
      break;
#endif
   default:
      msg_1d("Unknown code for SIGFPE : 0x%x\n",code);
      break;
   }
   hand_C(0);				/* clean up the parser */
   msg("I'll try to recover but no promises\n");
   yyerror("misc error");		/* get traceback */
   longjmp(env,1);
}

/****************************************/

void
hand_C(i)				/* we've found a ^C */
int i;					/* not used */
{
   if(sm_interrupt < 0) {		/* we are currently popping the stack */
      sm_interrupt = -3;
   } else {
      lexflush();		/* flush out all macros and reset parser */
      in_quote = 0;		/* not in quoted string */
      push("SNARK ",S_NORMAL);	/* cleanup recursion */
      if(noexpand) {
	 push("}\n",S_NORMAL);	/* should end all loops */
	 noexpand = 1;		/* close all other pending } */
      } else {
	 push("\n",S_NORMAL);	/* leave a carriage return */
      }
      sm_interrupt = 1;		/* return control at next convenient moment */
   }

   (void)signal(SIGINT,hand_C);		/* reset signal trap */
}

/****************************************/
static char *panic_save();		/* save everything to a temp file */

void
hand_ill(sig,code)                       /* Illegal operation */
int sig,				/* the signal */
    code;				/* and associated code */
{
   (void)signal(SIGILL,(void (*)P_ARGS((int)))hand_ill); /* reset the trap */
   hand_C(0);				/* clean up the parser */

   switch (code) {
#ifdef vax
    case ILL_PRIVIN_FAULT:
      msg("Reserved instruction fault\n");
      break;
    case ILL_RESOP_FAULT:
      msg("Reserved operand fault\n");
      break;
    case ILL_RESAD_FAULT:
      msg("Reserved addressing fault\n");
      break;
#endif
#ifdef ILL_PRIVVIO_FAULT
   case ILL_PRIVVIO_FAULT:
     msg("Privilege violation\n");
     break;
#endif
    default:
      msg_1d("Unknown code to SIGILL: 0x%x\n",code);
      break;
   }

   msg_1s("I'll try to recover, but cross your fingers. Saving to %s\n",
       								panic_save());
   yyerror("misc error");		/* get traceback */
   longjmp(env,1);
}

/******************************************************/
/*
 * Under unix, do a vfork then an abort to produce a coredump.
 * Under VMS, play silly games with a non-fatal error SPECIAL_SIGNAL
 * so as to produce a traceback
 */
void
hand_acc(sig)		/* we've found an access violation/ bus error */
int sig;		/* the signal */
{
   (void)signal(sig,hand_acc);		/* re-install handler */

#if defined(SIGBUS)
   if(sig == SIGBUS) {
      msg("Bus Error\n");
      msg_2s("We'll try to recover, but this is serious. %s%s\n",
					     	    "Saving to ",panic_save());
   }
#endif
   if(sig == SIGSEGV) {
      msg("Segmentation Violation\n");
      msg_2s("We'll try to recover, but this is no laughing matter. %s%s\n",
					     	    "Saving to ",panic_save());
   }
#if !defined(__BORLANDC__) && !defined(VMS) /* all is well */
   msg("I'll write a core file for you\n");
   if(fork() == 0) {	/* child */
      abort();
   }
#endif
#if defined(vms)
#  define SPECIAL_SIGNAL 0  /* must be a VMS "warning", to produce traceback */

   if(sig != SPECIAL_SIGNAL) {
      lib$signal(SPECIAL_SIGNAL);
      msg("\n\nThe real traceback is the second one\n");
   } 
   yyerror("misc error");		/* get traceback */
#endif
   longjmp(env, 1);
}
/*
 * After a segmentation violation the i/o system may not work, so you
 * can't use SAVE. We'll save for you with this routine
 */
static char *
panic_save()
{
   char	*temp_dir;
   static char file[80];

   if(!saving) {
      saving = 1;
      
      if(*(temp_dir = print_var("temp_dir")) == '\0') temp_dir = "";
      sprintf(file,"%spanic.dmp",temp_dir);
      
      save(file,111);			/* save everything */
   }
      
   return(file);
}

/*****************************************************************************/
/*
 * tell SM about a new window size
 */
void
hand_winch(sig)
int sig;
{
   set_term_type((char *)NULL, 0);
}
