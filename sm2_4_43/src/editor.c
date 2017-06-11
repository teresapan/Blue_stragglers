#include "copyright.h"
/*
 * these routines do emacs-style editing of a line of text.
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(POSIX_TTY)
#  define POSIX _POSIX_SOURCE
#  undef _POSIX_SOURCE			/* we need TIOCGWINSZ */
#  include <termios.h>
#  define _POSIX_SOURCE POSIX
#endif
#ifdef unix
#   include <sys/ioctl.h>
#endif
#if defined(__MSDOS__)
#  include <conio.h>
#endif
#include "charset.h"
#include "edit.h"
#include "tty.h"
#include "devices.h"
#include "sm_declare.h"

#define ADD_CHAR(X)			/* add a character */		\
{									\
   if(overwrite) {							\
      *lptr++ = putchar((X) & '\177');	/* overwriting is simple */	\
   } else {								\
      if(insert_char(X,line,&lptr,HSIZE) == END) { /* insert character */\
         FPUTCHAR(X);							\
      } else {					/* middle of line */	\
	 printf("%s",lptr - 1);			/* and rewrite it */	\
	 UPDATE_CURSOR;							\
      }									\
   }									\
}

#define DELETE_CHAR		/* delete a character under cursor */	\
del_character(lptr);							\
if(term.del_char[0] != '\0') {	/* that's easy */			\
   PRINT(term.del_char);						\
} else {			/* Can't delete just one character */	\
   delete_to_eol(lptr);							\
   printf("%s",lptr);		/* redraw end of line*/			\
   UPDATE_CURSOR;							\
}

#define DEL_CHAR_D		/* delete character under cursor with ^D*/ \
   undo_buff[uind%USIZE] = *lptr; /* save deleted character */		\
   undo_buff[uind%USIZE] |= '\200';					\
   uind++;								\
   DELETE_CHAR;								\

#define DEL_CHAR_H		/* delete character under cursor with ^H */ \
   undo_buff[uind%USIZE] = *lptr; /* save deleted character */		\
   uind++;								\
   DELETE_CHAR;

#define PRINT(S)		/* process padding and print string */	\
   {									\
     char *ptr = S;							\
     if(isdigit(*ptr)) {						\
        delay(atof2(ptr));						\
        while(isdigit(*ptr)) ptr++;					\
        if(*ptr == '.') {						\
	   ptr++;							\
           while(isdigit(*ptr)) ptr++;					\
        }								\
        if(*ptr == '*') ptr++;						\
     }									\
     printf("%s",ptr);							\
   }

#define UPDATE_CURSOR							\
   if(mv_cursor(lptr - line, 0) < 0) {					\
      if(*lptr != '\0') {						\
         printf("\r%s ",prompt);					\
         nprintf(line,lptr - line);					\
      }									\
   }

#define YANK_CHAR		/* yank back a character */		\
{									\
   char yank_c;								\
									\
   if(uind <= 0) {							\
      putchar(CTRL_G);							\
      break;								\
   }									\
   yank_c = undo_buff[--uind%USIZE];					\
   ADD_CHAR(yank_c);							\
   if(yank_c & '\200') {	/* character came from under cursor */	\
      putchar('\b'); lptr--;						\
   }									\
}
#define END 0				/* pointer at end of line */
#define MIDDLE 1			/* pointer not at end */
#define NKILL 5				/* number of lines in ^K kill ring */
#define NOWT 0				/* just get character from get1char */
#define FPUTCHAR(C)			/* putchar + fflush */		\
  putchar((C) & '\177');						\
  (void)fflush(stdout);
#define UNBOUND (-1)			/* key is not bound */
#define USIZE 160			/* number of characters to save */

extern char cchar;			/* comment character */
extern char prompt[];
static char save_buffer[NKILL][HSIZE + 1], /* Buffer for ^K/^Y */
	    undo_buff[USIZE];		/* buffer for deleted characters */
int last_char;				/* last char that was typed */
extern int sm_verbose;			/* should I be chatty? */
extern int no_editor;			/* use fgets() to read input? */
static int insert_char(),
	   kill_ind = NKILL,		/* index into save_buffer kill ring */
	   kill_ring_ctr = NKILL,	/* where is ESC-y in kill ring? */
	   list_edit_tree(),
	   mv_cursor(),
	   overwrite = 0,
	   uind = 0;			/* index to undo_buff */
extern int sm_interrupt,
	   plength;			/* num. of printing chars in prompt */
extern TERMINAL term;			/* describe my terminal */
static void delete_to_eol(),		/* delete to end of line */
  	    delay(),			/* delay for x msec */
	    del_character(),
	    get_term_size(),
	    nprintf();

/*****************************************************************/
/*
 * Set the terminal type, for the editor. This is nothing to do with graphics.
 */
#define NOMOVE 0			/* Can't get ch or cm to move cursor */
#define CH 1				/* got "ch" */
#define CM 2				/* got "cm" */

TERMINAL term;				/* defined in edit.h */
int curs_prop,				/* How to move the terminal */
    nlines;

