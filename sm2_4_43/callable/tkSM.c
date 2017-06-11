/*
 * A test application for SM's TK device. After building the executable,
 * you can say
 *      tkSM smtk.tcl
 * to generate a Tk application with two SM widgets in it.
 *
 * We imagine that most real applications will want to steal some or all
 * of the code in this file; feel free -- we hereby release this file
 * into the public domain
 */
#include <stdlib.h>
#include <ctype.h>
#include "tcl.h"
#include "tk.h"
#include "sm_declare.h"

/*
 * Declaration for the SM widget's class command procedure:
 */
extern int sm_create_tk _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int argc, char *argv[]));

/*****************************************************************************/

int
main(ac, av)
int ac;
char **av;
{
    sm_device("nodevice");
    Tk_Main(ac, av, Tcl_AppInit);

    return 0;				/* suppress compiler warning */
}

/*****************************************************************************/
/*
 * helper function to get exactly one REAL value
 */
static int
do_REAL(interp, ac, av, val)
Tcl_Interp *interp;
int ac;
char **av;
REAL *val;				/* value to return */
{
   if(ac < 2) {
      Tcl_AppendResult(interp, av[0], " needs a number", (char *)NULL);
      return(TCL_ERROR);
   } else if(ac > 2) {
      Tcl_AppendResult(interp, "Extra argument to ", av[0], " ", av[1],
		       " : ", av[2], (char *)NULL);
      return(TCL_ERROR);
   }

   if(!isdigit(av[1][0]) && av[1][0] != '.' && av[1][0] != '-') {
      Tcl_AppendResult(interp, "Invalid argument to ", av[0], " : ", av[1],
		       (char *)NULL);
      return(TCL_ERROR);
   }
	  
   *val = atof(av[1]);

   return(TCL_OK);
}

/*****************************************************************************/
/*
 * Functions to do the work of this "application"
 */

/*****************************************************************************/

static int
do_angle(cd, interp, ac, av)
ClientData cd;				/* any info passed from the client */
Tcl_Interp *interp;			/* the TCL interpreter */
int ac;
char **av;
{
   REAL val;
   
   if(do_REAL(interp, ac, av, &val) != TCL_OK) {
      return(TCL_ERROR);
   }

   sm_angle(val);

   return(TCL_OK);
}

/*****************************************************************************/

static int
do_ctype(cd, interp, ac, av)
ClientData cd;				/* any info passed from the client */
Tcl_Interp *interp;			/* the TCL interpreter */
int ac;
char **av;
{
   if(ac < 2) {
      Tcl_AppendResult(interp, "ctype needs a colour name", (char *)NULL);
      return(TCL_ERROR);
   } else if(ac > 2) {
      Tcl_AppendResult(interp, "Extra argument to ctype", av[1], " : ", av[2],
		       (char *)NULL);
      return(TCL_ERROR);
   }
	  
   sm_ctype(av[1]);

   return(TCL_OK);
}
     
static int
do_erase(cd, interp, ac, av)
ClientData cd;				/* any info passed from the client */
Tcl_Interp *interp;			/* the TCL interpreter */
int ac;
char **av;
{
   if(ac > 1) {
      Tcl_AppendResult(interp, "Extra argument to erase: ", av[1],
		       (char *)NULL);
      return(TCL_ERROR);
   }

   sm_erase();

   return(TCL_OK);
}

/*****************************************************************************/

static int
do_expand(cd, interp, ac, av)
ClientData cd;				/* any info passed from the client */
Tcl_Interp *interp;			/* the TCL interpreter */
int ac;
char **av;
{
   REAL val;
   
   if(do_REAL(interp, ac, av, &val) != TCL_OK) {
      return(TCL_ERROR);
   }

   sm_expand(val);

   return(TCL_OK);
}

/*****************************************************************************/

static int
do_ltype(cd, interp, ac, av)
ClientData cd;				/* any info passed from the client */
Tcl_Interp *interp;			/* the TCL interpreter */
int ac;
char **av;
{
   if(ac < 2) {
      Tcl_AppendResult(interp, "ltype needs a number", (char *)NULL);
      return(TCL_ERROR);
   } else if(ac > 2) {
      Tcl_AppendResult(interp, "Extra argument to ltype", av[1], " : ", av[2],
		       (char *)NULL);
      return(TCL_ERROR);
   }

   if(!isdigit(av[1][0])) {
      Tcl_AppendResult(interp, "Invalid argument to ltype: ", av[1],
		       (char *)NULL);
      return(TCL_ERROR);
   }
	  
   sm_ltype(atoi(av[1]));

   return(TCL_OK);
}

