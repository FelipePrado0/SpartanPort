/* Load: file trace, progress state and stall decision (docs/specs/2026-10-29-load-file-trace.md,
 * docs/specs/2026-10-29-load-stall-watchdog.md) on real files in a temporary game folder. */
#include "../src/runtime_file.c"
#include <assert.h>
static int32_t guest_errno;
int32_t *runtime_errno(void) { return &guest_errno; }
int32_t runtime_guest_errno(int e) { return e; }
uint64_t runtime_wait_begin(void) { return 0; }
void runtime_wait_end(const char *what,uint64_t start,const char *detail,long long arg) { (void)what; (void)start; (void)detail; (void)arg; }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t n,const char *name) {
    for (size_t i=0;i<n;++i) if (!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
const char *runtime_thread_current_name(void) { return "loader"; }
unsigned long runtime_thread_host_id(void) { return 4242; }
static int snapshots;
void runtime_thread_load_snapshot(FILE *out,int sample) { __atomic_add_fetch(&snapshots,1,__ATOMIC_RELEASE); fprintf(out,"Load: thread sample=%d tid=4242 name='loader' cpu_ms=0\n",sample); }

static char text[1<<16];
/* Everything written to the trace stream since the last call. */
static const char *take(void) {
    fflush(load_out);
    long n=ftell(load_out); assert(n>=0 && (size_t)n<sizeof(text));
    rewind(load_out);
    size_t got=fread(text,1,(size_t)n,load_out); text[got]=0;
    rewind(load_out); assert(!ftruncate(fileno(load_out),0));
    return text;
}
/* A line holding every given fragment. */
static int has_line(const char *all,const char *a,const char *b,const char *c) {
    for (const char *line=all;*line;) {
        const char *end=strchr(line,'\n'); if (!end) end=line+strlen(line);
        char one[1024]; size_t n=(size_t)(end-line); if (n>=sizeof(one)) n=sizeof(one)-1;
        memcpy(one,line,n); one[n]=0;
        if (strstr(one,a) && (!b || strstr(one,b)) && (!c || strstr(one,c))) return 1;
        line=*end ? end+1 : end;
    }
    return 0;
}
static int count(const char *all,const char *what) { int n=0; for (const char *p=all;(p=strstr(p,what));p+=strlen(what)) ++n; return n; }
static void write_file(const char *path,size_t size) {
    FILE *f=fopen(path,"wb"); assert(f);
    for (size_t i=0;i<size;++i) assert(fputc((int)(i&255),f)!=EOF);
    assert(!fclose(f));
}
int main(void) {
    unsetenv("SPARTAN_FILE_TRACE"); unsetenv("SPARTAN_FILE_TRACE_LIMIT"); unsetenv("SPARTAN_LOAD_WATCHDOG");
    load_watchdog_autostart=0;
    load_out=tmpfile(); assert(load_out);
    char root[]="/tmp/spartanport-load-trace-XXXXXX";
    assert(mkdtemp(root));
    char game[512],user[512],path[600];
    snprintf(game,sizeof(game),"%s/game",root); snprintf(user,sizeof(user),"%s/user",root);
    assert(!mkdir(game,0755));
    snprintf(path,sizeof(path),"%s/AREA.WAD",game); write_file(path,10000);
    snprintf(path,sizeof(path),"%s/other.bin",game); write_file(path,64);
    snprintf(path,sizeof(path),"%s/dir.wad",game); assert(!mkdir(path,0755));
    runtime_file_configure(game,user);
    char buffer[8192];

    /* Off by default: no line, no state, no watchdog. */
    int fd=(int)do_open("/app0/AREA.WAD",0,0); assert(fd>=3);
    assert(do_pread(fd,buffer,4096,0)==4096);
    assert(!do_close(fd));
    assert(!load_mode && !load_watchdog_seconds && !*take());

    /* WAD mode: begin/end of every operation, full, partial, EOF and failed reads. */
    load_configure("1","0",NULL);
    assert(has_line(take(),"Load: trace on (mode=wad, limit=0)",NULL,NULL));
    fd=(int)do_open("/app0/AREA.WAD",0,0); assert(fd>=3);
    assert(do_pread(fd,buffer,4096,0)==4096);
    assert(do_pread(fd,buffer,4096,4096)==4096);
    assert(do_pread(fd,buffer,4096,8000)==2000);   /* partial: 2000 bytes left */
    assert(do_pread(fd,buffer,100,10000)==0);      /* EOF */
    assert(do_read(fd,buffer,100)==100);           /* read uses the descriptor's offset (0) */
    const char *log=take();
    assert(has_line(log,"begin open","path=/app0/AREA.WAD","thread='loader'"));
    assert(has_line(log,"end open","size=10000","errno=0"));
    assert(has_line(log,"begin pread","off=8000 req=4096","tid=4242"));
    /* done = bytes read contiguously from offset 0, against the file size. */
    assert(has_line(log,"end pread","result=4096 errno=0","done=4096/10000"));
    assert(has_line(log,"end pread","result=2000 errno=0","done=10000/10000"));
    assert(has_line(log,"begin pread","off=10000 req=100",NULL));
    assert(has_line(log,"end pread","result=0 errno=0",NULL));
    assert(has_line(log,"begin read","off=0 req=100",NULL));
    assert(has_line(log,"end read","result=100",NULL));
    assert(count(log," begin ")==6 && count(log," end ")==6);
    assert(has_line(log,"t_us=","us=",NULL));
    /* State: the open WAD with its distinct bytes read (0..10000 covered once). */
    runtime_file_load_state(load_out);
    log=take();
    assert(has_line(log,"Load: state","last progress",NULL));
    assert(has_line(log,"open fd=","path=/app0/AREA.WAD","read=10000/10000"));
    /* Non-WAD files are not traced in WAD mode. */
    int other=(int)do_open("/app0/other.bin",0,0); assert(other>=3);
    assert(do_read(other,buffer,64)==64); assert(!do_close(other));
    assert(!*take());
    /* A failed read: same result to the caller, errno logged. */
    int dir=(int)do_open("/app0/dir.wad",0,0); assert(dir>=3);
    assert(do_read(dir,buffer,10)==-EISDIR);
    log=take();
    assert(has_line(log,"end read","result=-21","errno=21"));
    assert(!do_close(dir)); take();

    /* errno survives a trace stream that cannot be written. */
    FILE *keep=load_out;
    load_out=fopen("/dev/full","w"); assert(load_out);
    setvbuf(load_out,NULL,_IONBF,0);
    errno=4242;
    assert(do_pread(fd,buffer,16,0)==16);
    assert(errno==4242);
    dir=(int)do_open("/app0/dir.wad",0,0); assert(dir>=3);
    assert(do_read(dir,buffer,10)==-EISDIR && errno==EISDIR);
    assert(!do_close(dir));
    fclose(load_out); load_out=keep;

    /* Close ends the file's state. */
    assert(!do_close(fd));
    log=take();
    assert(has_line(log,"begin close","path=/app0/AREA.WAD",NULL) && has_line(log,"end close","result=0",NULL));
    runtime_file_load_state(load_out);
    assert(!has_line(take(),"open fd=",NULL,NULL));

    /* Limit: three operation lines, one limit line, state still kept. */
    load_configure("1","3",NULL);
    take();
    fd=(int)do_open("/app0/AREA.WAD",0,0); assert(do_pread(fd,buffer,10,0)==10); assert(!do_close(fd));
    log=take();
    assert(count(log,"Load: #")==3 && count(log,"Load: trace limit 3 reached")==1);
    fd=(int)do_open("/app0/AREA.WAD",0,0);
    log=take();
    assert(!*log);
    runtime_file_load_state(load_out);
    assert(has_line(take(),"open fd=","path=/app0/AREA.WAD",NULL));
    assert(!do_close(fd)); take();

    /* "all" mode traces every game file. */
    load_configure("all","0",NULL); take();
    other=(int)do_open("/app0/other.bin",0,0); assert(!do_close(other));
    assert(has_line(take(),"begin open","path=/app0/other.bin",NULL));

    /* Stall decision: tracking without lines, simulated clock. */
    load_configure(NULL,NULL,"5");
    assert(load_mode==LOAD_WAD && !load_lines && load_watchdog_seconds==5);
    take();
    fd=(int)do_open("/app0/AREA.WAD",0,0); assert(fd>=3);
    uint64_t t=load_last_us;
    assert(!load_stall_due(t+60000000));            /* opened but never read: not a load in progress */
    assert(do_pread(fd,buffer,100,0)==100);
    assert(!*take());                               /* watchdog alone writes no operation lines */
    t=load_last_us;
    assert(!load_stall_due(t+4999999));
    assert(load_stall_due(t+5000000));
    assert(!load_stall_due(t+9000000));             /* once per stall */
    assert(do_pread(fd,buffer,100,100)==100);       /* progress re-arms */
    t=load_last_us;
    assert(!load_stall_due(t+1000000) && load_stall_due(t+5000000));
    /* An operation still running counts as a load in progress, and the state names it. */
    LoadOp op=load_begin("pread",fd,NULL,200,4096);
    assert(op.id);
    runtime_file_load_state(load_out);
    assert(has_line(take(),"in flight","pread","path=/app0/AREA.WAD"));
    load_end(&op,4096);
    assert(!do_close(fd));
    t=load_last_us;
    assert(!load_stall_due(t+60000000));            /* nothing open, nothing running */
    /* The report: STALL line, state, two thread samples. */
    load_stall_report(load_out,t+6000000);
    log=take();
    assert(has_line(log,"Load: STALL no file progress for 6 s (watchdog 5 s)",NULL,NULL));
    assert(has_line(log,"thread sample=1",NULL,NULL) && has_line(log,"thread sample=2",NULL,NULL) && snapshots==2);

    /* The real watchdog thread: started by the first tracked operation, reports a 1 s stall. */
    load_configure(NULL,NULL,"1"); take();
    load_watchdog_autostart=1;
    snapshots=0;
    fd=(int)do_open("/app0/AREA.WAD",0,0); assert(fd>=3);
    assert(do_pread(fd,buffer,100,0)==100);
    for (int i=0;i<80 && __atomic_load_n(&snapshots,__ATOMIC_ACQUIRE)<2;++i) { struct timespec tick={0,100000000}; nanosleep(&tick,NULL); }
    assert(__atomic_load_n(&snapshots,__ATOMIC_ACQUIRE)==2);
    struct timespec settle={0,100000000}; nanosleep(&settle,NULL);
    assert(has_line(take(),"Load: STALL no file progress for 1 s (watchdog 1 s)",NULL,NULL));
    assert(!do_close(fd));
    load_watchdog_autostart=0;
    /* Watchdog values outside 1..3600 leave it off. */
    load_configure(NULL,NULL,"0"); assert(!load_watchdog_seconds && !load_mode);
    load_configure(NULL,NULL,"abc"); assert(!load_watchdog_seconds && !load_mode);
    load_configure("yes",NULL,NULL); assert(!load_mode);
    puts("PASS: Load: trace off by default, begin/end of open/read/pread/close, partial/EOF/error reads, errno kept, WAD filter, limit, state, stall decision");
    return 0;
}
