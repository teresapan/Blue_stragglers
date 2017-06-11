#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "tty.h"
#include "sm_declare.h"

static int readline(),
	   tty_extract_alias();
static void tty_scan_env();
/*
 * TTYOPEN -- Scan the named TERMCAP style file for the entry for the named
 * device, and if found allocate a TTY descriptor structure, leaving the
 * termcap entry for the device in the descriptor.
 */
TTY *
ttyopen(termcap_file,ac,av,ttyload)
char termcap_file[];		/* termcap file to be scanned */
int ac;				/* number of arguments to DEVICE */
char *av[];			/* pointers to arguments (av[0] is device) */
int (*ttyload)();		/* fetches pre-compiled entries from a cache */
{
   char ldevice[100];			/* local storage for "av[0]" */
   int i,
       len;				/* length of av[i] */
   TTY *tty;
/*
 * Allocate and initialize the tty descriptor structure
 */
     if((tty = (TTY *)malloc(sizeof(TTY))) == NULL) {
	msg("Can't malloc tty in ttyopen\n");
	return(NULL);
     }
     tty->t_op = 0;			/* index in tty->t_caplist */
     tty->t_caplen = 0;
     *tty->t_tcdevice = '\0';

     strcpy(ldevice,av[0]);		/* gets overwritten in tc expansion */
     if(ttyload != NULL) {
	tty->t_caplen = (*ttyload)(termcap_file,ldevice,tty);
     }

     if(tty->t_caplen == 0) {		/* didn't find device in cache */
	 if((tty->t_caplist = malloc(T_MEMINC)) == NULL) {
	    msg("Can't malloc tty->t_caplist in ttyopen\n");
	    free((char *)tty);
	    tty = NULL;
	    return(NULL);
	 } else {
	    tty->t_len = T_MEMINC;
	 }

	 for(i = 1;i < ac;i++) {	/* look for graphcap entries in av[] */
	    if(av[i][0] == ':') {
/*
 * tc and TC entries are special as they have to be overwritten. Store them
 * in the tty structure directly
 */
	       if(strncmp(av[i],":tc=",4) == 0 ||
		  				strncmp(av[i],":TC=",4) == 0) {
		  strcpy(tty->t_tcdevice,av[i] + 4);
		  len = strlen(tty->t_tcdevice);
		  if(tty->t_tcdevice[len - 1] == ':') {
		     tty->t_tcdevice[len - 1] = '\0';
		  }
		  tty->t_istc = (tty->t_tcdevice[1] == 't') ? 1 : 0;
		  continue;
	       }
	       
	       len = strlen(av[i]);
	       if(len + tty->t_op > tty->t_len) {
		  tty->t_len += T_MEMINC;
		  if((tty->t_caplist = realloc(tty->t_caplist,
					      (unsigned)tty->t_len)) == NULL) {
		     msg("Can't reallocate tty->t_caplist\n");
		     free((char *)tty);
		     return(NULL);
		  }
	       }
	       (void)strcpy(&(tty->t_caplist[tty->t_op]),
			    &av[i][i == 1 ? 0 : 1]); /* don't need 2 :'s */
	       tty->t_op += len - (i == 1 ? 0 : 1);
	       tty->t_caplen = tty->t_op + 1; /* include '\0' */
	    }
	 }
	 if(tty_scan_termcap_file(tty, termcap_file,ldevice) < 0) {
	     free(tty->t_caplist);
	     free((char *)tty);
	     return(NULL);
	 }
/* 
* Call realloc to return any unused space in the descriptor
*/
	 if((tty->t_caplist = realloc(tty->t_caplist,(unsigned)tty->t_caplen))
								    == NULL) {
	    msg("Can't realloc tty->t_caplist\n");
	    free((char *)tty);
	    tty = NULL;
	    return(NULL);
	 }
      }
      tty->t_len = tty->t_caplen;

     return (tty);
}

/**********************************************************/
/*
 * TTY_SCAN_TERMCAP_FILE -- Open and scan the named TERMCAP format database
 * file for the named device.  Fetch termcap entry, expanding any and all
 * "tc" references by repeatedly rescanning file.
 */
#define NFILES 4			/* max. number of files in path */

