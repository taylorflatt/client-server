// Defines DTRACE() macro to simplify tracing output:
//
// DTRACE() takes identical arguments as printf(), but is included
// in program only when DEBUG macro is defined.
// This can be done as:  gcc -DDEBUG ...
//
// Note that tracing messages are printed to stderr.

#ifdef DEBUG
#define DTRACE(args...) fprintf(stderr, args)
#else
#define DTRACE(args...) 
#endif