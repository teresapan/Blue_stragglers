#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "tty.h"
#include "stdgraph.h"
#include "sm_declare.h"
#include "charset.h"

#define BTOI(X) (X)
#define TO_INTEG(X) ( (X) - '0' )

/*
STG_ENCODE -- Table driven binary encoder/decoder.  The encoder (which can
also decode) processes a format string, also referred to as a program, to
either encode an output string or decode an input string.  Internally the
encoder operates in two modes, copy mode and execute mode.  In copy mode
all format characters are copied to the output except the following special
characters:

	'	escape next character (literal)	    (this ' is for your editor)
	%	begin a formatted output string
	(	switch to execute mode (stack driven, RPN interpreter)

An ( appearing in the format string causes a mode switch to execute mode.
In execute mode characters are metacode instructions to be executed.  An
unescaped ) causes reversion to copy mode.  Parens may not be nested; an
( in execute mode is an instruction to push the binary value of ( on the
stack, and an ) in copy mode is copied to the output as a character.  In
execute mode the following characters are recognized as special instructions.
All other characters are instructions too, telling the encoder to push the
ASCII value of the character on the stack.

	'	escape next character (recognized everywhere)  (this ' is
 							       for your editor)
	%	formatted output
	)	revert to copy mode
	#nnn	push signed decimal integer number nnn
	$ 	switch case construct
	.	pop number from stack and place in output string
	,	get next character from input string and push on stack
	`str`	prompt with str, get next character from keyboard,
  		and push it onto stack
	&	modulus (similar to AND of low bits)
	+	add (similar to OR)
	-	subtract (similar to AND)
	*	multiply (shift left if pwr of 2)
	/	divide (shift right if pwr of 2)
	<	less than (0=false, 1=true)
	> 	greater than (0=false, 1=true)
	=	equals (0=false, 1=true)
	;	branch if: <bool> <offset> ;.  The ; is at offset zero.
       0-9	push register
	!N	pop stack into register N
	!!	pop N from stack and output an N millisecond delay

The encoder communicates with the outside world via three general purpose
data structures.

	registers	0-9 (integer only)
	memory		char array
	program		char array

The registers are used for parameter input and output as well as for storing
intermediate results.  R 1-3 are used for input and output arguments.  R 4-9
and R0 (R10) are reserved for use by the program.  R11 is the i/o pointer into
encoder memory, used for character input and output.  R12 should contain the
maximum memory address upon input.  Memory may be used for anything but is
normally used only for the input string or output string.  The program is the
format string.

Further documentation is in the stdgraph appendix to the SM manual.
*/

#define	SZ_FORMAT	10		/* max length printf format */
#define	SZ_NUMSTR	10		/* encoded numeric string */

#define	R0	registers[0]		/* scratch */
#define	R1	registers[1]		/* argument */
#define	R2	registers[2]		/* argument */
#define	R3	registers[3]		/* argument */
#define	R4	registers[4]		/* scratch */
#define	R5	registers[5]		/* scratch */
#define	R6	registers[6]		/* scratch */
#define	R7	registers[7]		/* scratch */
#define	R8	registers[8]		/* scratch */
#define	R9	registers[9]		/* scratch */
#define	IOP	registers[E_IOP].i	/* i/o pointer into encoder memory */
#define	TOP	registers[E_TOP].i	/* max memory location */

#define	INPUT(X)							\
   if(memory[iop] == '\0') {						\
      (X) = get1char(0);						\
   } else {								\
      (X) = memory[iop++];						\
      if(iop > TOP) goto memory_overflow;				\
   }
#define	OUTPUT(X)							\
   memory[iop++] = (X);							\
   if(iop > TOP) goto memory_overflow;
#define	PUSH(X) stack[sp++]=(X)
#define	PUSH_I(X) stack[sp++].i=(X)
#define	POP(X) (X)=stack[--sp]
#define	POP_I(X) (X)=stack[--sp].i

