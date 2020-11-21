#include "slub.h"
#include "buddysys_pmm.h"

#define MAX_SLUB_PAGE_NUM 1024

struct slub_page __pages[MAX_SLUB_PAGE_NUM];  //重大问题！这个何时挪走，如何解决？
static uint32_t now_page;
//solution 1 修正page，2和1
//solution 2 最终达成自管理


struct kmem_cache* kmem_cache_ptr_t;            //temp pointer
struct kmem_cache* kmem_cache_node_ptr_t;  //temp pointer

static void *setup_object(struct kmem_cache *s, struct slub_page *page, void *object){
    //if(!s->ctor)
    //    s->ctor(s, object);
}
static inline void set_freepointer(struct kmem_cache *s, void *object, void *fp){
    *(void **)(object + s->offset) = fp;
}
static inline void* get_freepointer(struct kmem_cache*s, void *object){
    return *(void **)(object + s->offset);
    //[start]| object |pointer to next free|object|...
    //so has to skip a object size (which is basicly offset)
}

/*
 * Remove slab from the partial list, freeze it and
 * return the pointer to the freelist.
 * Returns a list of objects or NULL if it fails.
 */
static void *acquire_slab(struct kmem_cache *s, struct kmem_cache_node *n, struct slub_page *page,
int mode, int *objects){
    void* freelist;
    struct slub_page new;
    if(!(page->inuse)){
        cprintf("wcnm\n");
        return NULL;
    }
        
    cprintf("wxnm, %d \n", sizeof(*page));
    freelist = page->freelist;
    cprintf("wxnm\n");
    new.inuse = page->inuse;
    cprintf("wxnm\n");
    new.frozen = page->frozen;
    new.obj_num = page->obj_num;
    //cprintf("wxnm");
    // left over contains
    *objects = new.obj_num - new.inuse;

    //when object == NULL
    /*
     * Zap the freelist and set the frozen bit.         //WHY!!???
     * The old freelist is the list of objects for the  
     * per cpu allocation list.
     */
    //cprintf("ckpt");
    if(mode){
        new.inuse = page->obj_num;
        new.freelist = NULL;
    }else{
        new.freelist = freelist;
    }
    new.frozen = 1;

    if(new.inuse == page->inuse){
        //fail
        cprintf("not found\n");
        return NULL;
    }
    return freelist;
}          

/*
 * Put a page that was just frozen (in __slab_free|get_partial_node) into a
 * partial page slot if available.
 *
 * If we did not find a slot then simply move all the partials to the
 * per node partial list.
 */
static void put_cpu_partial(struct kmem_cache *s, struct slub_page *page, int drain)
{
    struct slub_page *oldpage;
    int pages;
    int pobjects;

    do{
        pages = 0;
        pobjects = 0;
        oldpage = s->cpu_slab->partial;
    
        if (oldpage){
            // pobjects = oldpage->pobjects;
            // pages = oldpage->pages;

            // if(drain && )            
        }

        //put at front
        pages++;
        pobjects += page->obj_num - page->inuse;
        page->pages = pages;
        page->pobjects = pobjects;

        list_add_before(&page->list, &oldpage->list);
    }while(s->cpu_slab->partial != oldpage);
}





/*
 * Try to allocate a partial slab from a specific node.
 */
static void *get_partial_node(struct kmem_cache *s, struct kmem_cache_node *n,
struct kmem_cache_cpu *c)
{
    struct slub_page *page, *page2;
    int objects;
    void *object = NULL;
    unsigned int available = 0;
    /*If we mistakenly see no partial slabs then we
     * just allocate an empty slab. If we mistakenly try to get a
     * partial slab and there is none available then get_partials()
     * will return NULL.
     */
    if (!n)
        return NULL;
    list_entry_t* mark = &n->partial;   //partial list of the node
    list_entry_t* now = mark;           //list head
    do{
        if (!now)break;
        page = to_struct(now, struct slub_page, list);
        void*t;
        cprintf("try to acquire a slab\n");
        /*
        remove page from partial
        objects : available num
        set page -> frozen
         */
        t = acquire_slab(s, n, page, 1, &objects); //return freelist
        if (!t)
            break;
        available += objects;
        cprintf("available: %d\n", available);
        if (!object) {
            c->page = page;
            object = t;
        } else {
            put_cpu_partial(s, page, 0);
        }
        // we don't support this optimization static
        // if (available > s-> / 2)
        //     break;
        if(available >= 1){
            break;
        }
        now = list_next(now);
    }while(mark != now);
    cprintf("available: %d\n", available);
    return object;
}


