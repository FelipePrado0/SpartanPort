/* Read-only package I/O through tools/pkg_bridge.cs. Included by runtime_file.c. */
#ifdef _WIN32
static pthread_mutex_t pkg_lock=PTHREAD_MUTEX_INITIALIZER;
static HANDLE pkg_pipe=INVALID_HANDLE_VALUE;
static const char *pkg_path(const char *guest) {
    if (!getenv("GOW3_PKG_PIPE") || !guest) return NULL;
    if (*guest!='/') return guest;
    if (!strncmp(guest,"/app0",5) && (!guest[5] || guest[5]=='/')) return guest+5+(guest[5]=='/');
    if (!strncmp(guest,"/hostapp",8) && (!guest[8] || guest[8]=='/')) return guest+8+(guest[8]=='/');
    return NULL;
}
static int pkg_transfer(void *buffer,DWORD size,int write_data) {
    unsigned char *p=buffer;
    while (size) {
        DWORD n=0;
        BOOL ok=write_data ? WriteFile(pkg_pipe,p,size,&n,NULL) : ReadFile(pkg_pipe,p,size,&n,NULL);
        if (!ok || !n) return EIO;
        p+=n; size-=n;
    }
    return 0;
}
static int pkg_request(uint32_t op,const char *path,int64_t offset,uint32_t count,unsigned char **reply,uint32_t *length) {
    size_t path_len=strlen(path);
    if (path_len>4096 || offset<0) return EINVAL;
    pthread_mutex_lock(&pkg_lock);
    int e=0;
    if (pkg_pipe==INVALID_HANDLE_VALUE) {
        char name[256];
        const char *pipe=getenv("GOW3_PKG_PIPE");
        if (!pipe || snprintf(name,sizeof(name),"\\\\.\\pipe\\%s",pipe)>=(int)sizeof(name)) e=EINVAL;
        else {
            WaitNamedPipeA(name,10000);
            pkg_pipe=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
            if (pkg_pipe==INVALID_HANDLE_VALUE) e=EIO;
        }
    }
    unsigned char header[20];
    uint32_t n=(uint32_t)path_len;
    memcpy(header,&op,4); memcpy(header+4,&n,4); memcpy(header+8,&offset,8); memcpy(header+16,&count,4);
    int32_t status=0;
    uint32_t size=0;
    if (!e) e=pkg_transfer(header,20,1);
    if (!e) e=pkg_transfer((void *)path,n,1);
    if (!e) e=pkg_transfer(&status,4,0);
    if (!e) e=pkg_transfer(&size,4,0);
    if (!e && size>16*1024*1024) e=EIO;
    unsigned char *data=NULL;
    if (!e && size) {
        data=malloc(size);
        if (!data) e=ENOMEM;
        else e=pkg_transfer(data,size,0);
    }
    if (e && pkg_pipe!=INVALID_HANDLE_VALUE) { CloseHandle(pkg_pipe); pkg_pipe=INVALID_HANDLE_VALUE; }
    pthread_mutex_unlock(&pkg_lock);
    if (e || status) { free(data); return e ? e : status; }
    *reply=data; *length=size;
    return 0;
}
static int pkg_info(const char *path,int *type,int64_t *size) {
    unsigned char *data=NULL; uint32_t length=0;
    int e=pkg_request(1,path,0,0,&data,&length);
    if (!e) {
        if (length!=12) e=EIO;
        else { memcpy(type,data,4); memcpy(size,data+4,8); }
    }
    free(data); return e;
}
static void pkg_guest_stat(int type,int64_t size,GuestStat *s) {
    memset(s,0,sizeof(*s)); s->mode=(uint16_t)(type==4 ? 0040555 : 0100444);
    s->nlink=1; s->size=size; s->blksize=65536; s->blocks=(size+511)/512;
}
static Listing *pkg_listing(const char *path,int *error) {
    unsigned char *data=NULL; uint32_t length=0,count=0;
    *error=pkg_request(3,path,0,0,&data,&length);
    if (*error) return NULL;
    if (length<4) { *error=EIO; free(data); return NULL; }
    memcpy(&count,data,4);
    if (count>length/8) { *error=EIO; free(data); return NULL; }
    Listing *l=calloc(1,sizeof(*l));
    if (!l) { *error=ENOMEM; free(data); return NULL; }
    l->offsets=calloc((size_t)count+1,sizeof(size_t));
    l->types=calloc((size_t)count+1,1); l->names=malloc((size_t)length+count+1);
    if (!l->offsets || !l->types || !l->names) *error=ENOMEM;
    size_t at=4,used=0;
    for (uint32_t i=0; !*error && i<count; ++i) {
        uint32_t type,n;
        if (at+8>length) { *error=EIO; break; }
        memcpy(&type,data+at,4); memcpy(&n,data+at+4,4); at+=8;
        if (n>255 || at+n>length) { *error=EIO; break; }
        l->offsets[i]=used; l->types[i]=(unsigned char)type;
        memcpy(l->names+used,data+at,n); l->names[used+n]=0;
        used+=n+1; at+=n; ++l->count;
    }
    free(data);
    if (*error) { free(l->offsets); free(l->types); free(l->names); free(l); return NULL; }
    return l;
}
#endif