int
tty_scan_termcap_file(tty, termcap_file, device)
TTY *tty;				/* tty descriptor structure */
char termcap_file[];			/* termcap format file to be scanned */
char device[];				/* termcap entry to be scanned for */
{
   char *ip, *op, *caplist,
   	*files[NFILES],			/* list of termcap files */
   	*tptr;
   FILE *fil;
   int i,
       is_tc,				/* continuation is tc= not TC= */
       old_t_op,			/* previous value of tty->t_op */
       nfiles,				/* number of termcap files */
       ntc;

   if((files[0] = malloc(strlen(termcap_file) + 1)) == NULL) {
      msg("Can't allocate storage for termcap file list\n");
      return(-1);
   }
   tptr = strcpy(files[0],termcap_file);

   nfiles = 0;
   if((ip = strchr(tptr,'\177')) != NULL) { /* the first part of the list is
					       $TERMCAP from the environment */
      *ip = '\0';
      if((fil = fopen(files[0],"r")) != NULL) {
	 fclose(fil);
	 *ip = ' ';
      } else {
	 tty_scan_env(tty,files[0],device);
	 if(*device == '\0') return(0);	/* we've got it */
	 nfiles++;
	 tptr = ip + 1;
      }
   }
   
   for(i = nfiles;i < NFILES;i++) {
      files[i] = tptr;
      while(*tptr != '\0') {
	 if(isspace(*tptr)) {
	    break;
	 }
	 tptr++;
      }
      if(*tptr != '\0') *tptr++ = '\0';	/* don't ++ over the closing '\0' */
      while(isspace(*tptr)) tptr++;
      if(*tptr == '\0') {
	 break;
      }
   }
   nfiles = (i == NFILES) ? NFILES : i + 1;

   ntc = 0;
   old_t_op = tty->t_op;
   for(;;) {
      for(i = 0;i < nfiles;i++) {
	 if((fil = fopen(files[i],"r")) == NULL ||
	 			tty_fetch_entry(fil,device,tty) == -1) {
	    if(fil != NULL) fclose(fil);
	    if(i == nfiles - 1) {
	       if(tty->t_op == old_t_op) {
		  msg_2s("Can't find entry for %s in%s",device,
					      nfiles == 1 ? "" : " any of");
		  for(i = 0;i < nfiles;i++) {
		     msg_1s(" %s",files[i]);
		  }
		  msg("\n");
	       }
	       free(files[0]);
	       return(-1);
	    }
	 } else {
	    fclose(fil);		/* we found something */
/*
 * Back up to start of last field in entry
 */
	    caplist = tty->t_caplist;
	    ip = &caplist[tty->t_op - 2];
	    while (*ip != ':') ip--;

	    if(*tty->t_tcdevice == '\0') {
	       *device = '\0';
	       is_tc = 0;		/* make gcc happy; value isn't used */
	    } else {
	       strcpy(device,tty->t_tcdevice);
	       is_tc = tty->t_istc;
	       *tty->t_tcdevice = '\0';
	       old_t_op = tty->t_op;
	    }
/*
 * If last field is "tc", backup up so that the tc field gets
 * overwritten with the referenced entry
 */
	    if(strncmp(ip + 1,"tc",2) == 0 || strncmp(ip + 1,"TC",2) == 0) {
	       is_tc = strncmp(ip + 1,"tc",2) == 0 ? 1 : 0; /* not TC */

	       if(++ntc > MAX_TC_NESTING) {	/* Check for recursive tc */
		  msg_1s("Too deep nesting of \"tc\" for %s\n",device);
		  continue;
	       }
	       
	       /* Set op to point to the ":" in ":tc=file". */
	       old_t_op = tty->t_op = ip - caplist;
/*
 * If device isn't already specified, get device name from tc field.
 */
	       if(*device == '\0') {
		  ip += strlen(":tc=");
		  for(op = device;*ip != '\0' && *ip != ':';) {
		     *op++ = *ip++;
		  }
		  *op = '\0';
	       }
	    }
/*
 * Time to actually process the tc/TC field if there is one
 */
	    if(*device != '\0') {
	       if(is_tc) {
		  break;
	       } else {
		  continue;
	       }
	    }
	    free(files[0]);
	    return(0);
	 }
      }
   }
}

/******************************************************/
/*
 * TTY_FETCH_ENTRY -- Search the termcap file for the named entry, then read
 * the colon delimited capabilities list into the caplist field of the tty
 * descriptor.  If the caplist field fills up, allocate more space.
 */
#define LBUF_SIZE 201
#define SZ_FNAME 40