/*
 * Get a partial page, lock it and return it.
 */
static void *get_partial(struct kmem_cache *s,struct kmem_cache_cpu *c)
{
    void *object;
    int searchnode = 0;
    //cprintf("%d", s->node[searchnode]->nr_partial);
    if (slab_state == DOWN)
        return NULL;
    object = get_partial_node(s, s->node[searchnode], c);
     cprintf("get_partial_node passed\n");
    return object;
}

static void *__slab_alloc(struct kmem_cache *s, struct kmem_cache_cpu *c);
/*
 * Inlined fastpath so that allocation functions (kmalloc, kmem_cache_alloc)
 * have the fastpath folded into their functions. So no function call
 * overhead for requests that can be satisfied on the fastpath.
 *
 * The fastpath works by first checking if the lockless freelist can be used.
 * If not then __slab_alloc is called for slow processing.
 *
 * Otherwise we can simply pick the next object from the lockless free list.
 */
static __always_inline void* slab_alloc(struct kmem_cache *s)
{
    void ** object;
    struct kmem_cache_cpu* c;
    struct slub_page* page;
    unsigned long tid;
    int node;
    // ONLY WHEN DEBUG IS ON        [SKIP!!!]
    //if (slab_pre_alloc_hook(s, gfpflags))
	//	return NULL;
    // ONLY WHEN CONFIG_MEMCG_KMEM [SKIP!!!]
    //s = memcg_kmem_get_cache(s, gfpflags);
redo:
    c = s->cpu_slab;
    tid = c->tid;
    object = c->freelist;
    page = c->page;
    cprintf("%p\n", s->node[0]);
    if(!object || object == 0x6b6b6b6b){
        cprintf("slow path\n");
        object = __slab_alloc(s, c);    //this is slow
    }    
    else{
        cprintf("%s %p\n", s->name, object);
        void *next_object = get_freepointer(s, object); //!!! probably not safe
        cprintf("ckpt a\n");
        if(unlikely(!next_object)){
            cprintf("slab_alloc_fail, trying again\n");
            goto redo;
        }
        //[DONT REMOVE]prefetch_freepointer(s, next_object); //fresh data
        //add data to cache
        //wo add ni ma wo mei you cache
    }
    if (object){
        memset(object, 0, s->object_size);
    }
    // [NOT REMOVE] Some operations of KASAN(mem detect tool of linux kern)
    // [NOT REMOVE] slab_post_alloc(s, object);// post process after allocating
    return object;
} 
static inline void flush_cpu_slab(struct kmem_cache *s, struct kmem_cache_cpu *c);

/*
 * Check the page->freelist of a page and either transfer the freelist to the
 * per cpu freelist or deactivate the page.
 * The page is still frozen if the return value is not NULL.
 * If this function returns NULL then the page has been unfrozen.
 * This function must be called with interrupt disabled.
 */
static inline void *get_freelist(struct kmem_cache *s, struct slub_page *page)
{
    struct slub_page new;
    int16_t inuse, frozen;
    void *freelist;
    freelist = page->freelist;
    frozen = page->frozen;
    if (frozen && (!freelist)){
        return freelist;
    }else{
        return NULL;
    }

}


static inline void *new_slab_objects(struct kmem_cache *s, struct kmem_cache_cpu **pc);
//slow path of slab allocating
/*
 * Slow path. The lockless freelist is empty or we need to perform
 * debugging duties.
 *
 * Processing is still very fast if new objects have been freed to the
 * regular freelist. In that case we simply take over the regular freelist
 * as the lockless freelist and zap the regular freelist.
 *
 * If that is not working then we fall back to the partial lists. We take the
 * first element of the freelist as the object to allocate now and move the
 * rest of the freelist to the lockless freelist.
 *
 * And if we were unable to get a new slab from the partial slab lists then
 * we need to allocate a new slab. This is the slowest path since it involves
 * a call to the page allocator and the setup of a new slab.
 *
 * Version of __slab_alloc to use when we know that interrupts are
 * already disabled (which is the case for bulk allocation).
 */