int
set_term_type(term_type,nl)
char *term_type;
int nl;				/* number of lines */
{
   char *baud_rate,
   	buff[TERM_SIZE],
   	*cap_file,
        *ptr,
	*s_cap_file = NULL;		/* space for cap_file */
   char *esc_chars = "\004bdfghuvy<>";	/* recognise ESC-char */
   static int first = 1;		/* Is this the first call? */
   static int no_editor0;		/* initial value of no_editor */
   static TTY *tty;
/*
 * Map things like word-operators
 */
   if(first) {
      char str[4],
      	   *ec;				/* pointer to esc_chars */
      
      first = 0;
      no_editor0 = no_editor;
      for(ec = esc_chars;*ec != '\0';ec++) {
	 sprintf(str,"^[%c",*ec);
	 (void)define_key(str,*ec | '\200');
      }
      (void)define_key("^[q",CTRL_Q);
      (void)define_key("^[s",CTRL_S);
   }

   if(term_type == NULL) {
      get_term_size();
      return(-1);
   }

   reset_term();
   if(strcmp(term_type,"none") == 0) {	/* turn off line editing */
      no_editor = 1;
      return(0);
   }

   no_editor = no_editor0;		/* revert to initial line edit state */
   if(strcmp(term_type,"dumb") == 0) {	/* worst case terminal */
      term.forw_curs[0] = '\0';
      curs_prop = NOMOVE;
      term.del_char[0] = '\0';
      term.del_line[0] = '\0';
      term.ncol = 80;
      term.nlin = 0;
      nlines = (nl != 0) ? abs(nl) : 20;
      return(0);
   }

   if((cap_file = getenv("TERMCAP")) != NULL) {    /* in the environment */
      int len = strlen(cap_file);
      if((s_cap_file = malloc(len + 1 + 100 + 1)) == NULL) {
	 msg("Can't allocate storage for s_cap_file\n");
	 return(-1);
      }
      strcpy(s_cap_file,cap_file);
      if((cap_file = get_val("termcap")) == NULL) { /* try .sm instead */
	 cap_file = "/etc/termcap";
      }
      s_cap_file[len] = '\177';		/* mark end of $TERMCAP entry */
      strncpy(&s_cap_file[len + 1],cap_file,100);
      s_cap_file[len + 1 + 100] = '\0';	/* play safe */
      cap_file = s_cap_file;
   } else {
      if((cap_file = get_val("termcap")) == NULL) { /* try .sm instead */
         cap_file = "/etc/termcap";
      }
   }
   if((tty = ttyopen(cap_file,1,&term_type,(int (*)())NULL)) == NULL) {
      return(-1);
   }
   tty_index_caps(tty,tty->t_capcode, tty->t_capindex);
   if(tty->t_ncaps == MAX_CAPS) {
       msg_2s("while searching for \"%s\" in \"%s\"\n", term_type, cap_file);
   }
   if(s_cap_file != NULL) free(s_cap_file);
/*
 * Now get the capabilities that we need
 */
   (void)ttygets(tty,"ch",term.forw_curs,TERM_SIZE);	/* look for "ch" */
   if(!strcmp(term.forw_curs,"disabled")) {
      term.forw_curs[0] = '\0';
   } else if(term.forw_curs[0] == '\0') {		/* settle for "cm" */
      (void)ttygets(tty,"cm",term.forw_curs,TERM_SIZE);
      curs_prop = CM;
   } else {
      curs_prop = CH;
   }
   if(nl < 0 || term.forw_curs[0] == '\0') {
      curs_prop = NOMOVE;
   }

   (void)ttygets(tty,"dc",term.del_char,TERM_SIZE);
   (void)ttygets(tty,"ce",term.del_line,TERM_SIZE);
   if((ptr = getenv("COLUMNS")) != NULL) {
      term.ncol = atoi(ptr);
   } else {
      term.ncol = ttygeti(tty,"co");
   }
   if((ptr = getenv("LINES")) != NULL) {
      term.nlin = atoi(ptr);
   } else {
      term.nlin = ttygeti(tty,"li") ;
   }
   term.pad = ttygeti(tty,"pc") ;
   if(*(baud_rate = print_var("ttybaud")) != '\0') {
      term.baud = atoi(baud_rate);
   } else {
      term.baud = DEF_BAUDRATE;
   }

   get_term_size();
   nlines = (nl != 0) ? abs(nl) : (term.nlin > 0) ? term.nlin - 1 : 20;
   if(curs_prop == CM) {
      term.dlin = nlines;
   }
/*
 * Initialise the terminal
 */
   (void)ttygets(tty,"is",buff,TERM_SIZE);
   PRINT(buff);
   (void)mv_cursor(-plength,term.dlin);	/* go to column 1 allowing for prompt*/
/*
 * Get the key init sequences, bind the arrow keys
 */
   (void)ttygets(tty,"ke",term.unset_keys,TERM_SIZE);
   (void)ttygets(tty,"ks",term.set_keys,TERM_SIZE);
   set_term();
   (void)ttygets(tty,"kl",buff,TERM_SIZE); (void)define_key(buff,CTRL_B);
   (void)ttygets(tty,"kr",buff,TERM_SIZE); (void)define_key(buff,CTRL_F);
   (void)ttygets(tty,"kd",buff,TERM_SIZE); (void)define_key(buff,CTRL_N);
   (void)ttygets(tty,"ku",buff,TERM_SIZE); (void)define_key(buff,CTRL_P);
   (void)ttygets(tty,"k1",buff,TERM_SIZE); (void)set_pf_key(buff,1);
   (void)ttygets(tty,"k2",buff,TERM_SIZE); (void)set_pf_key(buff,2);
   (void)ttygets(tty,"k3",buff,TERM_SIZE); (void)set_pf_key(buff,3);
   (void)ttygets(tty,"k4",buff,TERM_SIZE); (void)set_pf_key(buff,4);
   return(0);
}

static void
get_term_size()
{
#if defined(TIOCGWINSZ) && !defined(rs6000)
   static int fildes = -1;
   struct winsize twinsiz;

   twinsiz.ws_col = 0;
   twinsiz.ws_row = 0;
   if((fildes = open("/dev/tty",2)) >= 0) {
      (void)ioctl(fildes,TIOCGWINSZ,(char *)&twinsiz);
   }
   term.ncol = (twinsiz.ws_col == 0) ? term.ncol : twinsiz.ws_col;
   term.nlin = (twinsiz.ws_row == 0) ? term.nlin : twinsiz.ws_row;
#endif
#if defined(__MSDOS__)
   if( term.nlin <= 0 ) {
      int oldcur;
      asm {
	 mov ah, 0fh
	 int 10h
	 mov ah, 03h
	 int 10h
	 mov oldcur, dx
      }

      for(term.nlin = 100;term.nlin;term.nlin--) {
	 printf("\033[%d;0H", term.nlin );
	 asm {
	    mov ah, 0fh
	    int 10h
	    mov ah, 03h
	    int 10h
	 }
	 if(_DH+1 == term.nlin) break;
      }

      asm {
	 mov ah, 0fh
	 int 10h
	 mov ah, 02h
	 mov dx, oldcur
	 int 10h
      }

      asm {
	 mov ah, 0fh
	 int 10h
      }

      term.ncol = _AH;
   }
#endif
}