int
tty_fetch_entry (fil,device,tty)
FILE *fil;
char device[];
TTY *tty;
{
   char	lastch,
	*ip, *op, *otop, lbuf[LBUF_SIZE],
	alias[SZ_FNAME];
   int	ch,
   	device_found,
        maybe_macro;			/* is it a graphcap macro? */
/*
 * Locate entry.  First line of each termcap entry contains a list
 * of aliases for the device.  Only first lines and comment lines
 * are left justified.
 */
   do {
      /* Skip comment and continuation lines and blank lines. */
      device_found = 0;
      
      if ((ch = getc(fil)) == EOF)
	return(-1);
      
      if (ch == '\n') {
	 /* Skip a blank line. */
	 continue;
      } else if (ch == '#' || isspace(ch)) {
	 /* Discard the rest of the line and continue. */
	 if(readline(lbuf,LBUF_SIZE,fil) == EOF)
	   return(-1);
	 continue;
      }
/*
 * Extract list of aliases or a graphcap macro.
 * If the line looks like "^[_a-zA-Z]*=" it's a macro, otherwise it should
 * be an entry in which case the first occurrence of ':' marks
 * the end of the alias list and the beginning of the caplist.
 */
      lbuf[0] = ch;
      op = lbuf + 1;
      maybe_macro = 1;			/* still agnostic on macrodom */
      
      while(op < lbuf + LBUF_SIZE - 1) {
	 switch (ch = getc(fil)) {
	  case EOF:
	    return(-1);
	  case '|':
	    maybe_macro = 0;		/* it isn't */
	    break;
	  case '=':
	    if(maybe_macro) {
	       maybe_macro = 2;		/* it is */
	       op--;
	       while(isspace(*op)) op--;
	       *++op = '\0';
	       strncpy(alias,lbuf,SZ_FNAME);
	       for(op = lbuf;op < lbuf + LBUF_SIZE - 1;op++) {
		  if((*op = getc(fil)) == '\0' || *op == '\n') {
		     break;
		  }
	       }
	       op--;
	       while(isspace(*op)) op--;
	       *++op = '\0';

	       for(op = lbuf;isspace(*op);op++) continue;
	       def_sy_macro(alias,op);
	       break;
	    }
	    break;
	 }
	 if(maybe_macro == 2) break;
	 if((*op++ = ch) == ':') {
	    maybe_macro = 0;
	    break;
	 }
      }
      if(maybe_macro > 0) continue;	/* it's a macro so it isn't an alias */
      *op = '\0';
      
      ip = lbuf;
      while (tty_extract_alias (ip, alias, SZ_FNAME) > 0) {
	 if (device[0] == '\0' || !strcmp(alias, device)) {
	    device_found = 1;
	    break;
	 }
	 ip += strlen(alias);
	 if(*ip == '|' || *ip == ':')
	   ip++;				/* skip delimiter */
      }
      
      /* Skip rest of line if no match. */
      if (!device_found) {
	 if(readline(lbuf,LBUF_SIZE,fil) == EOF) {
	    return(-1);
	 }
      }
   } while (!device_found);
/*
 * Caplist begins at first ':'.  Each line has some whitespace at the
 * beginning which should be skipped.  Escaped newline implies
 * continuation.
 */
   op = &tty->t_caplist[tty->t_op];
   otop = &tty->t_caplist[tty->t_len];
   
   /* We are already positioned to the start of the caplist. */
   *op++ = ':';
   lastch = ':';
   
   /* Extract newline terminated caplist string. */
   while ((ch = getc(fil)) != EOF) {
      if (ch == '\\') {				/* escaped newline? */
	 if((ch = getc(fil)) == '\n') {
	    if((ch = getc(fil)) == '#') {
	       if(readline(lbuf,LBUF_SIZE,fil) == EOF) {
		  return(-1);
	       }
	    } else {
	       ungetc(ch,fil);
	    }
	    while((ch = getc(fil)) != EOF && isspace(ch)) continue;
	    
	    if(ch == EOF || ch == '\n')
	      return(-1);
	    if(ch == ':' && lastch == ':')/* Avoid null entries "::"*/
	      continue;
	    else
	      *op = ch;
	 } else {			/* no, keep both chars */
	    *op++ = '\\';
	    *op = ch;
	 }
      } else if (ch == '\n') {			/* normal exit */
	 *op = '\0';
	 tty->t_op = op - tty->t_caplist;
	 tty->t_caplen = tty->t_op + 1;		/* include '\0' */
	 return(0);
      } else
	*op = ch;
      
/*
 * Increase size of buffer if necessary.  Note that realloc may
 * move the buffer, so we must recalculate op and otop
 */
      lastch = ch;
      if (++op >= otop - 1) {	/* can have to op += 2 */
	 tty->t_op = op - tty->t_caplist;
	 tty->t_len += T_MEMINC;
	 if((tty->t_caplist = realloc(tty->t_caplist,(unsigned)tty->t_len))
								    == NULL) {
	    msg("Can't reallocate tty->t_caplist\n");
	    free((char *)tty);
	    tty = NULL;
	    return(-1);
	 }
	 op = tty->t_caplist + tty->t_op;
	 otop = tty->t_caplist + tty->t_len;
      }
   }
   return(-1);
}

