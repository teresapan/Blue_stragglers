#include "copyright.h"
/*
 * Define a macro to be bound to some key or other
 */
#include <stdio.h>
#include "options.h"
#include "charset.h"
#include "edit.h"
#include "sm_declare.h"

#define KEY_SIZE 20			/* max length of a key sequence */
#define NUM_KEY 10			/* number of keyboard macros */
#define NUM_PF 4			/* number of PF keys */

typedef struct {
   char key[KEY_SIZE],			/* key sequence */
   	*value;				/* pointer to associated string */
} KEY_MAC;

extern int no_editor;			/* should we use the history editor? */
extern int sm_verbose;
static KEY_MAC keys[NUM_KEY] = {
		{"", NULL}, {"", NULL}, {"", NULL}, {"", NULL}, {"", NULL},
		{"", NULL}, {"", NULL}, {"", NULL}, {"", NULL}, {"", NULL}
	     		     };		/* Which keys are in use? */

void
key_macro()
{
   char command[80],			/* command to execute */
   	key[KEY_SIZE];			/* Key to bind to */
   static char c[2] = " ";		/* single char action to use */
   int i;
   unsigned int len;			/* length of command */
   
   prompt_user("Enter key and command");
   {
      int no_editor_s = no_editor;
      no_editor = 1;
      (void)mscanstr(key,KEY_SIZE);
      (void)mgets(command,80);
      no_editor = no_editor_s;
   }
   len = strlen(command);		/* allow for the '\0' */
   if(len == 0) {
      command[1] = '\0';		/* be safe! */
   } else {
      if(command[len - 2] == '\\' && command[len - 1] == 'N') {
	 command[len - 2] = CTRL_M;
	 command[len - 1] = '\0';
	 len--;
      }
   }

   if((strncmp(key,"pf",2) == 0 || strncmp(key,"PF",2) == 0) &&
		key[3] == '\0' &&
		      ((i = key[2] - '0') > 0 && i <= NUM_PF)) { /* a PF key */
      c[0] = '\200' | ('0' + i);	/* keys[i].key already correct */
      define_map(c,keys[i].key);
   } else {
      for(i = 0;i < NUM_KEY;i++) {		/* Is key to be re-bound? */
	 if(!strcmp(key,keys[i].key)) {
	    break;
	 }
      }
      if(i == NUM_KEY) {
	 for(i = 0;i < NUM_KEY;i++) {		/* look for blank slot */
	    if(i >= 1 && i <= NUM_PF) continue;	/* reserved for PF keys */
	    if(keys[i].value == NULL) break;
	 }
	 if(i == NUM_KEY) {
	    if(sm_verbose > 1) {
	       msg_1d("You can only define %d keys",NUM_KEY);
	       msg_1d(" (%d of which are PF)\n",NUM_PF);
	    }
	    return;
	 }
      }
      (void)strcpy(keys[i].key,key);
      
      c[0] = '\200' | ('0' + i);
      define_map(c,key);
   }

   if(keys[i].value != NULL) {
      free(keys[i].value);
   }
   if((keys[i].value = malloc(len + 1)) == NULL) {
      fprintf(stderr,"Can't get space for definition of %s\n",key);
      return;
   }
   (void)strcpy(keys[i].value,&command[1]); /* [1] => room for the '\0' */
}


char *
get_key_macro(i)
int i;					/* number of desired macro */
{
   if(i < 0 || i > NUM_KEY) {
      msg_1d("Illegal key macro number %d\n",i);
      return(NULL);
   }
   return(keys[i].value);
}


/*
 * Set keys[i] to be current PFi key
 */
void
set_pf_key(seq,n)
char *seq;				/* sequence for key */
int n;					/* which key? */
{
   static char c[2] = " ";

   if(n >= 1 && n <= NUM_PF) {
      c[0] = '\200' | ('0' + n);
      define_map(c,seq);
      strcpy(keys[n].key,seq);
   } else {
      msg_1d("attempt to define PF%d\n",n);
   }
}