/*
 * set or reset the terminal's keypad
 */
void
set_term()
{
   PRINT(term.set_keys);
}

void
reset_term()
{
   PRINT(term.unset_keys);
}

/*
 * Move a cursor to a particular place on the screen. Ideally, the capability
 * "ch" (move within line) is used. Failing that, we use "cm", and go to the
 * `default line', term.dlin. Note that, by definition, "ch" strings won't
 * access this line information, so this routine needn't know if we use it.
 *
 * See Unix manual(5) for discussion of termcap.
 * (this routine fakes their definition of the "ch"/"cm" capability)
 */
static int
mv_cursor(i,row)
int i, row;					/* desired column */
{
   char *cptr;				/* pointer to forw_curs string */
   int j;

   i += plength;		/* allow for cursor */
   j = row;
   if(curs_prop == CM) {
      j = (row == 0 ? term.dlin : row);
   } else if(curs_prop == CH) {
      j = row;
   } else {
      return(-1);
   }
   cptr = term.forw_curs;

#if defined(__MSDOS__)
   if(cptr[0] == '@') {
      gotoxy( i+1, j+1 );
      return	0;
   }
#endif

   if(isdigit(*cptr)) {
      delay(atof2(cptr));
      while(isdigit(*cptr)) cptr++;	/* skip delay */
      if(*cptr == '.') {
	 cptr++;
	 while(isdigit(*cptr)) cptr++;
      }
      if(*cptr == '*') cptr++;
   }

   while(*cptr != '\0') {
      if(*cptr == '%') {
	 switch (*++cptr) {
	  case '%':
	    putchar('%');
	    break;
	  case '.':
	    printf("%c",j);
	    j = i;		/* use i next */
	    break;
	  case '+':
	    j += *++cptr;
	    printf("%c",j);
	    j = i;		/* use i next */
	    break;
	  case '>':
	    if(j > *++cptr) {
	       j += *++cptr;
	    } else {
	       cptr++;
	    }
	    break;
	  case '2':
	    printf("%2d",j);
	    j = i;		/* use i next */
	    break;
	  case '3':
	    printf("%3d",j);
	    j = i;		/* use i next */
	    break;
	  case 'd':
	    printf("%d",j);
	    j = i;		/* use i next */
	    break;
	  case 'i':
	    j++; i++;
	    break;
	  case 'r':
	    { int t; t = j; j = i; i = t;}
	    break;
	  default:
	    fprintf(stderr,"Unknown code in mv_cursor %c\n",*cptr);
	    break;
	 }
      } else {
	 putchar(*cptr);
      }
      cptr++;
   }
   return(0);
}

/*******************************************************************/
/*
 * Edit a line of text, either history or part of a macro
 */