static void *__slab_alloc(struct kmem_cache *s, struct kmem_cache_cpu *c){
    void* freelist;
    struct slub_page* page;
    page = c->page;
    if(!page)
        goto new_slab1;
redo:
     cprintf("ckpt1\n");
    if(1){ // node ok
        //deactivate_slab(s, page, c->freelist);
		flush_cpu_slab(s, c);
        c->page = NULL;
		c->freelist = NULL;
		goto new_slab1;
    }
     cprintf("ckpt2\n");
    if (freelist)
		goto load_freelist;
     cprintf("ckpt3\n");
    freelist = get_freelist(s, page);
    if (!freelist) {
		c->page = NULL;
		goto new_slab1;
	}
     cprintf("ckpt4\n");
load_freelist:
    /*
	 * freelist is pointing to the list of objects to be used.
	 * page is pointing to the page from which the objects are obtained.
	 * That page must be FROZEN for per cpu allocations to work.
	 */
    if(!(c->page->frozen))
        panic("PAGE FROZEN ERROR IN __slab_alloc \n");
    cprintf("frozen check passed \n");
    page->freelist = freelist;
    cprintf("cpu freelist : %p\n", c->freelist);
    c->freelist = freelist;//get_freepointer(s, freelist); //give node start to freelist
    cprintf("cpu freelist : %p\n", c->freelist);
    
    //set partial
    c->partial = page;
    set_page_is_partial(page);
    
    c->tid = 0; //c->tid = c->tid+1; //!!!
    return freelist;

new_slab1:
    //check if there's pages in partial
    if(c->partial){
        cprintf("try to allocate one from cpu partial\n");
        page = c->page = c->partial;    //we can get page from partial
        c->partial = to_struct((list_next(&page->list)), struct slub_page, list);
        c->freelist = NULL;
        goto redo;
    }else{
        //try to find one partial page from nodes
        cprintf("try to allocate one from nodes\n");
        for(int i = 0; i < MAX_NUMNODES; i++){
            struct slub_page* tmp;
            //cprintf("%s, \n", s->name);
            if(!s->node[i] || s->node[i] == 0x6b6b6b6b) continue;
            cprintf("node[%d], at %p\n",i, s->node[i]);
            if (list_next(&s->node[i]->partial) == &s->node[i]->partial)
                continue;
            tmp = to_struct(list_next(&s->node[i]->partial), struct slub_page, list);
            cprintf("tmp at %p, list %p\n", tmp, &tmp->list);
            cprintf("tmp freelist %p\n", tmp->freelist);
            if (tmp){
                freelist = c->freelist = tmp->freelist;
                page = c->page = tmp;
                c->page->frozen = 1; //has to frozen it when put in cpu
                c->partial = tmp;
                cprintf("got one\n");
                goto load_freelist;
            } 
        }
        
    }
     //cprintf("ckpt6\n");
    freelist = new_slab_objects(s, &c);
    cprintf("new_slab created\n");
    if(unlikely(!freelist)){
        panic("slab mem error, maybe low mem\n");
        return NULL;
    }
    page = c->page;
    goto load_freelist;
}

static struct slub_page* new_slab(struct kmem_cache *s, int node);

static inline void *new_slab_objects(struct kmem_cache *s, struct kmem_cache_cpu **pc){
    void *freelist;
	struct kmem_cache_cpu *c = *pc;
	struct slub_page *page;

    freelist = get_partial(s, c);       //try alloc from partial
    if (freelist)
		return freelist;
    cprintf("get partial failed, new a slab\n");
    //failed, try to new a slab
    page = new_slab(s, 0);
    if(page){
        //cprintf("new page yes\n");
        c = &this_kmem_cache_cpu;
        if (c->page)
			flush_cpu_slab(s, c);
		/*
		 * No other reference to the page yet so we can
		 * muck around with it freely without cmpxchg
		 */
        /* new empty page */
		freelist = (page->freelist);
        cprintf("frlst: %p\n", freelist);
		page->freelist = NULL;
		c->page = page;
		*pc = c;    //a little bit wasty in this cpu
    }else{
        freelist = NULL;
    }
    return freelist;
}