/*****************************************************************************/

static int
do_lweight(cd, interp, ac, av)
ClientData cd;				/* any info passed from the client */
Tcl_Interp *interp;			/* the TCL interpreter */
int ac;
char **av;
{
   REAL val;
   
   if(do_REAL(interp, ac, av, &val) != TCL_OK) {
      return(TCL_ERROR);
   }
   
   sm_lweight(val);

   return(TCL_OK);
}

/*****************************************************************************/

static int
do_ptype(cd, interp, ac, av)
ClientData cd;				/* any info passed from the client */
Tcl_Interp *interp;			/* the TCL interpreter */
int ac;
char **av;
{
   REAL val;
   
   if(ac < 3) {
      Tcl_AppendResult(interp, "ptype needs two numbers", (char *)NULL);
      return(TCL_ERROR);
   } else if(ac > 3) {
      Tcl_AppendResult(interp, "Extra argument to ptype", av[1], " ", av[2],
		       " : ", av[3], (char *)NULL);
      return(TCL_ERROR);
   }

   if(!isdigit(av[1][0])) {
      Tcl_AppendResult(interp, "Invalid argument to ptype: ", av[1],
		       (char *)NULL);
      return(TCL_ERROR);
   }
   if(!isdigit(av[2][0])) {
      Tcl_AppendResult(interp, "Invalid argument to ptype: ", av[2],
		       (char *)NULL);
      return(TCL_ERROR);
   }

   val = 10*atof(av[1]) + atof(av[2]);
   sm_ptype(&val, 1);

   return(TCL_OK);
}

/*****************************************************************************/
/*
 * a test plot
 */
static int
do_test(ClientData cd,			/* any info passed from the client */
	 Tcl_Interp *interp,		/* the TCL interpreter */
	 int ac,
	 char **av)
{
   if(ac < 2) {
      Tcl_AppendResult(interp, "test needs an argument", (char *)NULL);
      return(TCL_ERROR);
   } else if(ac > 2) {
      Tcl_AppendResult(interp, "Extra argument to test", av[1], " : ", av[2],
		       (char *)NULL);
      return(TCL_ERROR);
   }

   if(strcmp(av[1],"box") == 0) {
      sm_box(1, 2, 0, 0);
   } else if(strcmp(av[1],"cross") == 0) {
      sm_relocate(0.1, 0.1);
      sm_draw(0.9, 0.9);
      sm_relocate(0.1, 0.9);
      sm_draw(0.9, 0.1);
   } else if(strcmp(av[1],"dot") == 0) {
      sm_relocate(0.5, 0.5);
      sm_dot();
   } else if(strcmp(av[1],"square") == 0) {
      sm_relocate(0.1, 0.1);
      sm_draw(0.1, 0.9);
      sm_draw(0.9, 0.9);
      sm_draw(0.9, 0.1);
      sm_draw(0.1, 0.1);
   } else {
      Tcl_AppendResult(interp, "Unknown test: ", av[1],
		       (char *)NULL);
      return(TCL_ERROR);
   }

   sm_gflush();

   return(TCL_OK);
}

/*****************************************************************************/
/*
 * Setup TCL and TK
 */
int
Tcl_AppInit(interp)
Tcl_Interp *interp;			/* Interpreter for application. */
{
   if (Tcl_Init(interp) == TCL_ERROR) {
      return(TCL_ERROR);
   }
   if (Tk_Init(interp) == TCL_ERROR) {
      return(TCL_ERROR);
   }

   if(Tk_MainWindow(interp) == NULL) {
      return(TCL_ERROR);
   }
/*
 * Create the SM widget, and an application command or two
 */
    Tcl_CreateCommand(interp, "sm", sm_create_tk,
		      (ClientData)Tk_MainWindow(interp),
		      (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateCommand(interp, "angle", do_angle,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    Tcl_CreateCommand(interp, "ctype", do_ctype,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    Tcl_CreateCommand(interp, "erase", do_erase,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    Tcl_CreateCommand(interp, "expand", do_expand,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    Tcl_CreateCommand(interp, "ltype", do_ltype,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    Tcl_CreateCommand(interp, "lweight", do_lweight,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    Tcl_CreateCommand(interp, "ptype", do_ptype,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateCommand(interp, "test", do_test,
		      (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
/*
 * Startup file for application
 */
#if TCL_MAJOR_VERSION >= 8
   Tcl_SetVar(interp, "tcl_rcFileName", "~/.wishrc", TCL_GLOBAL_ONLY);
#else
    tcl_RcFileName = "~/wishrc";
#endif
   
    return(TCL_OK);
}