/**********************************************************/
/*
 * TTY_EXTRACT_ALIAS -- Extract a device alias string from the header of
 * a termcap entry.  The alias string is terminated by '|' or ':'.  Leave
 * ip pointing at the delimiter.  Return number of chars in alias string.
 */
static int
tty_extract_alias (ip, outstr, maxch)
char *ip;		/* on input, first char of alias */
char	outstr[];
int	maxch;
{
   char *op;
   
   op = outstr;
   while(*ip != '|' && *ip != ':' && *ip != '\0') {
      *op++ = *ip++;
      if(op >= outstr + maxch) {	/* no room in outstr, */
	 op--;				/* but read to end of alias anyway */
      }
   }
   *op = '\0';
   
   return(op - outstr);
}
/*
 * Read a line from fil into buff, return number of chars read
 */
static int
readline(buff,size,fil)
char buff[];
int size;
FILE *fil;
{
   buff[size - 1] = '\0';			/* make sure it's terminated */
   if(fgets(buff,size,fil) == NULL) {
      return(EOF);
   }

   if(buff[size - 1] != '\0') {
      msg_1d("Truncating line in capfile to %d chars in readline\n",size - 1);
      buff[size - 1] = '\0';
      return(size - 1);
   } else {
      return(strlen(buff));
   }
}

static void
tty_scan_env(tty, termcap, device)
TTY *tty;				/* tty descriptor structure */
char *termcap;				/* termcap string to be scanned */
char *device;
{
   char	alias[SZ_FNAME],
   	*ptr;
   int device_found,
       len;

   if(termcap[0] == ':') {		/* no list of names, just caps */
      ;
   } else {
      device_found = 0;
      while((len = tty_extract_alias(termcap,alias,SZ_FNAME)) > 0) {
	 if(device[0] == '\0' || !strcmp(alias, device)) {
	    device_found = 1;
	    break;
	 }
	 termcap += len;
	 if(*termcap == '|') termcap++;		/* skip delimiter */
      }
      
      if(!device_found) {
	 return;
      }
   }
/*
 * OK, we have the termcap entry. Look to see if there's a :tc=: continuation
 */
   if(*(ptr = termcap + strlen(termcap)) == ':') *--ptr = '\0';
   while(ptr > termcap && *ptr != ':') ptr--;
   if(strncmp(ptr,":tc=",4) == 0 || strncmp(ptr,":TC=",4) == 0) {
      *ptr = '\0';
      ptr += strlen(":tc=");
      strcpy(device,ptr);
   } else {
      device[0] = '\0';			/* no more entries to find */
   }
   tty->t_op = tty->t_caplen = strlen(termcap);
   if(tty->t_caplen >= tty->t_len) {
      tty->t_len = tty->t_caplen + 1;
      if((tty->t_caplist = realloc(tty->t_caplist, tty->t_len)) == NULL) {
	 msg("Can't reallocate tty->t_caplist\n");
	 free((char *)tty);
	 tty = NULL;
	 return;
      }
   }
   (void)strcpy(tty->t_caplist,termcap);
}
	   
/************************************************************************/
/*
 * TTYGDES -- Open a TTY graphics stdgraph descriptor. The name of the
 * graphcap file is fetched from the environment and the graphcap file
 * is searched for an entry corresponding to the named device.
 *
 * The descriptor is then allocated, and the graphcap entry read in. Graphcap
 * permits an entry to be defined in terms of another entry with the "tc"
 * field; we must expand such references by rescanning the file once for each
 * such reference. Finally, the graphcap entry is indexed for efficient access.
 * The form of a graphcap entry is identical to that for a termcap entry,
 * except that tc or TC is accepted for a continuation (TC doesn't rescan
 * the search path)
 */