int
edit_line(line,cu_ptr)
char *line,				/* line to be edited */
     **cu_ptr;				/* return current position in line */
{
   char c,
   	*key_mac,			/* pointer to keyboard macro */
   	*lptr;				/* pointer to line line */
   int i,
       tabsize = 0;			/* size of a tab */
   static int cur_position = HSIZE;	/* previous position of cursor */

   if(no_editor) {
      if(no_editor == 1) {
	 printf("%s ",prompt);
	 fflush(stdout);
      }
      if(sm_interrupt) {
	 return(CTRL_C);
      }

      REDRAW(0);			/* give the device a chance */
      
      if(fgets(line,HSIZE - 1,stdin) == NULL) {
	 strcpy(line,"QUIT");
      } else {
	 int len = strlen(line);
	 if(line[len - 1] == '\n') {	/* input fits in line */
	    line[len - 1] = '\0';
	 } else {
	    line[len] = '\\';
	    line[len + 1] = '\0';	/* we left space for this */
	 }
      }
      if(cu_ptr != NULL) *cu_ptr = line;
      return(CTRL_M);
   }

   if(*(lptr = print_var("tabsize")) != '\0') {
      tabsize = atoi(lptr);
   }
   if(tabsize == 0) tabsize = 8;
   
#if !defined(_Windows)	/* bug ?? under windows, this produces a line feed */
   putchar('\r');
#else
   for(i = 0;i < term.ncol;i++) putchar('\b');
#endif
   delete_to_eol((char *)NULL);		/* don't know line */
   printf("%s %s",prompt,line);
/*
 * Try to keep cursor in same column
 */
   i = strlen(line);
   if(i > 0) {
      if(cur_position > i) {		/* don't go beyond end of line */
         cur_position = i;
      }
      lptr = line + cur_position;	/* put cursor in old column */
   } else {
      lptr = line;
   }
   UPDATE_CURSOR;
   (void)fflush(stdout);

   (void)get1char(CTRL_A);			/* initialise rawmode */

   for(;;) {
      if(sm_interrupt) {
	 (void)get1char(EOF);
	 return(CTRL_C);
      }
      switch(i = readc()) {
       case UNBOUND :
	 putchar(CTRL_G);
	 break;
       case CTRL_A :			/* beginning of line */
	 printf("\r%s ",prompt);
	 lptr = line;
	 break;
       case CTRL_B :			/* back a character */
	 if(lptr > line) {
#if defined(_Windows)			/* Bug; use hardware */
	    gotoxy(wherex() - 1,wherey());
#else
	    printf("\b");
#endif
	    lptr--;
	 }
	 break;
       case CTRL_D :			/* delete character under cursor */
	 DELETE_CHAR;
	 break;
       case CTRL_E :			/* go to end of line */
	 printf("%s",lptr);
	 lptr += strlen(lptr);
	 break;
       case CTRL_F :			/* forward a character */
	 if(lptr - line < HSIZE - 1) {
	    if(*lptr == '\0') *lptr = ' ';
	    putchar(*lptr++);
	 }
	 break;
       case CTRL_H :			/* delete character before cursor */
	 if(lptr > line) {
	    lptr--;			/* back one character */
	    putchar('\b');
	    DELETE_CHAR;
	 }
	 break;
       case CTRL_I :			/* tab */
         ADD_CHAR(' ');
         while((lptr - line)%tabsize != 0) ADD_CHAR(' ');
         break;
       case CTRL_K :			/* erase to end of line */
	 (void)strcpy(save_buffer[(++kill_ind)%NKILL],lptr);
	 delete_to_eol(lptr);
	 erase_str(lptr,HSIZE - (lptr - line));
	 kill_ring_ctr = kill_ind;
	 break;
       case CTRL_L :			/* re-write line */
	 printf("\r%s %s",prompt,line);
	 delete_to_eol((char *)NULL);	/* don't know how much to delete */
	 UPDATE_CURSOR;
	 break;
       case CTRL_M :			/* accept line command */
       case CTRL_N :			/* next command */
       case CTRL_P :			/* previous command */
       case CTRL_R :			/* search reverse */
       case CTRL_S :			/* search forward */
       case CTRL_V :			/* forward many lines */
       case ('<' | '\200') :		/* first line */
       case ('>' | '\200') :		/* last line */
       case (CTRL_D | '\200') :		/* forget this line */
       case ('g' | '\200') :		/* go to line */
 	 (void)get1char(EOF);
	 if(uind > 0 && !isspace((undo_buff[(uind - 1)%USIZE]) & '\177')) {
	    if(undo_buff[(uind - 1)%USIZE] & '\200') {
	      undo_buff[(uind++)%USIZE] = (' ' | '\200');
	   } else {
	      undo_buff[(uind++)%USIZE] = ' ';
	   }
	 }
	 
	 if(lptr > line) {		/* only set if line non-null */
	    cur_position = (*lptr == '\0') ? HSIZE : lptr - line;
	 }
	 if(cu_ptr != NULL) {
	    *cu_ptr = lptr;		/* return current position */
	    if(i == CTRL_M && *lptr != '\0') {
	       cur_position = 0;	/* we're going to break line */
	    }
	 }
	 if(i == CTRL_M) putchar('\n');
	 return(i);
       case CTRL_O :			/* insert line above */
	 (void)unreadc(CTRL_M);
	 (void)unreadc(CTRL_E);
	 (void)unreadc(CTRL_P);
	 break;
       case CTRL_Q :			/* Literal next */
	 i = get1char(0) & '\177';
	 ADD_CHAR(i);
	 break;
       case CTRL_T :			/* toggle overwrite mode */
	 overwrite = (overwrite == 1) ? 0 : 1;
	 break;
       case CTRL_U :				/* erase to start of line */
	 while(lptr-- > line) {
	    del_character(lptr);		/* back one character */
	 }
	 lptr++;
	 unreadc(CTRL_L);
	 break;
       case CTRL_X :			/* finish editing */
         (void)get1char(EOF);		/* turn off raw mode */
         cur_position = HSIZE;
	 return(i);
       case CTRL_Y :			/* Restore save buffer */
	 for(i = 0;(c = save_buffer[kill_ind%NKILL][i]) != '\0';i++) {
	    ADD_CHAR(c);
	 }
         break;
       case CTRL_Z :			/* Attach to parent process */
         (void)get1char(EOF);		/* turn off raw mode */
	 reset_term();
	 (void)fflush(stdout);
	 if(sm_suspend() != 0) {		/* try to attach to shell */
	    putchar('\r');
	    delete_to_eol(line);
#if !defined(__BORLANDC__)
	    fprintf(stderr,"Out of raw mode for a moment...\r");
	    for(i = 0;i < 400000;i++) continue;
#endif
	 }
	 (void)get1char(CTRL_A);	/* back into raw mode */
	 set_term();
	 (void)unreadc(CTRL_L);		/* redraw line */
         break;
       case ('b' | '\200'):
	 if(lptr > line) {
	    putchar('\b'); lptr--;
	 }
	 while(lptr > line && isspace(*lptr)) {
	    putchar('\b'); lptr--;
	 }
	 while(lptr > line && !isspace(*lptr)) {
	    putchar('\b'); lptr--;
	 }
	 if(lptr > line) putchar(*lptr++);	/* went too far */
	 break;
       case ('d' | '\200'):		/* delete a word forwards */
	 while(*lptr != '\0' && isspace(*lptr)) {
	    DEL_CHAR_D;
	 }
	 while(*lptr != '\0' && *lptr != '\0' && !isspace(*lptr)) {
	    DEL_CHAR_D;
	 }
	 break;
       case ('f' | '\200'):		/* forward a word */
	 while(*lptr != '\0' && !isspace(*lptr)) {
	    putchar(*lptr++);
	 }
	 while(*lptr != '\0' && isspace(*lptr)) {
	    putchar(*lptr++);
	 }
	 break;
       case ('h' | '\200'):		/* delete a word backwards */
	 if(lptr > line) {
	    putchar('\b'); lptr--;
	    DEL_CHAR_H;
	 }
	 while(lptr > line && isspace(*(lptr - 1))) {
	    putchar('\b'); lptr--;
	    DEL_CHAR_H;
	 }
	 while(lptr > line && *(lptr - 1) != '\0' && !isspace(*(lptr - 1))) {
	    putchar('\b'); lptr--;
	    DEL_CHAR_H;
	 }
	 break;
       case ('u' | '\200'):		/* yank back a word */
	 if(uind <= 0) putchar(CTRL_G);
	 while(uind > 0 && isspace((undo_buff[(uind - 1)%USIZE]) & '\177')){
	    YANK_CHAR;
	 }
	 while(uind > 0 &&
	       ((c = (undo_buff[(uind - 1)%USIZE] & '\177')),c != '\0') &&
							       !isspace(c)) {
	    YANK_CHAR;
	 }
	 break;
       case ('v' | '\200'):		/* back a number of lines */
	 (void)get1char(EOF);
	 cur_position = (*lptr == '\0') ? HSIZE : lptr - line;
	 return(i);
       case ('y' | '\200'):		/* step back through kill ring */
	 kill_ring_ctr += (NKILL - 1);	/* go back one */
	 for(i = 0;(c = save_buffer[kill_ring_ctr%NKILL][i]) != '\0' &&
	     lptr - line < HSIZE - 1;i++) {
	    ADD_CHAR(c);
	 }
         break;
       case ('0' | '\200'):			/* may be keyboard macro */
       case ('1' | '\200'): case ('2' | '\200'): case ('3' | '\200'):
       case ('4' | '\200'): case ('5' | '\200'): case ('6' | '\200'):
       case ('7' | '\200'): case ('8' | '\200'): case ('9' | '\200'):
	 if((key_mac = get_key_macro((i & '\177') - '0')) == NULL) {
	    putchar(CTRL_G);			/* key is not defined */
	 } else {				/* push onto buffer */
	    i = strlen(key_mac) - 1;
	    for(;i >= 0;i--) {
	       (void)unreadc(key_mac[i]);
	    }
	 }
	 break;
       default :
	 if(iscntrl(i)) {			/* invalid key */
	    putchar(CTRL_G);
	    break;
	 }

	 ADD_CHAR(i);
	 break;
      }
      (void)fflush(stdout);
   }
}

