#include <stdio.h>
#include "sm_declare.h"
  
int
main()
{
   REAL a[10];
   int i;

   for(i = 0;i < 10;i++) a[i] = i;
   
   if(sm_array_to_vector(a,10,"xyz") < 0) {
      fprintf(stderr,"Can't define xyz\n");
      exit(1);
   }

   printf("Calling SM parser\n");

   sm_parser("-q -v0");

   fprintf(stderr,"Exited parser\n");
   return(0);
}
