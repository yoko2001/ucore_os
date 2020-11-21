#ifndef __SLUB__H__
#define __SLUB__H__

#include <defs.h>
#include <x86.h>
#include <atomic.h>
#include <list.h>
#include <string.h>
#include "pmm.h"

#define NODE_SHIFT 16
#define NODE_MASK 0xffff
#define for_each_object(__p, __s, __addr, __objects) \
	for (__p = (__addr); __p < (__addr) + (__objects) * (__s)->size;\
			__p += (__s)->size)

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
/* align addr on a size boundary - adjust address up if needed -- Cort */
#define _ALIGN(addr,size)   (((addr)+(size)-1)&(~((size)-1)))
#define ALIGN(x,a)    (((x)+(a)-1)&~((a)-1)) 

#define POISON_INUSE           0x6b     //Magic Num

#define KCN_listlocked          0       // if this bit=1, the list is locked

static uint16_t slab_state;            //before setup 0 , after setup 1
#define DOWN  0
#define UP    1

#define LockCacheNodeList(node)           set_bit(KCN_listlocked, &((node)->cn_flags))  
#define CacheNodeListLocked(node)         test_bit(KCN_listlocked, &((node)->cn_flags))
#define UnlockCacheNodeList(node)         clear_bit(KCN_listlocked, &((node)->cn_flags))


//slab page flag
#define PARTIAL 0
#define FULL 1
#define SlabPageSetPartial(page)          set_bit(PARTIAL, &((page)->flags))
#define SlabPageTestPartial(page)         test_bit(PARTIAL, &((page)->flags))
#define SlabPageClearPartial(page)        clear_bit(PARTIAL, &((page)->flags))
#define SlabPageSetFull(page)             set_bit(FULL, &((page)->flags))
#define SlabPageTestFull(page)            test_bit(FULL, &((page)->flags))
#define SlabPageClearFull(page)           clear_bit(FULL, &((page)->flags)) 


#define SLUB_POISON     0

#define SetCachePoison(kc)            set_bit(SLUB_POISON, &((kc)->flags))
#define CachePoison(kc)               test_bit(SLUB_POISON, &((kc)->flags))
#define ClearCachePoison(kc)          clear_bit(SLUB_POISON, &((kc)->flags))

#define MAX_NUMNODES 64        //an arbitrary number

#define SLAB_HWCACHE_ALIGN 4   //not sure what this value should be in i386


struct kmem_cache_node{
    int16_t cn_flags;       //0:list_lock_bit
    uint32_t nr_partial;  /*Num of Partial slabs*/
    uint32_t nr_total;
	list_entry_t partial;  /*Partial slab's lists*/
    list_entry_t full;
};

struct kmem_cache_cpu{
    void **freelist;	/* list entry Pointer to next available(free/partial) object */
    int16_t tid;	    /* Globally unique transaction id */
                        /* Used to confirm it's on the right cpu */
                        /* Also used as a lock*/ 
    struct slub_page* page;         /* The (conpound) slab page(s) from which we are allocating */   
    struct slub_page* partial;      /* BackUp pages, Partially allocated frozen slabs */
                        /* Once all allocated, however 1+ object was retracted */
};

/*
 * Slab cache management.
 */
struct kmem_cache{
    struct kmem_cache_cpu * cpu_slab;  /* Per CPU Cache management */
    int16_t flags;
        // unsigned long min_partial;
    uint32_t size;		        /* The size of an object including meta data */ 
	uint32_t object_size;	    /* The size of an object without meta data */ 
	uint32_t offset;		        /* Free pointer offset. */
    uint32_t cpu_partial;        /* Number of per cpu partial objects to keep around */
    
    /*kmem_cache_order_objects oo */
    uint16_t obj_num;           /* num of objects in one slab allocation */
    uint16_t order;             /* num of pages in one allocation */
                                /* a number like 1 << n */
    uint16_t min;                /* min order */
    //gfp_t allocflags;	/* gfp flags to use on each alloc */
    int refcount;               /* Refcount for slab cache destroy */


    uint32_t inuse;		        /* Offset to metadata */
    uint32_t align;             /* Alignment */ 
    int reserved;		        /* Reserved bytes at the end of slabs */
    uint64_t nodestatus;
	const char *name;	        /* Name (only for display!) */  

    list_entry_t list;         /* List of slab caches */
    void *(*ctor) (void * self, void* obj); //构造函数 

    struct kmem_cache_node *node[MAX_NUMNODES];

};
static list_entry_t slab_caches;

/* used before slub is open then we'll deal with this two mem segment */
/* we'll use the OPENED slub to manage it's self kmem_caches & kmem_cache_nodes */
/* which is named <self-management> */
static struct kmem_cache boot_kmem_cache, boot_kmem_cache_node;
static struct kmem_cache_cpu this_kmem_cache_cpu;


/*

 */
struct slub_page{
    struct Page* p;
    list_entry_t list;
    int16_t obj_num;
    int16_t frozen;     //frozen == 1: slab is in cpulab; frozen == 0: slab is in node or its no-empty-obj slab
    int16_t inuse;      //inuse : used objects
    int32_t flags;
    int16_t pages;
    int16_t pobjects;
    struct kmem_cache* slub_cache;
    void* freelist;                 //internal free object list
};
static inline int page_to_nid(const struct slub_page* page){
    struct slub_page* p = (struct slub_page*) page;
    return (p->flags >> NODE_SHIFT) & NODE_MASK;
}

static inline int set_page_nid_flag(struct slub_page* page, int i){
    page->flags = page->flags | ((i & NODE_MASK) << NODE_SHIFT);
}
static inline void set_page_is_partial(struct slub_page* page){
    SlabPageSetPartial(page);
    SlabPageClearPartial(page);
}
static inline void set_page_is_full(struct slub_page* page){
    SlabPageSetFull(page);
    SlabPageClearPartial(page);
}
void kmem_cache_init(void);

#endif