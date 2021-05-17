#ifndef _MCIN_H
#define _MCIN_H

#include "thpool.h"

int mcin_init();
void mcin_free();
void mcin_match(const char *str, const threadpool thpool);

#endif // _MCIN_H