/* Entry */
/*   */
void create_boot_cache(struct kmem_cache* s, const char*name ,size_t size, uint16_t hwalign);
static struct kmem_cache * bootstrap(struct kmem_cache *static_cache);
void kmem_cache_init(void){
    //!!!
    list_init(&slab_caches);
    now_page = 0;
    memset(__pages, 0, sizeof(struct slub_page)*MAX_SLUB_PAGE_NUM);
    //!!!

    slab_state = DOWN;
    kmem_cache_ptr_t = &boot_kmem_cache;
    kmem_cache_node_ptr_t = &boot_kmem_cache_node;
    //cprintf("sizeof(struct kmem_cache_node) %d\n", sizeof(struct kmem_cache_node));
    struct kmem_cache_node tmp;
    //cprintf("add all %d\n", sizeof(tmp.cn_flags)+sizeof(tmp.nr_partial)+sizeof(tmp.nr_total)+sizeof(tmp.full) + sizeof(tmp.partial));
    create_boot_cache(kmem_cache_node_ptr_t, "kmem_cache_node",
		sizeof(struct kmem_cache_node), SLAB_HWCACHE_ALIGN);
    cprintf("create_boot_cache(kmem_cache_node) passed\n");
    /* Able to allocate the per node structures */
    create_boot_cache(kmem_cache_ptr_t, "kmem_cache", offsetof(struct kmem_cache, node) + sizeof(struct kmem_cache_node*) * MAX_NUMNODES, SLAB_HWCACHE_ALIGN);
    cprintf("create_boot_cache(kmem_cache) passed\n");
    cprintf("%p\n", kmem_cache_ptr_t->node[0]);
    cprintf("%p\n", boot_kmem_cache.node[0]);
    kmem_cache_ptr_t = bootstrap(&boot_kmem_cache);
    cprintf("bootstrap of kmem_cache passed \n");
    kmem_cache_node_ptr_t = bootstrap(&boot_kmem_cache_node);
    cprintf("bootstrap of kmem_cache_node passed \n");
    //NOTEND !!!
}

static void init_kmem_cache_node(struct kmem_cache_node *n){
    cprintf("ckpt %p\n", n);
    n->nr_partial = n->nr_total = 0;
    cprintf("ckpt\n");
    //lock node
    list_init(&(n->partial)); //init page list
    cprintf("%p\n", &(n->partial));
    list_init(&(n->full));
    
    //unlock node
}

void *kmem_cache_alloc(struct kmem_cache *s);

/********************************************************************
 *			Basic setup of slabs
 *******************************************************************/
/*
 * Used for early kmem_cache structures that were allocated using
 * the page allocator. Allocate them properly then fix up the pointers
 * that may be pointing to the wrong kmem_cache structure.
 */
static struct kmem_cache * bootstrap(struct kmem_cache *static_cache){
    int node;
    struct kmem_cache* s = kmem_cache_alloc(kmem_cache_ptr_t);
    cprintf("new slab allocated, at %p, from %p\n", s, static_cache);
    cprintf("check node %p\n", s->node[0]);
    memcpy(s, static_cache, kmem_cache_ptr_t->object_size); //copy to new space
    cprintf("moved to new place\n");
    /*
	 * This runs very early, and only the boot processor is supposed to be
	 * up. 
	 */
    cprintf("try to flush cpu slab, %s\n", s->name);
    flush_cpu_slab(s, &this_kmem_cache_cpu);
    cprintf("flushed cpu slab\n");

    for(int i = 0; i < MAX_NUMNODES; i++){
        struct slub_page* page;
        if(!s->node[i]){
            continue;
        }
        if ((list_next(&s->node[i]->full) == &s->node[i]->full) && 
            (list_next(&s->node[i]->partial) == &s->node[i]->partial)){
                cprintf("%d\n", i);
                continue;
            }
        cprintf("ckpt %d\n", i);
        list_entry_t* pe, *mark;
        mark = &s->node[i]->partial;
        pe = list_next(pe);
        while(pe != mark){
            page = to_struct(pe, struct slub_page, list);
            page->slub_cache = s;
            pe = list_next(pe);
        }
        mark = &s->node[i]->full;
        pe = list_next(pe);
        while(pe != mark){
            page = to_struct(pe, struct slub_page, list);
            page->slub_cache = s;
            pe = list_next(pe);
        }
    }
    //cprintf("%p, %p", &slab_alloc, &s->list);
    list_add_after(&slab_caches, &s->list);
    cprintf("bootstrap success\n");
    return s;
    // not finished!!!
}

