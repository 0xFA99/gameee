//
// Created on 2/28/26.
//

#ifndef GAMEEE_MMAPALLOC_H
#define GAMEEE_MMAPALLOC_H

#include <stddef.h>

extern void *mmapalloc(size_t size_alloc);
extern void mmapfree(void *ptr);

#endif GAMEEE_MMAPALLOC_H