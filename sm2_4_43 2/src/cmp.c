#include "copyright.h"
/*
 * This programme returns 1 (true) if the two files given as
 * arguments are identical, otherwise it returns 2 (false)
 */
#include <stdio.h>


main(ac,av)
int ac;
char *av[];
{
   char c1,c2;				/* characters read from files */
   int fil1,fil2;			/* fd's for two files */

   if(ac < 3 || (fil1 = open(av[1],0)) < 0 || (fil2 = open(av[2],0)) < 0) {
      exit(2);
   }

   while(1) {
      if(read(fil1,&c1,1) == 1) {
         if(read(fil2,&c2,1) == 1) {
            if(c1 != c2) exit(2);
            else ;
         } else {
            exit(2);
         }
      } else if(read(fil2,&c2,1) == 1) {
         exit(2);
      } else {
         exit(1);
      }
   }
}