#define STACK_OVERFLOW -1
#define STACK_UNDERFLOW -2
#define MEMORY_OVERFLOW -3

static char *pc;
static REGISTER stack[LEN_STACK+1];
static int sp, iop, top, incase;
static void sge_printf();
/*
 * STG_ENCODE -- Interpret a program, encoding values passed in registers into
 * memory, or decoding memory into registers.
 */

int
stg_encode (program, memory, registers)
char	program[];			/* program to be executed */
char	memory[];			/* data space */
REGISTER *registers;			/* general purpose registers */
{
   char extra;
   int x, y, ch, status;
   REGISTER reg;

   incase = NO;
   iop = IOP;
   top = TOP;
   pc = program;
   sp = 0;
/*
 * Process a format string
 */
	for (ch = *pc++;ch != '\0';ch = *pc++) {
	    if (ch == '%' && *pc != '\0') {
		if(*pc == 't') {	/* Tek format */
		   pc++;
		   x = R1.i;
		   y = R2.i;
		   if(iop > TOP - 4) goto memory_overflow;

		   memory[iop++] = ((y >> 5) & 037) | 040;
		   memory[iop++] = (y & 037) | 0140;
		   memory[iop++] = ((x >> 5) & 037) | 040;
		   memory[iop++] = (x & 037) | 0100;
		} else if(*pc == 'T') {		/* 12-bit tektronix */
		   pc++;
		   x = R1.i;
		   y = R2.i;
		   if(iop > TOP - 5) goto memory_overflow;

		   extra = ((y & 03) << 2) | (x & 03) | 0140;
		   x >>= 2; y >>= 2;
		   memory[iop++] = ((y >> 5) & 037) | 040;
		   memory[iop++] = extra;
		   memory[iop++] = (y & 037) | 0140;
		   memory[iop++] = ((x >> 5) & 037) | 040;
		   memory[iop++] = (x & 037) | 0100;
		} else {
		    /* Extract a general format specification and use it to */
		    /* encode the number on top of the stack. */
		    POP(reg);
		    if (sp < 0) {
			IOP = iop;
			msg_2s("Encoder stack underflow at \"%s\" in \"%s\"\n",
				       			       pc,program);
			return (STACK_UNDERFLOW);
		    } else
			sge_printf(reg, memory);
		}

	    } else if (ch == '(' && *pc != '\0') {
		/* Switch to execute mode. */
		status = sge_execute (program, memory, registers);
		if (status != OK)
		    return (status);

	    } else if (ch == '\'' && *pc != '\0') {
		/* Escape next character. */
		OUTPUT(*pc);
		pc++;

	    } else {
		/* Copy an ordinary character to the output string. */
		OUTPUT (ch);
	    }
	}

	IOP = iop;
	return (OK);

memory_overflow:
	IOP = iop;
	msg_2s("Memory overflow in encoder at \"%s\" in \"%s\"\n",pc,program);
	return (MEMORY_OVERFLOW);
}

/*****************************************************************************/
/*
 * SGE_EXECUTE -- Execute a metacode program stored in encoder memory starting
 * at the location of the PC.  The stack, program counter, i/o pointer, and
 * registers are external to functions, and are shared by the copy and execute
 * mode procedures 
 */
