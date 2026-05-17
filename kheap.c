#include <inc/memlayout.h>
#include <kern/kheap.h>
#include <kern/memory_manager.h>

//2022: NOTE: All kernel heap allocations are multiples of PAGE_SIZE (4KB)

uint32 next_fit_ptr = KERNEL_HEAP_START;  // marks where the search starts

// Size tracking: for each page in the kernel heap, store how many pages
// were allocated starting at this page (0 if this page isn't an allocation start).
// Number of pages in kernel heap = (KERNEL_HEAP_MAX - KERNEL_HEAP_START) / PAGE_SIZE
// We use a static array sized to cover the entire kernel heap range.
#define NUM_KHEAP_PAGES ((KERNEL_HEAP_MAX - KERNEL_HEAP_START) / PAGE_SIZE)
uint32 kheap_sizes[NUM_KHEAP_PAGES];

void* kmalloc(unsigned int size)
{
   if(size==0)return NULL;
   uint32 num_pages=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
   uint32 start_search=next_fit_ptr;
   uint32 va = next_fit_ptr;
   uint32 count=0;
   uint32 alloc_start=0;
   while(1)
   {
	   uint32* pt=NULL;
	   struct Frame_Info* fi=get_frame_info(ptr_page_directory,(void*)va,&pt);
	   if(fi==NULL)
	   {
		   if(count==0) alloc_start=va;
		   count++;
		   if(count==num_pages)
			   break;
	   }
	   else
	   {
		   count=0;
		   alloc_start=0;
	   }
	   va+=PAGE_SIZE;
	   if(va>= KERNEL_HEAP_MAX)va=KERNEL_HEAP_START;
	   if(va==start_search)
	   {
		   return NULL;
	   }
   }
	   for(uint32 i=0;i<num_pages;i++)
	   {
		   struct Frame_Info* frame;
		   if(allocate_frame(&frame)!=0)
		   {
			   return NULL;
		   }
		   map_frame(ptr_page_directory,frame,(void*)(alloc_start+i*PAGE_SIZE),PERM_WRITEABLE);

	   }
	   uint32 start_index=(alloc_start - KERNEL_HEAP_START)/PAGE_SIZE;

	   kheap_sizes[start_index] = num_pages;

	   next_fit_ptr = alloc_start + num_pages * PAGE_SIZE;
	   if (next_fit_ptr >= KERNEL_HEAP_MAX) next_fit_ptr = KERNEL_HEAP_START;

    return (void*)alloc_start;
}

void kfree(void* virtual_address)
{
	uint32 va = (uint32)virtual_address;
	uint32 start_index=(va-KERNEL_HEAP_START)/PAGE_SIZE;
	uint32 num_pages=kheap_sizes[start_index];
	if(num_pages==0)
	{
		return;
	}
	for(uint32 i=0;i<num_pages;i++)
	{
		unmap_frame(ptr_page_directory,(void*)(va+i*PAGE_SIZE));

	}
	kheap_sizes[start_index]=0;

}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
   uint32 rounded_ad=ROUNDDOWN(physical_address,PAGE_SIZE);
	uint32 offset=physical_address & 0xfff;
	for(uint32 virt_ad=KERNEL_HEAP_START;virt_ad<KERNEL_HEAP_MAX;virt_ad+=PAGE_SIZE)
	{
		uint32 phys_ad=kheap_physical_address(virt_ad);
		if(phys_ad==rounded_ad)
		{
			return (virt_ad & ~(0xfff))|offset;
		}
	}

    return 0;
}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
  if(virtual_address<KERNEL_HEAP_START|| virtual_address>KERNEL_HEAP_MAX)
  {
	  cprintf("virtual address out of boundries");
	  return 0;
  }
  uint32* ptr_page_table=NULL;
  get_page_table(ptr_page_directory,(void*)virtual_address,&ptr_page_table);
  if(ptr_page_table!=NULL)
  {
	  uint32 pg_table_entry=ptr_page_table[PTX(virtual_address)];
	  if(pg_table_entry&PERM_PRESENT)
	  {
		  return (pg_table_entry &~(0xfff))| (virtual_address & 0xfff);

	  }
  }

    return 0;
}
