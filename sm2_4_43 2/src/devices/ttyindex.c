#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "tty.h"
#include "sm_declare.h"

#if 0
.nf _________________________________________________________________________
TTY_INDEX_CAPS -- Prepare an index into the caplist string, stored in
the tty descriptor.  Each two character capability name maps into a unique
integer code, called the capcode.  We prepare a list of capcodes, keeping
only the first such code encountered in the case of multiple entries.
The offset of the capability in the caplist string is associated with each
capcode.  When these lists have been prepared, they are sorted to permit
a binary search for capabilities at runtime.
#endif

void
tty_index_caps (tty, t_capcode, t_capindex)
TTY *tty;
int t_capcode[], t_capindex[];
{
   char *ip, *caplist;
   int i, swap, capcode, temp,
       ncaps;

   ip = caplist = tty->t_caplist;
/*
 * Scan the caplist and prepare the capcode and capindex lists.
 */
   for(ncaps=0;ncaps < MAX_CAPS;) {
/*
 * Advance to the next capability field.  Normal exit occurs when ':' is
 * followed immediately by EOS.
 */
	while (*ip != ':' && *ip != '\0') ip++;
	if(*(ip+1) == '\0' || *ip == '\0') {
		break;
	}
	ip++;			/* skip : */
	capcode = ENCODE(ip);
/*
 * Is the capcode already in the list?  If not found, add it to the list.
 */
	for(i=0;i < ncaps && tty->t_capcode[i] != capcode;i++) continue;
	if(i >= ncaps) {				/* not found */
	   t_capcode[ncaps] = capcode;
	   t_capindex[ncaps++] = ip - caplist;
	}
    }

    if (ncaps >= MAX_CAPS) {
       msg("Too many capabilities in {graph,term,file,...}cap entry\n");
    }
    tty->t_ncaps = ncaps;
/*
 * A simple interchange sort is sufficient here, even though it would
 * not be hard to interface to qsort.  The longest termcap entries are
 * about 50 caps, and the time req'd to sort such a short list is
 * negligible compared to the time spent searching the termcap file.
 */
   if (ncaps > 1) {
      do {
	 swap = 0;
	 for(i = 0;i < ncaps-1;i++) {
	    if(t_capcode[i] > t_capcode[i+1]) {
	       temp = t_capcode[i];
	       t_capcode[i] = t_capcode[i+1];
	       t_capcode[i+1] = temp;
	       temp = t_capindex[i];
	       t_capindex[i] = t_capindex[i+1];
	       t_capindex[i+1] = temp;
	       swap = 1;
	    }
	}
      } while (swap);
   }
}


/* TTY_FIND_CAPABILITY -- Search the caplist for the named capability. */
/* If found, return a pointer to the first char of the value field, */
/* otherwise return NULL. If the first char in the capability string */
/* is '@', the capability "is not present". */

char *
tty_find_capability(tty, cap)
TTY *tty;			/* tty descriptor */
char cap[];			/* two character name of capability */
{
   char *ptr;
   int	capcode, capnum;
   
   capcode = ENCODE(cap);
   capnum = tty_binsearch (capcode, tty->t_capcode,tty->t_ncaps);
   
   if (capnum >= 0) {
      /* Add 2 to skip the two capname chars. */
      ptr = &tty->t_caplist[tty->t_capindex[capnum] + 2];
      if(*ptr == '@') return (NULL);
      else return(ptr);
   }
   return (NULL);
}


/* TTY_BINSEARCH -- Perform a binary search of the capcode array for the */
/* indicated capability.  Return the array index of the capability if found, */
/* else zero. */

int
tty_binsearch (capcode, t_capcode, ncaps)
int capcode,
    t_capcode[],
    ncaps;
{
   int	low, high, pos, ntrips;

   if(ncaps == 0) {
      return(-1);
   }

	low = 0;
	high = ncaps - 1;
	/* Cut range of search in half until code is found, or until range */
	/* vanishes (high - low <= 1).  If neither high or low is the one, */
	/* code is not found in the list. */

	pos = 0;
	for(ntrips = 0;ntrips < ncaps;ntrips++){
	    pos = (high - low) / 2 + low;
	    if (t_capcode[low] == capcode)
		return (low);
	    else if (t_capcode[high] == capcode)
		return (high);
	    else if (pos == low)			/* (high-low)/2 == 0 */
		return (-1);				/* not found */
	    else if (t_capcode[pos] < capcode)
		low = pos;
	    else
		high = pos;
	}

/* Search cannot fail to converge unless there is a bug in somewhere. */

   msg("Search failed to converge in tty_binsearch\n");
   return(-1);
}