/******************************************************/
/*
 * delete a character from the position ptr in string
 */
static void
del_character(ptr)
char *ptr;
{
   while(*ptr != '\0') {
      *ptr = *(ptr + 1);
      ptr++;
   }
}

/******************************************************/
/*
 * prompt for and return a string
 */
char *
get_edit_str(pstring)
char *pstring;
{
   static char str[60];
   int c,i;

   printf("\r%s",pstring);
   delete_to_eol((char *)NULL);
   (void)fflush(stdout);

   (void)get1char(CTRL_A);
   for(i = 0;i < 60;i++) {
      if((c = get1char(0)) == EOF || (str[i] = c) == '\n' || c == '\r') {
	 break;
      } else if(str[i] == DEL || str[i] == CTRL_H) {
	 if(i == 0) {
	    break;			/* stop trying to get string */
	 } else {
	    i -= 2;
	    (void)printf("\b \b");
	    (void)fflush(stdout);
	 }
      } else {
	 (void)FPUTCHAR(str[i]);
      }
   }
   (void)get1char(EOF);
   str[i] = '\0';
   return(str);
}

/*******************************************************/
/*
 * Erase to the end of line. If we can't do this properly, then overwrite
 * and backspace. If we don't know how many to overwrite, guess.
 */
static void
delete_to_eol(s)
char *s;				/* rest of line */
{
   int i,
       nchar;				/* number of chars to overwrite */

#if defined(__MSDOS__)
   if(term.del_line[0] == '@') {
      clreol();
      return;
   }
#endif
   if(term.del_line[0] != '\0') {
      PRINT(term.del_line);
   } else {
      nchar = (s == NULL) ? 10 : strlen(s);
      for(i = 0;i < nchar;i++) putchar(' ');
      for(i = 0;i < nchar;i++) putchar('\b');
   }
}

/******************************************************/
/*
 * delay for x msec
 */
static void
delay(x)
double x;
{
   char padchar;
   float msec_per_char;
   int npadchars;
   
   if(x > 0 && term.baud > 0) {
      padchar = term.pad & '\177';
      msec_per_char = (8*1000.)/term.baud;
      npadchars = x/msec_per_char + 0.5;
      
      for(;npadchars > 0;npadchars--)
	write(1,&padchar,1);
   }
}

/*******************************************************/
/*
 * fill a string with nulls
 */
void
erase_str(string,size)
char *string;
int size;
{
   register char *send = string + size;

   while(string < send) *string++ = '\0';
}

/**********************************************************/
/*
 * insert a character into a string
 */
static int
insert_char(c,string,pptr,size)
int c;					/* character to be inserted */
char *string,				/* string to be inserted into */
     **pptr;				/* position before which to insert */
int size;				/* size of string */
{
   char cc,save_c;
   char *line = string,			/* UPDATE_CURSOR needs these names */
	*lptr = *pptr;

   if((int)strlen(string) >= size) {
      fprintf(stderr,"\rSorry, there is no room for that character%c",CTRL_G);
      (void)fflush(stderr);
      printf("\r%s %s",prompt,line);
      delete_to_eol((char *)NULL);	/* don't know how much to delete */
      UPDATE_CURSOR;
      return(-1);
   }
   (*pptr)++;

   cc = c & '\177';

   if(*lptr == '\0') {
      *lptr++ = cc;
      *lptr = '\0';
      return(END);
   }

   do {
      save_c = *lptr;
      *lptr++ = cc;
      cc = save_c;
   } while(cc != '\0');
   *lptr = '\0';
   
   return(MIDDLE);
}

/**********************************************/
/*
 * Print first n characters of a string
 */
static void
nprintf(str,n)
char str[];
int n;
{
   int i;
   
   for(i = 0;i < n && str[i] != '\0';i++) {
      putchar(str[i]);
   }
}

/***********************************************************/
/*
 * find_str returns a history node, given a string
 */
EDIT *
find_str(str,repeat,dir,first,last)
char *str;				/* string to find */
int repeat;				/* repeat previous search? */
int dir;				/* -1: down; 1: up */
EDIT *first,*last;			/* range of nodes to search */
{
   EDIT *command;

   (void)match_pattern("",str,(char **)NULL); /* compile pattern */

   command = (dir == 1) ? last : first;
   while(command != NULL) {
      if(command->mark & SEARCH_MARK) {
	 command->mark &= ~SEARCH_MARK;
	 if(repeat) {			/* repeat last search; start here */
	    command = (dir == 1) ? command->prev : command->next;
	    if(command == NULL) return(NULL);
	    break;
	 }
      }
      command = (dir == 1) ? command->prev : command->next;
   }
   if(command == NULL) command = (dir == 1) ? last : first;
      
   while(command != NULL) {
      if(match_pattern(command->line,(char *)NULL,(char **)NULL) != NULL) {
	 if(*command->line == '\0') {	/* not a real match */
	    ;
	 } else {
	    command->mark |= SEARCH_MARK;
	    break;
	 }
      }
      command = (dir == 1) ? command->prev : command->next;
   }

   return(command);
}