int
sge_execute(program, memory, registers)
char *program;				/* program to be executed */
char *memory;				/* data space */
REGISTER *registers;			/* general purpose registers */
{
   char extra;
   int num, ch, neg, x, y, msec;
   REGISTER reg,a,b;
/*
 * successive single character instructions until either ) or
 * is seen.  On a good host machine this case will be compiled as
 * vectored goto with a loop overhead of only a dozen or so machine
 * per loop
 */
	for(ch = *pc++;ch != '\0';ch = *pc++) {
	    switch (ch) {
	    case '\'':
		/* Escape next character (recognized everywhere). */
		ch = *pc;
		if (ch != '\0') {
		    /* Push ASCII value of character. */
		    PUSH_I(ch);
		    pc++;
		}
	        break;
	    case '%':
		if(*pc == 't') {		/* Tek format again. */
		   pc++;
		   x = R1.i;
		   y = R2.i;
		   if (iop > TOP - 4) goto memory_overflow;
		   
		   memory[iop++] = ((y >> 5) & 037) | 040;
		   memory[iop++] = (y & 037) | 0140;
		   memory[iop++] = ((x >> 5) & 037) | 040;
		   memory[iop++] = (x & 037) | 0100;
		} else if(*pc == 'T') {		/* 12-bit tektronix */
		   pc++;
		   x = R1.i;
		   y = R2.i;
		   if(iop > TOP - 5) goto memory_overflow;

		   extra = ((y & 03) << 2) | (x & 03) | 0140;
		   x >>= 2; y >>= 2;
		   memory[iop++] = ((y >> 5) & 037) | 040;
		   memory[iop++] = extra;
		   memory[iop++] = (y & 037) | 0140;
		   memory[iop++] = ((x >> 5) & 037) | 040;
		   memory[iop++] = (x & 037) | 0100;
		} else {		    /* Formatted output. */
		   if (*pc != '\0') {
		      POP(reg);
		      sge_printf(reg, memory);
		   } else
		      OUTPUT (ch);
		}
	        break;
	    case ')':		/* End interpreter mode. */
		return (OK);
	    case '#':		/* Push signed decimal integer number */
		neg = NO;
		if (*pc == '-') {
		    neg = YES;
		    pc++;
		}
		num = 0;
		while (isdigit(*pc)) {
		    num = num * 10 + TO_INTEG(*pc);
		    pc++;
		}

		if (neg == YES)
		    PUSH_I(-num);
		else
		    PUSH_I(num);

	        break;
	    case '$':		/* Switch case instruction. */
		if (incase == NO) {

		    POP(reg);
		    num = reg.i;
/*
 * Search for case number 'num'
 */
		    for(ch = *pc;ch != '\0';ch = *pc) {
			if (ch == '$') {	/* End of switch statement. */
			    pc++;
			    incase = NO;
			    goto end_case_dollar;
			} else if(ch == 'D') { /* default case */
			    pc++;
			    incase = YES;
			    goto end_case_dollar;
			}

			if(*pc == '#') {
			   pc++;
			   for(x = 0;isdigit(*pc);) {
			      x = x*10 + TO_INTEG(*pc++);
			   }
			} else {
			   x = TO_INTEG(*pc++);
			}
			   
			if(*pc == '-') { /* range of cases */
			   pc++;
			   if(*pc == '#') {
			      pc++;
			      for(y = 0;isdigit(*pc);) {
				 y = y*10 + TO_INTEG(*pc++);
			      }
			   } else {
			      y = TO_INTEG(*pc++);
			   }
			   if(x <= num && num <= y) {
			      incase = YES;
			      goto end_case_dollar;
			   }
			} else if(x == num) { /* requested case. */
			    incase = YES;
			    goto end_case_dollar;
			}
/*
 * Advance to the next case.  Leave pc pointing to the N of case $N
 */
			if (ch != '$' && incase == NO) {
			    while (*pc != '\0' && *pc != '$')
				pc++;
			    if (*pc == '$')
				pc++;
			}
		    }
		} else {	/* i.e. INCASE == TRUE */
/*
 * A $ was encountered delimiting a case.  Search forward for $$ or EOS
 */
		    if (*pc != '$') {
			for(ch = *pc++;  ch != '\0';ch = *pc++) {
			    if (ch == '$' && *pc == '$')
				break;
			}
		    }
		    if(*pc == '$') pc++;

		    incase = NO;
		}
		end_case_dollar: ;
	        break;
	    case '|':		/* round the bottom of the stack to an int */
		POP(reg);
		x = reg.f + 0.5;
		PUSH_I(x);
	        break;
	    case '.':		/* Pop number from stack and place in output
	    					string as a binary character */
		POP(reg);
		OUTPUT(reg.i);
	        break;
	    case ',':	/* Get next character from input and push on stack */
		INPUT(num);
		PUSH_I(num);
	        break;
	    case '`':	/* Get next character from keyboard and put on stack */
		while((ch = *pc++) != '\0') {
		   if((ch == '\'' || ch == '\\') && *pc == '`') {
		      ch = *pc++;
		   } else if(ch == '`') {
		      break;
		   }
		   (void)putchar(ch);
		}
		(void)fflush(stdout);
		(void)get1char(CTRL_A);
		num = get1char(0);
		(void)get1char(EOF);
		PUSH_I(num);
	        break;
	    case '&':		/* Modulus (similar to AND of low bits). */
		POP(b);
		POP(a);
		PUSH_I(a.i%b.i);
	        break;
	    case '+':		/* Add (similar to OR). */
		POP(b);
		POP(a);
		PUSH_I(a.i + b.i);
	        break;
	    case '-':		/* Subtract (similar to AND). */
		POP(b);
		POP(a);
		PUSH_I(a.i - b.i);
	        break;
	    case '*':		/* Multiply (shift left if pwr of 2). */
		POP(b);
		POP(a);
		PUSH_I(a.i * b.i);
	        break;
	    case '/':		/* Divide (shift right if pwr of 2). */
		POP(b);
		POP(a);
		PUSH_I(a.i / b.i);
	        break;
	    case '<':		/* Less than (0=false, 1=true). */
		POP(b);
		POP(a);
		PUSH_I(BTOI (a.i < b.i));
	        break;
	    case '>':		/* Greater than (0=false, 1=true). */
		POP(b);
		POP(a);
		PUSH_I(BTOI (a.i > b.i));
	        break;
	    case '=':		/* Equals (0=false, 1=true). */
		POP(b);
		POP(a);
		PUSH_I(BTOI (a.i == b.i));
	        break;
	    case ';':		/* If 2nd value on stack is true add 1st
	    			   value on stack to PC. Example: "12<#-8;".
				   The ; is at offset zero, now at pc-1 */
		POP(a);
		POP(b);
		if (b.i != 0) pc += a.i - 1;
	        break;
	    case '0':
		PUSH(R0);
	        break;
	    case '1':		/* Push contents of register 1. */
		PUSH(R1);
	        break;
	    case '2':		/* Push contents of register 2. */
		PUSH(R2);
	        break;
	    case '3':		/* Push contents of register 3. */
		PUSH(R3);
	        break;
	    case '4':		/* Push contents of register 4. */
		PUSH(R4);
	        break;
	    case '5':		/* Push contents of register 5. */
		PUSH(R5);
	        break;
	    case '6':		/* Push contents of register 6. */
		PUSH(R6);
	        break;
	    case '7':		/* Push contents of register 7. */
		PUSH(R7);
	        break;
	    case '8':		/* Push contents of register 8. */
		PUSH(R8);
	        break;
	    case '9':		/* Push contents of register 9. */
		PUSH(R9);
	        break;
	    case '!':
	    	if (*pc == '!') {	/* !!: Pop stack and delay */
		    pc++;
		    POP_I(msec);
		    ttydelay(g_out,g_tty,msec);
		} else {		    /* !N: Pop stack into register N */
		    num = TO_INTEG(*pc);
		    if (num >= 0 && num <= 9) {
			if (num == 0)
			    num = 10;
			POP(registers[num]);
			pc++;
		    } else
			OUTPUT (ch);
		}
		break;
	    default:		/* Push ASCII value of character. */
		PUSH_I(ch);
		break;
	    }

	    if (sp < 0) {
		msg_2s("Encoder stack underflow at \"%s\" in \"%s\"\n",
						       pc,program);
		return (STACK_UNDERFLOW);
	    } else if (sp >= LEN_STACK) {
		msg_2s("Encoder stack overflow at \"%s\" in \"%s\"\n",
						       pc,program);
		return (STACK_OVERFLOW);
	    }
	}
	pc--;			/* we went one too far */

	return (OK);

memory_overflow:
	msg_2s("Memory overflow in encoder at \"%s\" in \"%s\"\n",pc,program);
	return(MEMORY_OVERFLOW);
}


