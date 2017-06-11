#include "copyright.h"
#include <stdio.h>
#include "options.h"
#include "sm_declare.h"
/*
 * GSTRCPY -- Copy string s2 to s1, return the number of characters copied
 */
int
gstrcpy (s1, s2, maxch)
char	s1[], s2[];
int	maxch;
{
   int i;

   maxch--;				/* leave room for '\0' */
   for(i=0;s2[i] != '\0' && i < maxch;i++) s1[i] = s2[i];
   s1[i] = '\0';

   return (i);
}
