
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

#include "hmalloc.h"

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

// full size of a free_list_cell is 16 bytes
// therefore minimum allocation size is 16 bytes
/*
  typedef struct free_list_cell {
  size_t size;
  struct free_list_cell* next;
  } free_cell;
*/

const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.
static free_list_cell* free_list; // point to head of free list

long
free_list_length()
{
	if (free_list == NULL) {
		//printf("free_list is NULL\n");
		return 0;
	}
	else {
		long count = 1;
		free_list_cell* head = free_list;
		// traverse next to next until next is null indicating end
		while (1) {
			if (head->next == NULL) {
				return count;
			}
			else {
				count++;
				head = head->next;
			}
		}
	}
}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}


// given number of bytes, size of page, returns how many pages
// need to be allocated.
static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

// helper to call mmap given number of pages
void*
mmap_pages(size_t num_pages)
{
	stats.pages_mapped += num_pages;
	return mmap(0, num_pages * PAGE_SIZE,
			PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
}

// delete first free_list cell with at least given size
// Credit (modified): Philluminati
// URL: https://codereview.stackexchange.com/questions/496
void
delete_item(size_t size) {
	free_list_cell* iterator;
	iterator = free_list;

	if (iterator == NULL) {
		return;
	}

	// found as first element
	if (iterator->size >= size) {
		free_list = free_list->next;
	}

	while (iterator->next != NULL) {
		if (iterator->next->size >= size) {
			iterator->next = iterator->next->next;
			return;
		}
		iterator = iterator->next;
	}
}


// combine blocks next to eachother in memory into one
void
coalesce() 
{
	if (free_list == NULL) {
		return;
	}
	else {
		free_list_cell* head = free_list;	
		
		while (1) {
			if (head->next == NULL) {
				return;
			}
			else {
				// found two blocks of memory to combine
				if (((void*)head) + head->size == head->next) {
					head->size += head->next->size;
					head->next = head->next->next;
					continue;
				}
				head = head->next;
			}

		}
	}

}

// insert into a sorted position on the free list by memory address
// if there are two free regions next to eachother, coalesce them
// credit (modfied) URL: https://www.geeksforgeeks.org/given-a-linked-list-which-is-sorted-how-will-you-insert-in-sorted-way/
void
insert_item(free_list_cell* new) {
	free_list_length();
	// static free_list_cell* free_list; is head
	// currently empty, just set as head
	if (free_list == NULL) {
		free_list = new;
		// nothing to coalesce
		return;
	}
	else if (free_list >= new) {
		// inserting onto front of free list
		new->next = free_list;
		free_list = new;
	}
	else {
		free_list_cell* current = free_list;
		// find address before insertion
		while (current->next != NULL && current->next <= new) {
			// current will be the cell before the insertion
			current = current->next;
		}
		new->next = current->next;
		current->next = new;
	}
	// call helper to combine blocks
	coalesce();
}

void*
hmalloc(size_t size)
{
	stats.chunks_allocated += 1; // counts # of hmalloc calls
	
	size += sizeof(size_t); // extra 8 bytes for size
	void* data = NULL;	

	// large allocation -> directly call mmap
	if (size >= PAGE_SIZE) {

		size_t num_pages = div_up(size, PAGE_SIZE);

		data = mmap_pages(num_pages);

		// fill in the size of the block as # of pages * PAGE_SIZE
		size_t* size_loc = (size_t*)data;
		*size_loc = num_pages * PAGE_SIZE;

		// return pointer after the size field
		return data + sizeof(size_t);
	}
	else {
		// allocation of less than one page:
		
		// 1. check free list, there is space, we fill this space
		free_list_cell* head = free_list;
		
		if (head){
			while (1) {
				// found space on free list
				if (head->size >= size) {
					data = head;
					
					// remove from free_list
					delete_item(size);
				}
				if (head->next == NULL) {
					break;
				}
				else {
					// go to next element
					head = head->next;
				}
			}
		}


		// 2. no space on free list, then new mmap, append leftover to free list
		// map new page
		size_t block_size;
		if (data == NULL) {
			block_size = PAGE_SIZE;
			data = mmap_pages(1);
		}
		else {
			// have size of data from free list
			block_size = ((free_list_cell*)data)->size;
		}
		

		// if leftover data is less than 16B we don't want to add it to the free list
		size_t leftover = block_size - size;

		if (leftover >= 16){
			// where free list should point
			//printf("insert into free list ONE\n");

			// size here already has += 8, so this is correct
			free_list_cell* new_cell = (free_list_cell*) (data + size);

			//printf("leftover = %d, size = %d\n", leftover,size);
			new_cell->size = leftover;
			new_cell->next = NULL; // next points to previous free list
			insert_item(new_cell);
			//printf("%p %dB leftover added to free_list at\n", free_list, leftover);
		}
		else {
			// less than 16 gets added to size
			size += leftover;
		}

		// set size
		size_t* size_loc = (size_t*) data;
		*size_loc = size; // size here already accounts for sizeof(size_t)
		
		// move pointer after size
		data += sizeof(size_t);
		return data;
	}
}

void
hfree(void* item)
{
    stats.chunks_freed += 1; // counts $ hfree calls
	// size is the actual pointer to start of the memory we want to free
	size_t* size = item - sizeof(size_t); // get the size which was stored before	

	// check if size is bigger than 4096, if yes directly munmap it
	if (*size >= PAGE_SIZE) {
		
		// calculate nubmer of pages to munmap
		size_t num_pages = div_up(*size, PAGE_SIZE);
		
		stats.pages_unmapped += num_pages; 
		munmap(size, num_pages * PAGE_SIZE);
	}
	else {
		// if block is less than one page stick it on free list
		free_list_cell* new_cell = (free_list_cell*) size;
		new_cell->size = *size;
		new_cell->next = NULL;
		insert_item(new_cell);
	}
}

