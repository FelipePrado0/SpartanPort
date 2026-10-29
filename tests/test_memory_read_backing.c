#undef NDEBUG
#include "../src/runtime_memory.c"
#include <assert.h>

int32_t *runtime_errno(void) { abort(); }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t count,const char *name) {
    (void)table; (void)count; (void)name; abort();
}

static uintptr_t base;
static void fixture(void) {
    /* The guest view is inaccessible: a host upload must never touch it. */
    base=(uintptr_t)runtime_low_map(6*PAGE,PROT_NONE);
    assert(base);
    backing_base=malloc(8*PAGE);
    assert(backing_base);
    memset(backing_base,0x19,8*PAGE);
    memset(backing_base+PAGE,0x31,PAGE);
    memset(backing_base+3*PAGE,0x72,PAGE);
    write_lock();
    assert(vma_insert(0,(Vma){base,base+PAGE,KIND_DIRECT,3,0,PAGE})==0);
    assert(vma_insert(1,(Vma){base+2*PAGE,base+3*PAGE,KIND_RESERVED,0,0,0})==0);
    assert(vma_insert(2,(Vma){base+3*PAGE,base+4*PAGE,KIND_FLEXIBLE,0,0,3*PAGE})==0);
    write_unlock();
}

static void sparse_and_protected_reads(void) {
    unsigned char out[6*PAGE+2];
    memset(out,0xcc,sizeof(out));
    runtime_memory_read_backing(base-1,out,6*PAGE+2);
    for (size_t i=0;i<sizeof(out);++i) {
        unsigned char expected=i>=1 && i<=PAGE ? 0x31 :
                               i>=3*PAGE+1 && i<=4*PAGE ? 0x72 : 0;
        assert(out[i]==expected);
    }
    /* Zero-length calls do not dereference either pointer. Invalid wrapping ranges
     * read as zero rather than wrapping back into mapped memory. */
    runtime_memory_read_backing(UINTPTR_MAX,NULL,0);
    memset(out,0xcc,8);
    runtime_memory_read_backing(UINTPTR_MAX-3,out,8);
    for (size_t i=0;i<8;++i) assert(out[i]==0);
    /* Changing the physical backing is visible on the next read. */
    write_lock(); vmas[0].phys=3*PAGE; write_unlock();
    runtime_memory_read_backing(base,out,PAGE);
    for (size_t i=0;i<PAGE;++i) assert(out[i]==0x72);
}

static void *remap(void *unused) {
    (void)unused;
    for (size_t i=0;i<2000;++i) {
        write_lock(); vmas[0].phys=(i%2 ? 1 : 3)*PAGE; write_unlock();
    }
    return NULL;
}

static void concurrent_remaps(void) {
    pthread_t writer;
    assert(!pthread_create(&writer,NULL,remap,NULL));
    for (size_t i=0;i<2000;++i) {
        unsigned char out[PAGE];
        runtime_memory_read_backing(base,out,sizeof(out));
        assert(out[0]==0x31 || out[0]==0x72);
        for (size_t j=1;j<sizeof(out);++j) assert(out[j]==out[0]);
    }
    assert(!pthread_join(writer,NULL));
    write_lock(); vma_count=0; write_unlock();
    unsigned char out[PAGE];
    runtime_memory_read_backing(base,out,sizeof(out));
    for (size_t i=0;i<sizeof(out);++i) assert(!out[i]);
}

static void split_backing_offsets(void) {
    memset(backing_base+5*PAGE,0x51,PAGE);
    memset(backing_base+6*PAGE,0x62,PAGE);
    for (int kind=KIND_RESERVED;kind<=KIND_FLEXIBLE;++kind) {
        write_lock();
        vma_count=0;
        assert(!vma_insert(0,(Vma){base,base+2*PAGE,kind,3,0,5*PAGE}));
        /* mprotect and partial unmap use carve/split_at. The remaining right
         * fragment must still address its own physical page. */
        assert(!split_at(base+PAGE));
        write_unlock();
        unsigned char out[PAGE];
        runtime_memory_read_backing(base+PAGE,out,sizeof(out));
        for (size_t i=0;i<sizeof(out);++i)
            assert(out[i]==(kind==KIND_RESERVED ? 0 : 0x62));
        write_lock();
        vma_erase(0,1);
        write_unlock();
        runtime_memory_read_backing(base+PAGE,out,sizeof(out));
        for (size_t i=0;i<sizeof(out);++i)
            assert(out[i]==(kind==KIND_RESERVED ? 0 : 0x62));
    }
}

int main(void) {
    fixture(); sparse_and_protected_reads(); concurrent_remaps(); split_backing_offsets();
    puts("PASS: protected backing reads, sparse zero fill, remap and concurrency");
    return 0;
}