/*
 * SGE_PRINTF -- Process a %.. format specification.
 * The format is %, followed by a string of digits, -, or ., and delimited
 * by a letter.
 *
 * The number to be encoded has already been popped from the stack
 * into the first argument.  The encoded number is returned in memory
 * at IOP, leaving IOP positioned to the first char following the
 * encoded number.  The format used to encode the number is extracted
 * from the program starting at PC.  PC is left pointing to the first
 * character following the format.
 */
static void
sge_printf(reg, memory)
REGISTER reg;				/* number to be encoded */
char *memory;				/* output buffer */
{
   char fmt[SZ_FORMAT+2];
   char *fptr;
   char numstr[SZ_NUMSTR+2];

   fptr = fmt;
   *fptr = '%';
   while((*++fptr = *pc++) != '\0') {
      if(islower(*fptr)) {
	 break;
      }
   }
   *(fptr + 1) = '\0';

   if(*fptr == 'e' || *fptr == 'f' || *fptr == 'g') {
      if(sprintf(numstr,fmt,reg.f) == 0) { /* maybe NULL, maybe not...*/
	 numstr[0] = '\0';
      }
   } else {
      if(sprintf(numstr,fmt,reg.i) == 0) { /* maybe NULL, maybe not...*/
	 numstr[0] = '\0';
      }
   }
   iop += gstrcpy(&memory[iop],numstr,top - iop + 1);
}

