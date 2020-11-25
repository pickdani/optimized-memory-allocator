# optimized-memory-allocator
Optimized multithread bucket allocator with the goal of outperforming the system allocator.

![performance graph](https://github.com/pickdani/optimized-memory-allocator/blob/main/src/graph.png)


### hwx_malloc.c
baseline singly linked free-list allocator
  
### sys_malloc.c
wrapper for system allocator
  
### opt_malloc.c
optimized thread-safe bucket allocator

### Supported functions
```
void*  xmalloc(size_t bytes);
void     xfree(void* ptr);
void* xrealloc(void* prev, size_t bytes);
```