static inline void flush_cpu_slab(struct kmem_cache *s, struct kmem_cache_cpu *c){
    struct slub_page* page = c->page;// the pages to free
    if((c->freelist) == NULL || c->freelist == 0x6b6b6b6b){
        cprintf("#flush_cpu_slab: freelist == NULL\n");
        return;
    }
    if(!(c->page) && !(c->partial))
        return;
    cprintf("%p\n", c->freelist);
    cprintf("page: %p\n", c->page);
    cprintf("partial: %p\n", c->partial);
    cprintf("page freept %p \n", page->freelist);
    void* freelist = *(c->freelist);

    void *nextfree;
    struct slub_page newpg;
    struct slub_page oldpg;
    if(!page)
        return;
    struct kmem_cache_node *n = s->node[page_to_nid(page)];
    cprintf("nodeid %d \n", page_to_nid(page));
    /*
	 there should be some safety problem with multi cpus 
     if freelist and the slab to flush has left some free objects
     remove those, point freelist to the last free obj of slab
     */
    cprintf("flushing, freelist %p\n", freelist);
    cprintf("ckpt fl: %p, next fp: %p\n", freelist, get_freepointer(s, freelist));
    while (freelist && (nextfree = get_freepointer(s, freelist))) {
        void *prior;
        int16_t obj_num;
        int16_t frozen;
        int16_t inuse;
        do {
            
            prior = freelist;
            cprintf("%p %p %p\n", freelist, nextfree, prior);
            newpg.inuse = page->inuse;
            newpg.obj_num = page->obj_num;
            newpg.frozen = page->frozen;
            /*freelist = page->freelist */
            set_freepointer(s, freelist, prior);
            newpg.inuse--;
            nextfree = get_freepointer(s, freelist);
        } while (!(freelist == nextfree));
        freelist = nextfree;    //check next
    }

    /* Determine target state of the slab */
redo:
    oldpg.freelist = page->freelist;
    oldpg.inuse = page->inuse;
    oldpg.obj_num = page->obj_num;
    oldpg.frozen = page->frozen;
    //freelist = page->freelist
    //freelist -> last free object of the freed slab;
    
    if (freelist){
        newpg.inuse --; // the last one left
        cprintf("new.inuse %d\n", newpg.inuse);
        set_freepointer(s, freelist, oldpg.freelist);
        newpg.freelist = freelist;
    }else{
        newpg.freelist = oldpg.freelist;
    }
    newpg.frozen = 0; //defrozen
    //empty and 
    cprintf("ckpt\n");
    if(!newpg.inuse){  
        cprintf("empty\n"); 
        //!!! should be removed directly
        buddysys_pmm_manager.free_pages(newpg.p, 1);
    }else if (newpg.freelist){
        //partial
        cprintf("partial\n");
        cprintf("%p", n->partial.next);
        list_add_before(&n->partial, &newpg.list); 
    }else{
        //full
        cprintf("full\n");
        list_add_after(&n->full, &newpg.list);
    }
    cprintf("ckpt\n");
}

void *kmem_cache_alloc(struct kmem_cache *s){
    void *ret = slab_alloc(s);
    //!!!
    //trace_kmem_cache_alloc(_RET_IP_, ret, s->object_size,
    //            s->size, gfpflags);
    return ret;
}


int __kmem_cache_create(struct kmem_cache *s, void (*ctor)(struct kmem_cache *, void *));

/* Create a cache when no slab services are available yet */
/* However Buddy System is available, so we can use it */
void create_boot_cache(struct kmem_cache* s, const char*name ,
                        size_t size, uint16_t hwalign)
{
    int err;
    s->name = name;
    s->size = s->object_size = size;
    s->align = hwalign;
    SetCachePoison(s);
    err = __kmem_cache_create(s, NULL);
    
    if (err)
		panic("Creation of kmalloc slab %s size=%zu failed. Reason %d\n",
					name, size, err);
    
    s->refcount = -1;	/* Exempt from merging for now */
}

