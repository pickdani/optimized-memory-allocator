
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "xmalloc.h"

typedef struct free_list_cell {
	size_t size;
	struct free_list_cell* next;
} free_list_cell;

static __thread pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// free list for each bucket of sizes
// 32, 64, 128, 256, 512, 1024, 2048, 4096 --> 8 buckets
static __thread free_list_cell* buckets[8];
const size_t PAGE_SIZE = 4096;

// first malloc call init all buckets
static __thread int init = 0;

// returns next power of two greater than xx
size_t
get_bucket_size(size_t xx)
{
  size_t yy = 32;
  while (1) {
    if (yy >= xx) {
      return yy;
    }
    yy = yy * 2;
  }
}

// get bucket index from power of two size
// for each bucket to get a page is 8160 ~ 2 pages total
int
get_index(size_t size)
{
	switch (size) {
		case 32 :  return 0;
		case 64 :  return 1;
		case 128 : return 2;
		case 256 : return 3;
		case 512 : return 4;
		case 1204 :return 5;
		case 2048 :return 6;
		default :  return 7;
	}
}

// given bucket index returns size of bucket
// these are only bc I can't modify makefile for math lib
int
get_size(int index)
{
	switch (index) {
		case 0 :  return 32;
		case 1 :  return 64;
		case 2 :  return 128;
		case 3 :  return 256;
		case 4 :  return 512;
		case 5 :  return 1024;
		case 6 :  return 2048;
		default:  return 4096;
	}
}

// mmap large initial amount and distribute it among the buckets
// distribution of 100 pages (409,600):
// this will fill all the buckets at once
// 32  - 128 - 1 page
// 64  -  64 - 1 page
// 128 -  32 - 1 page
// 256 -  16 - 1 page
// 512 -   8 - 1 page
// 1024 -  4 - 1 page
// 2048 -  4 - 2 page
// 4096 -  2 - 2 page
void
init_buckets() {
		void* data = mmap(0, 409600, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		for (int ii = 0; ii < 1280; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) (data + ii * 32);
			new_cell->size = 32;
			new_cell->next = buckets[0];
			buckets[0] = new_cell;
		}
		for (int ii = 0; ii < 640; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) ((data + 40960) + ii * 64);
			new_cell->size = 64;
			new_cell->next = buckets[1];
			buckets[1] = new_cell;
		}
		for (int ii = 0; ii < 320; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) ((data + 40960 * 2) + ii * 128);
			new_cell->size = 128;
			new_cell->next = buckets[2];
			buckets[2] = new_cell;
		}
		for (int ii = 0; ii < 160; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) ((data + 40960 * 3) + ii * 256);
			new_cell->size = 256;
			new_cell->next = buckets[3];
			buckets[3] = new_cell;
		}
		for (int ii = 0; ii < 80; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) ((data + 40960 * 4) + ii * 512);
			new_cell->size = 512;
			new_cell->next = buckets[4];
			buckets[4] = new_cell;
		}
		for (int ii = 0; ii < 40; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) ((data + 40960 * 5) + ii * 1024);
			new_cell->size = 1024;
			new_cell->next = buckets[5];
			buckets[5] = new_cell;
		}
		for (int ii = 0; ii < 40; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) ((data + 40960 * 6) + ii * 2048);
			new_cell->size = 2048;
			new_cell->next = buckets[6];
			buckets[6] = new_cell;
		}
		for (int ii = 0; ii < 20; ++ii) {
			free_list_cell* new_cell = (free_list_cell*) ((data + 40960 * 8) + ii * 4096);
			new_cell->size = 4096;
			new_cell->next = buckets[7];
			buckets[7] = new_cell;
		}
}

// given the index of a bucket, mmaps 50 pages for this bucket
//size, number slots created
//32	 640
//64	 320
//128	 160
//256	  80
//512	  40
//1024	20
//2048	10
//4096   5
void
refill_bucket(int bucket)
{
	int size = get_size(bucket);
	void* data = mmap(0, 4096 * 50, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	for (int ii = 0; ii < (4096 * 50) / size; ++ii) {
		free_list_cell* new_cell = (free_list_cell*) (data + ii * size);
		new_cell->size = size;
		new_cell->next = buckets[bucket];
		buckets[bucket] = new_cell;
	}
}

// given an index for a certain bucket returns
// a pointer to a free space or allocates more space
free_list_cell*
get_free_cell(int index)
{
	if (buckets[index] == 0) {
		refill_bucket(index);
		//init_buckets();
		free_list_cell* front = buckets[index];
		buckets[index] = buckets[index]->next; 
		return front;
	}
	else {
		free_list_cell* front = buckets[index];
		buckets[index] = buckets[index]->next; 
		return front;
	}
}

void
xfree(void* item)
{
	size_t* size_loc = item - sizeof(size_t);
	if (*size_loc > 4096) {
		munmap(size_loc, *size_loc);
	}
	else {
		free_list_cell* new_cell = (free_list_cell*)size_loc;
		new_cell->size = *size_loc;

		pthread_mutex_lock(&lock);
		int index = get_index(new_cell->size);
		
		// cons to front of respective bucket
		new_cell->next = buckets[index];
		buckets[index] = new_cell;
		pthread_mutex_unlock(&lock);
	}
}

void*
xmalloc(size_t nbytes)
{
	size_t size = nbytes + sizeof(size_t);
	
	if (init == 0) {
		init_buckets();
		init = 1;
	}
	pthread_mutex_lock(&lock);
	// directly mmap for large allocations
	if (size > PAGE_SIZE) {
		void* data = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		size_t* size_loc = (size_t*) data;
		*size_loc = size;
		pthread_mutex_unlock(&lock);
		return data + sizeof(size_t);
	}

	// set minimum allocation size to be size of free cell
	if (size < sizeof(free_list_cell)) {
		size = sizeof(free_list_cell);
	}

	// get closest bucket size
	size = get_bucket_size(size);
	
	int index = get_index(size);

	// get free cell from bucket
	free_list_cell* space = get_free_cell(index);

	size_t* size_loc = (size_t*)space;
	*size_loc = size;
	pthread_mutex_unlock(&lock);
	return ((void*)space) + sizeof(size_t);
}

void*
xrealloc(void* prev, size_t nn)
{
  void* new_data = xmalloc(nn + sizeof(size_t));
	size_t* sizepp = prev - sizeof(size_t);
	//pthread_mutex_lock(&lock);
	memcpy(new_data, prev, *sizepp);
	//pthread_mutex_unlock(&lock);
  xfree(prev);
  return new_data;
}
