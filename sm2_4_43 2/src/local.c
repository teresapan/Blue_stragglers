/*
 * This is C code that is included into main() just before the first
 * user-specified code is executed. A simple use would be to write a
 * banner, although this might be better done in the startup macro.
 */
#if 0					/* this is an example of calling
					   SM code in local.c */
   if(greeting && no_editor < 2) {
      printf("Welcome to ");
      push("version\n",S_NORMAL);      
   }
#endif
