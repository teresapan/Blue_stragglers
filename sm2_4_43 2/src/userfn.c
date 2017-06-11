#include "copyright.h"
#include "options.h"
#include <stdio.h>
#include <math.h>
#include "vectors.h"
#include "sm_declare.h"
/*
 * This function allows the user to add his/her/its favourite feature
 * without re-making the complete SM. In particular, it allows
 * a user without access to yacc to add features
 */
void
userfn(i,str)
int *i;
char *str;
{
   int ac;				/* number of arguments */
   char *av[10];			/* list of arguments */
   char buff[10];
   int j;
/*
 * First we'll cut the line up into words, putting them into av[1]...av[ac-1]
 * (av[0] gets the integer you specified as a string)
 */
   sprintf(buff,"%d",*i);
   ac = 0;
   av[ac++] = buff;
   if(*(av[ac] = next_word(str)) != '\0') ac++;
   for(;ac < 10;ac++) {
      if(*(av[ac] = next_word((char *)NULL)) == '\0') break;
   }
/*
 * Now do something
 */
   if(*i != 0) {
      switch(*i) {
       case 1:				/* set a vector to a constant */
	 {
	    VECTOR v;
	    
	    if(ac <= 2) {
	       msg("User 1: Needs two arguments\n");
	       return;
	    }
	    v.type = VEC_FLOAT;
	    if(vec_malloc(&v,1) < 0) {
	       msg_1s("User 1: Can't allocate space for %s\n",av[1]);
	       return;
	    }
	    v.vec.f[0] = atof(av[2]);
	    copy_vector(av[1],v);
	 }
	 return;
       default:
	 msg("User Function ");
	 for(j = 0;j < ac;j++) {
	    msg_1s("%s:",av[j]);
	 }
	 msg("\n");
      }
   }
   
   if(!strcmp(av[1],"segv")) {
      msg("You have requested a segmentation violation: OK\n");
      *i = *(int *)(long)(-*i);
   } else if(!strcmp(av[1],"dump")) {
      dump_stack();
   }
}
