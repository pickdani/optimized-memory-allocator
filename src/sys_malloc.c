
#include <stdlib.h>

#include "xmalloc.h"

void*
xmalloc(size_t bytes)
{
    return malloc(bytes);
}

void
xfree(void* ptr)
{
    free(ptr);
}

void*
xrealloc(void* prev, size_t bytes)
{
    return realloc(prev, bytes);
}