static int init_kmem_cache_nodes(struct kmem_cache *s);
static void free_kmem_cache_nodes(struct kmem_cache* s);
static int alloc_kmem_cache_cpus(struct kmem_cache* s);
static int calculate_sizes(struct kmem_cache *s, int forced_order);
int __kmem_cache_create(struct kmem_cache *s, void (*ctor)(struct kmem_cache *, void *)){
    int err;
    s->reserved = 0;    //extention space, set to 0 in this version
    if (!calculate_sizes(s, -1))
		goto error;
    /*
	 * The larger the object size is, the more pages we want on the partial
	 * list to avoid pounding the page allocator excessively.
	 *
	 * cpu_partial determined the maximum number of objects kept in the
	 * per cpu partial lists of a processor.
	 * This setting also determines
	 *
	 * A) The number of objects from per cpu partial slabs dumped to the
	 *    per node list when we reach the limit.
	 * B) The number of objects in cpu partial slabs to extract from the
	 *    per node list when we run out of per cpu objects. We only fetch
	 *    50% to keep some capacity around for frees.
	*/
    if (s->size >= PGSIZE)
		s->cpu_partial = 2;
	else if (s->size >= 1024)
		s->cpu_partial = 6;
	else if (s->size >= 256)
		s->cpu_partial = 13;
	else
		s->cpu_partial = 30;

    if (!init_kmem_cache_nodes(s))
		goto error;
    
    if (alloc_kmem_cache_cpus(s))
		err = 0;
    else
        free_kmem_cache_nodes(s);
    //err = sysfs_slab_add(s);
    //if (err)
	//	kmem_cache_close(s);

	return err;

error: 
    panic("Cannot create slab %s size=%lu realsize=%u "
			"order=%u offset=%u\n",
			s->name, (unsigned long)s->size, s->size,
			s->order, s->offset);
    err = -1;
    return err;
}

static int calculate_sizes(struct kmem_cache *s, int forced_order){
    int16_t flags =  s->flags;
    uint32_t size = s->size;
    int order;

    /*
	 * Round up object size to the next word boundary. We can only
	 * place the free pointer at word boundaries and this determines
	 * the possible location of the free pointer.
	 */
	size = ALIGN(size, sizeof(void *));

    /*
	 * With that we have determined the number of bytes in actual use
	 * by the object. This is the potential offset to the free pointer.
	 */
	s->inuse = size;
    s->offset = size;
	size += sizeof(void *);
    /*
	 * SLUB stores one object immediately after another beginning from
	 * offset 0. In order to align the objects we have to simply size
	 * each object to conform to the alignment.
	 */
	size = ALIGN(size, s->align);
    s->size = size;
	if (forced_order >= 0)
		order = forced_order;
	else 
        order = 4;      //arbitrary !!!
    if (order < 0)
        return 0;
    
    /*
	 * Determine the number of objects per slab
	 */
    s->order = order;
    
    return 1;
}
static void early_kmem_cache_node_alloc(struct kmem_cache *s, int node);
static int init_kmem_cache_nodes(struct kmem_cache *s){
    int i = 0;
    struct kmem_cache_node* n;
       for(i = 0; i < MAX_NUMNODES ; i++){
            struct kmem_cache_node *n;

            if (slab_state == DOWN) {
                // before SLUB SETUP
                //cprintf("early_kmem_cache_node_alloc\n");
                early_kmem_cache_node_alloc(s, i);
                continue;
            }
            //after SLUB SETUP
            // n = kmem_cache_alloc_node(kmem_cache_node,
            // 				GFP_KERNEL, node);

            // if (!n) {
            // 	free_kmem_cache_nodes(s);
            // 	return 0;
            // }
            // s->node[i] = n;
            // init_kmem_cache_node(n);
        } 
    
    
    return 1;
}


static void free_kmem_cache_nodes(struct kmem_cache* s){

}
static int alloc_kmem_cache_cpus(struct kmem_cache* s){
    s->cpu_slab = &this_kmem_cache_cpu;
    s->cpu_slab->tid = 0; //arbitrary
    s->cpu_slab->freelist = NULL;
    s->cpu_slab->page = s->cpu_slab->partial = NULL;
    this_kmem_cache_cpu.partial = this_kmem_cache_cpu.page = NULL;
    this_kmem_cache_cpu.freelist = NULL;
    return 1;
}
static inline void inc_slabs_node(struct kmem_cache* s, int node, int objnum);
/*
 * No kmalloc_node yet so do it by hand. We know that this is the first
 * slab on the node for this slabcache. There are no concurrent accesses
 * possible.
 *
 * Note that this function only works on the kmem_cache_node
 * when allocating for the kmem_cache_node. This is used for bootstrapping
 * memory on a fresh node that has no slab structures yet.
 */
