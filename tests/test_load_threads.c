/* Load: thread samples for the stall watchdog (docs/specs/2026-10-29-load-stall-watchdog.md):
 * a thread blocked in read() shows up with its id, name, CPU time, syscall and pc (Linux). */
#include "../src/runtime_thread.c"
#include <assert.h>
void *runtime_low_map(size_t size,int prot) { (void)prot; return calloc(1,size); }
void *runtime_low_alloc(size_t size) { return calloc(1,size); }
void runtime_low_free(void *pointer) { free(pointer); }
void runtime_thread_keys_cleanup(void) {}

static int pipe_ends[2];
static unsigned long blocked_tid;
static void *blocked(void *arg) {
    (void)arg;
    pthread_setname_np(pthread_self(),"fios-blocked");
    __atomic_store_n(&blocked_tid,runtime_thread_host_id(),__ATOMIC_RELEASE);
    char byte;
    ssize_t n=read(pipe_ends[0],&byte,1); /* blocks until the test writes */
    (void)n;
    return NULL;
}
static void image_marker(void) {}
int main(void) {
    /* A thread without a guest record is "host", and asking does not create a record. */
    assert(!strcmp(runtime_thread_current_name(),"host"));
    assert(!current);
    assert(runtime_thread_host_id()==(unsigned long)syscall(SYS_gettid));
    runtime_thread_attach_host("loader");
    assert(!strcmp(runtime_thread_current_name(),"loader"));

    assert(!pipe(pipe_ends));
    pthread_t thread;
    assert(!pthread_create(&thread,NULL,blocked,NULL));
    while (!__atomic_load_n(&blocked_tid,__ATOMIC_ACQUIRE)) sched_yield();
    struct timespec settle={0,100000000}; nanosleep(&settle,NULL);
    /* This test's code as the "image": the blocked thread's pc is in libc, not in it. */
    runtime_thread_set_image((uintptr_t)image_marker,4096);

    FILE *out=tmpfile(); assert(out);
    errno=4242;
    runtime_thread_load_snapshot(out,2);
    assert(errno==4242);
    fflush(out); rewind(out);
    char line[512], want[64];
    snprintf(want,sizeof(want),"tid=%lu name='fios-blocked'",blocked_tid);
    int found=0, lines=0;
    while (fgets(line,sizeof(line),out)) {
        ++lines;
        assert(!strncmp(line,"Load: thread sample=2 tid=",26));
        if (strstr(line,want)) {
            found=1;
            assert(strstr(line,"cpu_ms="));
            /* read(2) is syscall 0 on x86-64; pc and sp are present and non-zero. */
            assert(strstr(line," syscall=0 "));
            assert(strstr(line," pc=0x") && !strstr(line," pc=0x0 "));
            assert(strstr(line," sp=0x") && !strstr(line," sp=0x0 "));
            assert(!strstr(line,"image+"));
        }
    }
    assert(found && lines>=2);
    assert(write(pipe_ends[1],"x",1)==1);
    assert(!pthread_join(thread,NULL));
    puts("PASS: Load: thread name/id without a record, thread samples with id, name, CPU, syscall, pc and sp");
    return 0;
}
