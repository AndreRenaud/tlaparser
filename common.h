#ifndef COMMON_H
#define COMMON_H

// returns true of option 'name' was passed on the cmd line
int option_set (char *name);

// returns 1 if option 'name' was set, and puts its value into buffer, up to buff_len bytes maximum
int option_val (char *name, char *buffer, int buff_len);

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

#endif
