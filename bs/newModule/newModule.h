#ifndef NEW_MODULE_H
#define NEW_MODULE_H

#ifdef NEW_MODULE_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* NEW_MODULE_C */



#undef PUBLIC

#endif /* NEW_MODULE_H */

/* end of file */
