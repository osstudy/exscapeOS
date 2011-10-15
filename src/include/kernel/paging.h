#ifndef _PAGING_H
#define _PAGING_H

#include <types.h>
#include <kernel/interrupts.h>

#define PAGE_SIZE 0x1000

/* Used for alloc_frame(), perhaps others */
#define PAGE_USER 0
#define PAGE_KERNEL 1
#define PAGE_READONLY 0
#define PAGE_WRITABLE 1

/* Represents a page entry in memory */
typedef struct page {
	uint32 present  : 1;  /* is page present in physical memory? */
	uint32 rw       : 1;  /* 0 if read-only, 1 if read-write */
	uint32 user     : 1;  /* 0 if kernel-mode, 1 if user-mode */
	uint32 pwt      : 1;  /* Page-level write-through */
	uint32 pcd      : 1;  /* Page-level cache disable */
	uint32 accessed : 1;  /* has the page been accessed (read or written) since last refresh? */
	uint32 dirty    : 1;  /* has the page been written to since last refresh? */
	uint32 pat      : 1;
	uint32 global   : 1;  /* if CR4.PGE = 1, determines whether the translation is global; ignored otherwise */
	uint32 avail    : 3;  /* bits available for use by the OS */
	uint32 frame    : 20; /* high 20 bits of the frame address */
} __attribute__((packed)) page_t;

/* Represents a page table in memory */
typedef struct page_table {
	page_t pages[1024];
} page_table_t;

/* Represents a page directory in memory */
typedef struct page_directory {
	/* An array of pointers to page tables */
	page_table_t *tables[1024];

	/* Physical positions of the above page tables */
	uint32 tables_physical[1024];

	/* The physical address *of* the variable tables_physical */
	uint32 physical_address;
} page_directory_t;

/* Sets up everything required and activates paging. */
void init_paging(unsigned long upper_mem);

/* Loads the page directory at /new/ into the CR3 register. */
void switch_page_directory(page_directory_t *new);

/* Returns a pointer to the page entry responsible for the address at /addr/.
 */
page_t *get_page (uint32 addr, bool create, page_directory_t *dir);

/* The page fault interrupt handler. */
uint32 page_fault_handler(uint32);

void alloc_frame(uint32 virtual_addr, page_directory_t *page_dir, bool kernelmode, bool writable);
void free_frame(uint32 virtual_addr, page_directory_t *page_dir);

bool addr_is_mapped(uint32 addr);

/* Executes INVLPG for the argument. */
void invalidate_tlb(void *addr);

/* Flushes the TLB for all pages (by writing to CR3) */
void flush_all_tlb(void);

/* Clones a page directory */
page_directory_t *clone_directory(page_directory_t *src);

/* Returns the number of bytes free in PHYSICAL memory. */
uint32 free_bytes(void);

/* Converts a virtual address to a physical one (within the current address space, i.e. current_directory is used) */
uint32 virtual_to_physical(uint32 virt_addr);

page_directory_t *create_user_page_dir(void);

#endif /* header guard */
