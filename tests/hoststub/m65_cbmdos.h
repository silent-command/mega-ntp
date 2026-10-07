/* host stand-in for the test of places.c */
#ifndef HOST_CBMDOS_H
#define HOST_CBMDOS_H
unsigned long cbmdos_load(const char *name, unsigned char drive, unsigned long dest, unsigned long maxlen);
#endif