/************************************************/
/*
 * Now the code to handle key-bindings.
 *
 * Every ascii character has an entry in array map[] which governs the mapping
 * of key-sequences starting with that letter. This array consists of a struct
 * containing a pointer to a chain of nodes decribing multi-character mapping
 * sequences; in the event that this is NULL the other element of the array
 * gives the action to be associated with the character typed. For example, the
 * letter `a' has a struct { 'a', NULL } meaning that no multi-character
 * sequences start with an a, and that an `a' is to be interpreted as a simple
 * `a'. To map lower to upper case, use { 'a', 'A' }. The action char can be a
 * single char (\0 -- \127) or a char with the \200 bit set. These actions are
 * understood by the main edit loop, and are the final output of the
 * key-mapping process. A structure was used instead of a union as a struct
 * is no larger, when allowance is made for keeping tabs on what is stored.
 *
 * To return to multi-character strings. In this case, the pointer is not NULL,
 * and the action is ignored. The pointer points to a branched chain of nodes,
 * each of which contains four items: the action and pointer as before, and
 * also a pointer to a side-tree and a char. A typed key-sequence is compared
 * char by char as the code
 * traverses the chain, if the char in a node doesn't match the current char
 * in the input key-sequence the next node is inspected; if it does and the
 * action isn't UNBOUND, the action is returned, otherwise another character
 * is obtained and the search continues down the sub-tree.
 *
 * lint is confused by MAKE_NODE, as R->T is declared as (struct map *)
 * not (MAP *)
 */
#define MAKE_NODE(R,T,C)	/* make and initialise a node of type T */    \
   if((R->T = (MAP *)malloc(sizeof(MAP))) == NULL) {		      	      \
      fprintf(stderr,"Can't allocate a node in define_key()\n");	      \
      return(-1);							      \
   }									      \
   R->T->next = R->T->subtree = NULL;					      \
   R->T->action = UNBOUND;						      \
   R->T->c = (C);

typedef struct node {
   short action;		/* action to take */
   struct map *subtree;		/* tree of more complex mappings */
} NODE;

typedef struct map {
   short action;		/* action to take */
   struct map *next,		/* next node in search for match */
   	      *subtree;		/* tree of more complex mappings */
   char c;			/* character to look for at this node */
} MAP;
/*
 * Note that ^@, ^J --> ^M, ^[ --> UNBOUND, ^W --> (\200 | h), DEL --> ^H
 * ^ --> (\200 | ^)
 */
NODE map[128] = {
		  {'\015',NULL},{'\001',NULL},{'\002',NULL},{'\003',NULL},
		  {'\004',NULL},{'\005',NULL},{'\006',NULL},{'\007',NULL},
		  {'\010',NULL},{'\011',NULL},{'\015',NULL},{'\013',NULL},
		  {'\014',NULL},{'\015',NULL},{'\016',NULL},{'\017',NULL},
		  {'\020',NULL},{'\021',NULL},{'\022',NULL},{'\023',NULL},
		  {'\024',NULL},{'\025',NULL},{'\026',NULL},{'h'|'\200',NULL},
		  {'\030',NULL},{'\031',NULL},{'\032',NULL},{UNBOUND,NULL},
		  {'\034',NULL},{'\035',NULL},{'\036',NULL},{'\037',NULL},
		  {'\040',NULL},{'\041',NULL},{'\042',NULL},{'\043',NULL},
		  {'\044',NULL},{'\045',NULL},{'\046',NULL},{'\047',NULL},
		  {'\050',NULL},{'\051',NULL},{'\052',NULL},{'\053',NULL},
		  {'\054',NULL},{'\055',NULL},{'\056',NULL},{'\057',NULL},
		  {'\060',NULL},{'\061',NULL},{'\062',NULL},{'\063',NULL},
		  {'\064',NULL},{'\065',NULL},{'\066',NULL},{'\067',NULL},
		  {'\070',NULL},{'\071',NULL},{'\072',NULL},{'\073',NULL},
		  {'\074',NULL},{'\075',NULL},{'\076',NULL},{'\077',NULL},
		  {'\100',NULL},{'\101',NULL},{'\102',NULL},{'\103',NULL},
		  {'\104',NULL},{'\105',NULL},{'\106',NULL},{'\107',NULL},
		  {'\110',NULL},{'\111',NULL},{'\112',NULL},{'\113',NULL},
		  {'\114',NULL},{'\115',NULL},{'\116',NULL},{'\117',NULL},
		  {'\120',NULL},{'\121',NULL},{'\122',NULL},{'\123',NULL},
		  {'\124',NULL},{'\125',NULL},{'\126',NULL},{'\127',NULL},
		  {'\130',NULL},{'\131',NULL},{'\132',NULL},{'\133',NULL},
		  {'\134',NULL},{'\135',NULL},{'\336',NULL},{'\137',NULL},
		  {'\140',NULL},{'\141',NULL},{'\142',NULL},{'\143',NULL},
		  {'\144',NULL},{'\145',NULL},{'\146',NULL},{'\147',NULL},
		  {'\150',NULL},{'\151',NULL},{'\152',NULL},{'\153',NULL},
		  {'\154',NULL},{'\155',NULL},{'\156',NULL},{'\157',NULL},
		  {'\160',NULL},{'\161',NULL},{'\162',NULL},{'\163',NULL},
		  {'\164',NULL},{'\165',NULL},{'\166',NULL},{'\167',NULL},
		  {'\170',NULL},{'\171',NULL},{'\172',NULL},{'\173',NULL},
		  {'\174',NULL},{'\175',NULL},{'\176',NULL},{'\010',NULL}
	    };
/*
 * Map a key sequence to a code, keep the last key typed in
 * extern int variable last_char
 */