static void early_kmem_cache_node_alloc(struct kmem_cache* s,  int node){
    struct slub_page* page;
    struct kmem_cache_node* n;

    page = new_slab(kmem_cache_node_ptr_t, node);
    if (page_to_nid(page) != node){
        cprintf("[SLUB FAULT]SLUB: Unable to allocate memory from "
				"node %d\n", node);
        cprintf("flags: %d\n", page->flags);
    }
    n = kmem_cache_node_ptr_t->node[node];
    //kernel addr + offset
    page->freelist = get_freepointer(kmem_cache_node_ptr_t, page2kva(page->p)); 
    //get the first free pointer
    cprintf("page start: %p, free pointer: %p\n", page->p, page->freelist);
    
    page->inuse = 1; //slab has already used one object
    
    page->frozen = 0; //slab not in s->cpu_slab, however i guess it's not in nodes also
    n = kmem_cache_node_ptr_t->node[node] = page->freelist;
    //initialization of kmem_cache_node n
    list_init(&(kmem_cache_node_ptr_t->node[node]->partial));
    list_init(&(kmem_cache_node_ptr_t->node[node]->full));
    list_add(&(kmem_cache_node_ptr_t->node[node]->partial), &(page->list));
    //init the first one

    //void* start = page2kva(page->p);     
    //get virtual addr of page and do memset
    //void* last = start;
    //cprintf("objnum %d, size %d\n", s->obj_num, s->object_size);
    // for (void* p = start; p < start + PGSIZE*s->order; p+=s->object_size){
    //     ((struct kmem_cache_node*)last)->nr_partial = 0;
    //     ((struct kmem_cache_node*)last)->nr_total = 0;
    //     list_init(&((struct kmem_cache_node*)last)->partial);
    //     list_init(&((struct kmem_cache_node*)last)->full);
	// 	set_freepointer(s, last, p);
	// 	last = p;
    // }


    //init_kmem_cache_node(n);    //initialize node list_entry_t
    //cprintf("ckpt\n");
    //node ++
    inc_slabs_node(kmem_cache_node_ptr_t, node, page->obj_num); 
    cprintf("node[i] %p, node[i]->partial %p\n", kmem_cache_node_ptr_t->node[node]
            ,&(kmem_cache_node_ptr_t->node[node]->partial));
     
    set_page_nid_flag(page, node);
    cprintf("page flag nid set %d\n", page_to_nid(page));
    if(s != kmem_cache_node_ptr_t){
        s->node[node] = kmem_cache_node_ptr_t->node[node];
    }
    //add in the node's partial list 
}

static inline struct Page *alloc_slab_page(int node,uint16_t order);

/*
try to new a slab page
 */
static struct slub_page* new_slab(struct kmem_cache *s, int node){
    struct slub_page *page = NULL;
	void *start;
	void *last;
	void *p;
	int order = s->order;
    cprintf("new slab_page from node %d\n", node);
    page =__pages + now_page;       //!!!
    page->flags = 0;
    page->p = alloc_slab_page(node, order);

    //cprintf("change flags from %d", page->flags);
    page->flags = page->flags | (node << NODE_SHIFT);
    set_page_is_partial(page); // set partial flag
    //cprintf("after change %d, node %d \n", page->flags, node);
    if (unlikely(!page)){
        order = s->min;
        page->p = alloc_slab_page(node, order);
    }

    if(!page->p){
        goto out;
    }

    page->obj_num = s->obj_num;
    
    //order = checkorder(page);  //for Safety
    page->slub_cache = s;

    start = page2kva(page->p);     //get virtual addr of page and do memset
    if(CachePoison(s)){
        memset(start, POISON_INUSE, PGSIZE*s->order);
    }
    
    last = start;
    //cprintf("objnum %d, size %d\n", s->obj_num, s->object_size);
    for (p = start; p < start + PGSIZE*s->order; p+=s->object_size){
        //cprintf("%p, \n", p);
        setup_object(s, page, last);
        //cprintf("%p, \n", p);
		set_freepointer(s, last, p);
        
		last = p;
    }
    // for_each_object(p, s, start, page->obj_num) {
		
	// }
    page->freelist = start;
    cprintf("slub page at %p, free list starts at %p, node %d\n", page->p, start, page_to_nid(page));
    page->frozen = 1; // not sure
    set_page_is_partial(page);
out:
    return page;
}
static inline void inc_slabs_node(struct kmem_cache* s, int node, int objnum){
    cprintf("node: %d n_partial %p\n",node, s->node[node]->nr_partial);
    s->node[node]->nr_partial++;
}
/*
 * Slab allocation and freeing
 */
static inline struct Page *alloc_slab_page(int node,uint16_t order){
    return buddysys_pmm_manager.alloc_pages(order);
}



void check_kmem_cache_init(){
    kmem_cache_init();
}