TTY *
ttygdes(ac,av)
int ac;					/* number of arguments to DEVICE */
char *av[];				/* pointers to arguments */
{
   char *baud_rate,			/* baud rate from environment */
	*cap_file;			/* name of graphcap file */
   TTY *tty;
/*
 * av[0] is the name of the desired graphcap entry in the graphcap file.
 */
   if(*(cap_file = print_var("graphcap")) == '\0') {
      msg("File graphcap is not defined\n");
      return(NULL);
   }
/*
 * Allocate and initialize the tty descriptor structure.  Fetch graphcap
 * entry from graphcap file into descriptor.  The G_TTYLOAD procedure,
 * passed as an argument to TTYOPEN, is searched for cached termcap
 * entries before accessing the actual file.
 */
   if((tty = ttyopen(cap_file,ac,av,g_ttyload)) == NULL) {
      return(NULL);
   }
/*
 * Prepare index of fields in the descriptor, so that we can more
 * efficiently search for fields later.
 */
   tty_index_caps(tty,tty->t_capcode, tty->t_capindex);
   if(tty->t_ncaps == MAX_CAPS) {
       msg_2s("while searching for \"%s\" in \"%s\"\n", av[0], cap_file);
   }
/*
 * Get pad char and baud rate (used by ttyputs to generate delays)
 * and put in descriptor for efficient access.  Also get tty screen
 * dimensions since they are fundamental and will probably prove
 * useful later.
 */
   tty->t_padchar = ttygeti(tty, "pc");	/* returns 0 if field not found */
   tty->t_nlines  = ttygeti (tty, "li");
   tty->t_ncols   = ttygeti (tty, "co");
/*
 * Baud rate may not be right if device is attached to the host, and
 * and user is working remotely.  Nonetheless it is safer to pick up
 * the value from the environment than to assume a default.
 */
   if(*(baud_rate = print_var("ttybaud")) != '\0') {
      tty->t_baud = atoi(baud_rate);
   } else {
      tty->t_baud = DEF_BAUDRATE;
   }

   return(tty);
}

/***************************************************************/
/*
 * G_TTYLOAD -- Version of the TTYLOAD procedure for accessing compiled entries
 * from the GRAPHCAP database file.  Search the database of compiled GRAPHCAP
 * entries for the named device, and if found, return the CAPLIST string (list
 * of device capabilities) in the output string.  The number of characters output
 * is returned as the function value.  The compiled database is defined by the
 * include file "cacheg.dat", which serves as a cache for the GRAPHCAP
 * entries of heavily used devices (see compile_g.c)
 */
int
g_ttyload (cap_file, device, tty)
char	cap_file[];		/* name of termcap file being referenced */
char	device[];		/* device name as in TERMCAP entry */
TTY *tty;
{
   int dev,
       i;
#  include "cacheg.dat"
/*
 * termcap_filename, devname, offsets, and buf are defined and initialized
 * in the include file. If you don't have a cacheg.dat file, use compile_g
 * to make one. Even without this file g_compile can make a valid (although
 * null) cacheg.dat file which you can use to bootstrap a real one. See 
 * compile_g.c for details.
 *
 * If the name of the file being referenced is not the same as the
 * name of the file used to build the cache, then the cache is
 * invalidated. If a list of names is given assume that the cache
 * refers to the first. This may not be very useful.
 */
   for(i = 0;cap_file[i] != '\0' && !isspace(cap_file[i]);i++) continue;
   if(strncmp(cap_file, termcap_filename,i))
     return (0);

   for(dev = 0;devices[dev][0] != '\0';dev++) {
      if(!strcmp(devices[dev],device)) {
	 tty->t_caplen = offsets[dev + 1] - offsets[dev];
	 if((tty->t_caplist = malloc(tty->t_caplen + 1)) == NULL) {
	    fprintf(stderr,"Can't allocate storage for tty->t_caplist\n");
	    tty->t_caplen = 0;
	    return(0);
	 }
      	 memcpy(tty->t_caplist,&buf[offsets[dev]],tty->t_caplen);
	 tty->t_caplist[tty->t_caplen] = '\0';
	 return(tty->t_caplen);
      }
   }
   return (0);
}