/*
 * STG_CTRL -- Fetch an encoder format string from the graphcap entry and
 * use it to encode zero, one, or two integer arguments into a control string.
 * Put the control string to the output device.
 */
void
stg_ctrl (cap)
char	cap[];		/* name of device capability to be encoded */
{
   char prog[SZ_PROGRAM];
/*
 * Fetch the program from the graphcap file.  Zero is returned if the
 * device does not have the named capability, in which case the function
 * is inapplicable and should be ignored.
 */
	if (ttygets (g_tty, cap, prog, SZ_PROGRAM) > 0) {
	    /* Encode the output string and write the encoded string to the */
	    /* output file. */
	    g_reg[E_IOP].i = 0;
	    if (stg_encode (prog, g_mem, g_reg) == OK) {
		g_mem[g_reg[E_IOP].i] = '\0';
		ttyputs (g_out, g_tty, g_mem, g_reg[E_IOP].i, 1);
	    }
	}
}

/* STG_CTRL1 -- Encode one integer argument. */

void
stg_ctrl1(cap, arg1)
char	cap[];		/* device capability */
int	arg1;
{
	g_reg[1].i = arg1;
	stg_ctrl (cap);
}


/* STG_FCTRL1 -- Encode one floating argument. */

void
stg_fctrl1(cap, arg1)
char	cap[];		/* device capability */
double	arg1;
{
	g_reg[1].f = arg1;
	stg_ctrl (cap);
}


/* STG_CTRL2 -- Encode two integer arguments. */

void
stg_ctrl2 (cap, arg1, arg2)
char	cap[];		/* device capability */
int	arg1, arg2;
{
	g_reg[1].i = arg1;
	g_reg[2].i = arg2;
	stg_ctrl (cap);
}


/* STG_CTRL3 -- Encode three integer arguments. */

void
stg_ctrl3 (cap, arg1, arg2, arg3)
char	cap[];		/* device capability */
int	arg1, arg2, arg3;
{
	g_reg[1].i = arg1;
	g_reg[2].i = arg2;
	g_reg[3].i = arg3;
	stg_ctrl (cap);
}