int
map_key()
{
   MAP *n;

   if((last_char = get1char(0)) == EOF) return(EOF);
   last_char &= '\177';

   if(map[last_char].action != UNBOUND) {
      return(map[last_char].action);
   } else {
      if((n = map[last_char].subtree) == NULL) {
	 return(UNBOUND);
      }
      last_char = get1char(0);
      for(;;) {
	 if(last_char == n->c) {
	    if(n->action != UNBOUND) {
	       return(n->action);
	    } else if((n = n->subtree) == NULL) {
	       return(UNBOUND);
	    } else {
	       last_char = get1char(0);
	    }
	 } else if((n = n->next) == NULL) {
	    return(UNBOUND);
	 }
      }
   }
}

/*
 * Define a string to map to an action. The string is processed for
 * escapes, namely: ^n --> control-n (n alpha only)
 *		    \e,\E,^[ --> ESC
 *		    \nnn --> octal nnn
 */
int
define_key(string,action)
char *string;			/* string to be mapped */
int action;			/* to this action */
{
   char *key,			/* ascii value of key */
   	kkey[10],		/* storage for key */
        *sptr;			/* pointer to input string */
   int nkey,			/* number of chars in key sequence */
       okey;			/* octal value of *key */
   MAP *n;
   NODE *root;

   if(*string == '\0') return(0);
/*
 * Expand escape sequences
 */
   for(sptr = string,key = kkey;*sptr != '\0' &&
					      key < kkey + sizeof(kkey) - 1;) {
      if(*sptr == '^' && (*key = *(sptr + 1)) != '\0') { /* ^c */
	 sptr += 2;				/* skip ^n */
	 if(islower(*key)) *key = toupper(*key);
	 *key = *key - '@';
	 if((*key & '\200') == 0) {
	    key++;
	 } else {
	    *(sptr + 2) = '\0';		/* isolate offender */
	    fprintf(stderr,"%s is illegal\n",sptr);
	    break;
	 }
      } else if(!strncmp(sptr,"\\E",2) || !strncmp(sptr,"\\e",2)) {/* escape */
	 sptr += 2;				/* skip \E */
	 *key++ = '\033';
      } else if(str_scanf(sptr,"\\%o",&okey) == 1) {	/* \nnn */
	 sptr += 4;				/* skip \nnn */
	 *key++ = okey & '\177';
      } else {
	 *key++ = *sptr++;
      }
   }
   nkey = key - kkey;
   *key = '\0';				/* terminate */
   key = kkey;				/* and rewind */
/*
 * Now go ahead and do the mapping
 */
   if(*key == '\0' && nkey == 1) {	/* special case */
      map[0].action = action;
      return(0);
   }
   root = &map[(int)(*key++)]; nkey--;
   if(*key == '\0') {
      root->action = action;
      return(0);
   }
   if(root->subtree == NULL) {
      MAKE_NODE(root,subtree,*key);
   }
   n = root->subtree;

   for(;;) {
      if(n->c == *key) {
	 key++; nkey--;
	 if(nkey == 0) {
	    n->action = action;
	    return(0);
	 } else {
	    if(n->subtree == NULL) {
	       MAKE_NODE(n,subtree,*key);
	    }
	    n = n->subtree;
	 }
      } else {
	 if(n->next == NULL) {
	    MAKE_NODE(n,next,*key);
	 }
	 n = n->next;
      }
   }
}

/***************************************************************/
/*
 * Define a mapping from the characters typed by the user to those
 * expected by the history/macro editors.
 * The names of the possible definitions are given in the struct mappings[],
 * and in addition a single character represents itself.
 * A default mapping is defined to do nothing except map ^J to ^M and DEL to ^H
 */
static struct {
   char operation[30];		/* name to use */
   int def_key;			/* the default value of that operation */
} mappings[] = {
   {"start_of_line",CTRL_A},
   {"previous_char",CTRL_B},
   {"delete_char",CTRL_D},
   {"end_of_line",CTRL_E},
   {"next_char",CTRL_F},
   {"illegal",UNBOUND},
   {"delete_previous_char",CTRL_H},
   {"tab",CTRL_I},
   {"kill_to_end",CTRL_K},
   {"refresh",CTRL_L},
   {"carriage_return",CTRL_M},
   {"next_line",CTRL_N},
   {"insert_line_above",CTRL_O},
   {"previous_line",CTRL_P},
   {"quote_next",CTRL_Q},
   {"search_reverse",CTRL_R},
   {"search_forward",CTRL_S},
   {"toggle_overwrite",CTRL_T},
   {"delete_to_start",CTRL_U},
   {"scroll_forward",CTRL_V},
   {"exit_editor",CTRL_X},
   {"yank_buffer",CTRL_Y},
   {"attach_to_shell",CTRL_Z},
   {"escape",ESC},
   {"history_char",'^' | '\200'},
   {"previous_word",'b' | '\200'},
   {"delete_next_word",'d' | '\200'},
   {"delete_from_history",CTRL_D | '\200'},
   {"next_word",'f' | '\200'},
   {"goto_line",'g' | '\200'},
   {"delete_previous_word",'h' | '\200'},
   {"undelete_word",'u' | '\200'},
   {"yank_previous_buffer",'y' | '\200'},
   {"scroll_back",'v' | '\200'},
   {"first_line",'<' | '\200'},
   {"last_line",'>' | '\200'}
};
#define NMAP (sizeof(mappings)/sizeof(mappings[0]))
/*
 * Set up the map[] array from a file.
 * The format is: two lines of header, followed by pairs of op_name key_name,
 * where op_name must be defined in mappings[], and key_name is the alias
 * that the user wishes to use. Control characters (^A to ^Z) may be entered as
 * ^n, octal sequences (\nnn) are also allowed.
 */
int
read_map(file)
char file[];			/* name of file */
{
   char key_name[10],		/* name of user-defined key */
   	line[81],		/* read a line */
	op_name[30];		/* for this operation */
   FILE *fil;			/* input file */
   int i;
   
   if((fil = fopen(file,"r")) == NULL) {
      fprintf(stderr,"Can't open %s\n",file);
      return(-1);
   }
   (void)fgets(line,81,fil);		/* skip header */
   (void)fgets(line,81,fil);

   while(fgets(line,81,fil) != NULL) {
      if(line[0] == cchar) {		/* a comment */
	 continue;
      }
      for(i = 0;isspace(line[i]);i++) continue;
      if(line[i] == '\0') continue;
      
      if(str_scanf(line,"%s %s",op_name,key_name) != 2) {
	 msg_2s("error READ EDITing line \"%s\" from file %s\n",line,file);
	 continue;
      }
      if(str_scanf(op_name,"\\%o",&i) == 1) { /* \nnn */
	 op_name[0] = i; op_name[1] = '\0';
      }
      if(op_name[1] == '\0') {				/* single char */
	 (void)define_key(key_name,op_name[0]);
	 continue;
      }
      for(i = 0;i < NMAP;i++) {
	 if(!strcmp(op_name,mappings[i].operation)) {	/* found it */
	    (void)define_key(key_name,mappings[i].def_key);
	    break;
	 }
      }
      if(i == NMAP) {
         msg_1s("Unknown operator %s\n",op_name);
      }
   }
   (void)fclose(fil);
   return(0);
}

/********************************************/
/*
 * Specify a mapping from the keyboard
 */
void
define_map(op_name,key_name)
char *op_name,			/* operator desired */
     *key_name;			/* mapped to this string */
{
   int i;

   if(op_name[1] == '\0') {				/* single char */
      (void)define_key(key_name,op_name[0]);
      return;
   }
   for(i = 0;i < NMAP;i++) {
      if(!strcmp(op_name,mappings[i].operation)) {	/* found it */
	 (void)define_key(key_name,mappings[i].def_key);
	 return;
      }
   }
   msg_1s("Unknown operator %s\n",op_name);
}

/*************************************************************/
/*
 * List all current bindings
 */
#define ADD_KEY(C)		/* add a key to kseq[] in printable form */\
  if((C) < 27) { (void)sprintf(kptr,"^%c",(C)+'A'-1); kptr += 2; }	\
  else if((C) == 27) { (void)sprintf(kptr,"^["); kptr += 2; }		\
  else if(isprint(C)) { (void)sprintf(kptr,"%c",(C)); kptr++; }		\
  else { (void)sprintf(kptr,"\\%03o",(C)); kptr += 4; }

static char buff[81],			/* buffer for more */
	    kseq[17];			/* key-sequence being listed */

void
list_edit()
{
   char *kptr,				/* pointer to kseq */
   	*mptr;				/* pointer to keyboard macro */
   int i,j;
   NODE *root;

   (void)more((char *)NULL);		/* initialise more() */
   for(i = 0;i < sizeof(map)/sizeof(map[0]);i++) {
      kptr = kseq;
      ADD_KEY(i);
      root = &map[i];
      if(root->action == i) {
	 if(sm_verbose == 0) {
	    continue;
	 } else if(isprint(i)) {
	    if(sm_verbose >= 2) {
	       (void)sprintf(buff,"%s\t%c\n",kseq,i);
	       if(more(buff) < 0) break;
	    }
	    continue;
	 }
      }
      if(root->action == UNBOUND) {
	 if(root->subtree == NULL) {
	    (void)sprintf(buff,"%s\tUNBOUND\n",kseq);
	    if(more(buff) < 0) break;
	 } else {
	    if(list_edit_tree(root->subtree,kptr) < 0) break;
	 }
      } else {
	 if((map[i].action & '\200') && isdigit(map[i].action & '\177')) {
	    if((mptr = get_key_macro((map[i].action & '\177') - '0')) == NULL){
	       (void)sprintf(buff,"%s\tUNKNOWN\n",kseq);
	    } else {
	       j = strlen(mptr);
	       if(mptr[j - 1] == CTRL_M) {
		  (void)sprintf(buff,"%s\t%.*s\\N\n",kseq,j - 1,mptr);
	       } else {
		  (void)sprintf(buff,"%s\t%s\n",kseq,mptr);
	       }
	    }
	 } else {
	    for(j = 0;j < NMAP;j++) {
	       if(mappings[j].def_key == map[i].action) break;
	    }
	    (void)sprintf(buff,"%s\t%s\n",kseq,
				    (j==NMAP)?"UNKNOWN":mappings[j].operation);
	 }
	 if(more(buff) < 0) break;
      }
   }
}

static int
list_edit_tree(node,kptr)
MAP *node;				/* next node to consider */
char *kptr;
{
   char *init_kptr,			/* kptr on entry into function */
   	*mptr;				/* pointer to keyboard macro */
   int j;

   init_kptr = kptr;
   ADD_KEY(node->c);

   if(node->action != UNBOUND) {	/* this is a complete binding */
      if((node->action & '\200') && isdigit(node->action & '\177')) {
	 if((mptr = get_key_macro((node->action & '\177') - '0')) == NULL) {
	    (void)sprintf(buff,"%s\tUNKNOWN\n",kseq);
	 } else {
	    j = strlen(mptr);
	    if(mptr[j - 1] == CTRL_M) {
	       (void)sprintf(buff,"%s\t%.*s\\N\n",kseq,j - 1,mptr);
	    } else {
	       (void)sprintf(buff,"%s\t%s\n",kseq,mptr);
	    }
	 }
      } else {
	 for(j = 0;j < NMAP;j++) {
	    if(mappings[j].def_key == node->action) break;
	 }
	 (void)sprintf(buff,"%s\t%s\n",kseq,
		 	(j==NMAP)?"UNKNOWN":mappings[j].operation);
      }
      if(more(buff) < 0) return(-1);
   }

   if(node->subtree != NULL) {
      if(list_edit_tree(node->subtree,kptr) < 0) {
	 return(-1);
      }
   }

   if(node->next != NULL) {
      if(list_edit_tree(node->next,init_kptr) < 0) {
	 return(-1);
      }
   }

   return(0);
}
