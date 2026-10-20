/* libSceAudioOut on SDL3 audio streams. sceAudioOutOutput returns once per
 * buffer period on a steady clock, like PS4 hardware: FMOD's output thread
 * reads 256-frame slots out of its mixer's 512-frame ring, and returning in
 * bursts (whenever the device pulls a quantum) lets it overtake the mixer and
 * play stale halves - a click every 10.7 ms. The lowest SDL queue level of
 * each window is steered to two buffers (silence prefill on underrun, ±3%
 * cadence corrections), which also absorbs device clock drift. Without an audio device (headless runs, SPARTAN_AUDIO=none)
 * ports only follow the clock. */
#define _GNU_SOURCE
#include "runtime.h"
#include "gpu/spartanportgpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <SDL3/SDL.h>

#define PORTS 25
#define ERR_NOT_OPENED ((int32_t)0x80260001)
#define ERR_INVALID_PORT ((int32_t)0x80260003)
#define ERR_INVALID_POINTER ((int32_t)0x80260004)
#define ERR_PORT_FULL ((int32_t)0x80260005)
#define ERR_INVALID_SIZE ((int32_t)0x80260006)
#define ERR_INVALID_FORMAT ((int32_t)0x80260007)
#define ERR_INVALID_FREQ ((int32_t)0x80260008)
#define ERR_INVALID_VOLUME ((int32_t)0x80260009)
#define ERR_INVALID_TYPE ((int32_t)0x8026000A)
#define ERR_ALREADY_INIT ((int32_t)0x8026000E)
#define ERR_NOT_INIT ((int32_t)0x8026000F)
#define VOLUME_0DB 32768

typedef struct {
    int used, type, channels, is_float, frames, sample_bytes, std_layout;
    int out_channels; /* what is sent to the device: `channels`, or 2 / 6 for a 7.1 port folded down */
    float peak[8]; unsigned peak_buffers, peak_reports; /* levels the game sends, for the log (track_peaks) */
    int state_logged;
    int32_t volume[8];
    SDL_AudioStream *stream;
    uint64_t next_deadline_ns; /* next return of sceAudioOutOutput */
    int64_t adjust_ns; int window_min, window_count; /* queue level control */
    uint64_t last_output_us;
    pthread_mutex_t lock;
    FILE *dump;                     /* SPARTAN_AUDIO_DUMP: raw converted PCM per port */
    int stats;
    uint64_t stat_start_ns, stat_last_ns, stat_max_gap_ns; /* SPARTAN_AUDIO_STATS */
    unsigned stat_starved, stat_buffers; int stat_min_queued;
} Port;
typedef struct { uint16_t output; uint8_t channel, reserved; int16_t volume; uint16_t reroute; uint64_t flag, reserved64[2]; } PortState;
_Static_assert(sizeof(PortState)==32,"AudioOut port state layout");

static pthread_mutex_t table_lock=PTHREAD_MUTEX_INITIALIZER;
static Port ports[PORTS];
static int initialized, sdl_ready=-1;
static size_t buffers_out, ports_opened;

static uint64_t now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec; }
static void sleep_until(uint64_t deadline) {
#ifdef _WIN32
    /* winpthreads' clock_nanosleep only takes CLOCK_REALTIME (EINVAL otherwise). */
    for (uint64_t now=now_ns(); now<deadline; now=now_ns()) compat_sleep_ns(deadline-now);
#else
    struct timespec t={(time_t)(deadline/1000000000u),(long)(deadline%1000000000u)};
    while (clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&t,NULL)) {}
#endif
}
static int sdl_audio(void) {
    if (sdl_ready<0) {
        const char *mode=getenv("SPARTAN_AUDIO");
        sdl_ready = (!mode || strcmp(mode,"none")) && SDL_InitSubSystem(SDL_INIT_AUDIO);
        printf("Runtime: audio backend %s\n", sdl_ready ? SDL_GetCurrentAudioDriver() : "timer (silent)");
    }
    return sdl_ready;
}
static int port_range(int type, int *first, int *last) {
    switch (type) {
    case 0: *first=0; *last=7; return 1;      /* main */
    case 1: *first=8; *last=8; return 1;      /* bgm */
    case 2: *first=9; *last=12; return 1;     /* voice */
    case 3: *first=13; *last=16; return 1;    /* personal */
    case 4: *first=17; *last=20; return 1;    /* pad speaker */
    case 126: *first=21; *last=22; return 1;  /* audio3d */
    case 127: *first=23; *last=24; return 1;  /* aux */
    default: return 0;
    }
}
#ifndef PEAK_INTERVAL
#define PEAK_INTERVAL 562 /* buffers (about 3 s) between two level reports */
#endif
#define PEAK_REPORTS 10   /* reports per port: the first half minute of the game */

/* Speaker layouts a 7.1 port can be sent to (spartanport.ini audio_channels, menu "Speakers"). */
enum { OUT_STEREO=2, OUT_51=6, OUT_71=8 };
enum { MODE_AUTO=0, MODE_STEREO=1, MODE_51=2, MODE_71=3 }; /* spartanportgpu_audio_channels() */

/* The layout a 7.1 port is sent in. `mode` is the setting, `device` the playback device's channel
   count (0: unknown, the stream is left to SDL's own conversion). Automatic follows the device:
   7.1 or more stays 7.1, 5.1 / 6.1 is folded to 5.1, anything smaller to stereo. */
static int output_channels(int mode, int device) {
    switch (mode) {
    case MODE_STEREO: return OUT_STEREO;
    case MODE_51: return OUT_51;
    case MODE_71: return OUT_71;
    default: return device<=0 || device>=8 ? OUT_71 : device>=6 ? OUT_51 : OUT_STEREO;
    }
}
static const char *layout_name(int channels) { return channels==OUT_STEREO ? "stereo" : channels==OUT_51 ? "5.1" : "7.1"; }

/* The menu's master volume, 0 to 200 percent, as a gain. */
static float master_gain(void) {
    int percent=spartanportgpu_audio_volume_percent();
    return (float)(percent<0 ? 0 : percent>200 ? 200 : percent)/100.0f;
}

/* Keeps a folded or boosted mix inside full scale without the harsh distortion of a hard clip:
   untouched up to 0.8, then bent towards 1.0 (slope 1 at the knee, never above 1). */
static float soft_limit(float x) {
    const float knee=0.8f, room=1.0f-knee;
    float a=x<0 ? -x : x;
    if (a<=knee) return x;
    if (!(a<1e6f)) return a!=a ? 0.0f : x<0 ? -1.0f : 1.0f; /* NaN and huge values */
    float y=knee+(a-knee)/(1.0f+(a-knee)/room);
    if (y>1.0f) y=1.0f;
    return x<0 ? -y : y;
}

/* SDL's own 7.1 conversions keep only part of the front channels (to stereo ~0.21, -13.5 dB), so
   the game was far too quiet. Folds one SDL-ordered 7.1 frame (FL FR C LFE BL BR SL SR) to
   `out_channels`, in the layout SDL expects:
   - stereo: fronts at full level, center and surrounds at -3 dB, LFE dropped;
   - 5.1 (FL FR C LFE BL BR, the last two being the surrounds): front, center and LFE as they are,
     each surround takes the side channel plus the back one at -3 dB.
   The result can pass full scale; the caller limits it. */
static void fold_frame(const float s[8], float *out, int out_channels) {
    const float k=0.70710678f;
    if (out_channels==OUT_STEREO) {
        out[0]=s[0]+k*(s[2]+s[4]+s[6]); out[1]=s[1]+k*(s[2]+s[5]+s[7]);
    } else {
        out[0]=s[0]; out[1]=s[1]; out[2]=s[2]; out[3]=s[3];
        out[4]=s[6]+k*s[4]; out[5]=s[7]+k*s[5];
    }
}

static float load_sample(const void *data, size_t index, int is_float) {
    if (is_float) { float v; memcpy(&v,(const char *)data+index*4,4); return v; }
    int16_t v; memcpy(&v,(const char *)data+index*2,2); return (float)v/32768.0f;
}
static void store_sample(void *data, size_t index, float v, int is_float) {
    if (is_float) { memcpy((char *)data+index*4,&v,4); return; }
    float t=v*32768.0f;
    t = t>32767.0f ? 32767.0f : t<-32768.0f ? -32768.0f : t;
    int16_t o=(int16_t)(t<0 ? t-0.5f : t+0.5f);
    memcpy((char *)data+index*2,&o,2);
}
/* Remembers the loudest sample the game sends on each channel of an 8ch port, in the PS4's order.
   Returns 1 once PEAK_INTERVAL buffers are in and the levels are to be logged (PEAK_REPORTS times at
   most): shows in the log whether the game fills the surround channels at all. */
static int track_peaks(Port *p, const void *data) {
    if (p->channels!=8 || p->peak_reports>=PEAK_REPORTS) return 0;
    for (size_t f=0;f<(size_t)p->frames;++f) for (int c=0;c<8;++c) {
        float v=load_sample(data,f*8+(size_t)c,p->is_float);
        if (v<0) v=-v;
        if (v>p->peak[c]) p->peak[c]=v;
    }
    return ++p->peak_buffers>=PEAK_INTERVAL;
}
static void report_peaks(Port *p, int id) {
    static const char *const ps4[8]={"L","R","C","LFE","SL","SR","BL","BR"}, *const sdl[8]={"L","R","C","LFE","BL","BR","SL","SR"};
    const char *const *names=p->std_layout ? sdl : ps4;
    char line[256];
    int n=snprintf(line,sizeof(line),"Runtime: audio levels port %d (the game's mix, peak per channel):",id);
    for (int c=0;c<8 && n>0 && n<(int)sizeof(line);++c) n+=snprintf(line+n,sizeof(line)-(size_t)n," %s %.2f",names[c],(double)p->peak[c]);
    puts(line);
    memset(p->peak,0,sizeof(p->peak));
    p->peak_buffers=0; ++p->peak_reports;
}

/* One period of the game's samples to what the device stream takes: the port's per-channel
   volumes times `master`, PS4 8ch (L R C LFE SL SR BL BR) to SDL 7.1 order, then folded to
   `out_channels` when that is fewer. Folded or boosted mixes go through the soft limiter; a
   port passed on as it is (volume 100%) is only scaled by its own volumes, as before. `out` holds
   frames * out_channels samples of the port's format. */
static void shape_buffer(const Port *p, const void *data, void *out, float master) {
    static const int remap[8]={0,1,2,3,6,7,4,5};
    float gain[8];
    for (int c=0;c<p->channels;++c) gain[c]=(float)p->volume[c]/VOLUME_0DB*master;
    const int fold=p->channels==8 && p->out_channels<8, limit=fold || master>1.0f;
    const int n=fold ? p->out_channels : p->channels;
    for (size_t f=0;f<(size_t)p->frames;++f) {
        float s[8]={0}, o[8];
        for (int c=0;c<p->channels;++c) {
            int target=p->channels==8 && !p->std_layout ? remap[c] : c;
            s[target]=load_sample(data,f*(size_t)p->channels+(size_t)c,p->is_float)*gain[c];
        }
        const float *src=s;
        if (fold) { fold_frame(s,o,p->out_channels); src=o; }
        for (int c=0;c<n;++c) store_sample(out,f*(size_t)n+(size_t)c,limit ? soft_limit(src[c]) : src[c],p->is_float);
    }
}
static Port *port_of(int32_t handle, int32_t *error) {
    int id=handle & 0xff;
    if ((handle & 0x3f000000)!=0x20000000 || id>=PORTS) { *error=ERR_INVALID_PORT; return NULL; }
    if (!ports[id].used) { *error=ERR_NOT_OPENED; return NULL; }
    return &ports[id];
}

static ABI int32_t audio_init(void) {
    pthread_mutex_lock(&table_lock);
    int32_t r=initialized ? ERR_ALREADY_INIT : 0;
    initialized=1;
    pthread_mutex_unlock(&table_lock);
    return r;
}
#ifdef TEST_AUDIO_HOOK
static SDL_AudioStream *(*test_open_stream)(SDL_AudioDeviceID, const SDL_AudioSpec *, SDL_AudioStreamCallback, void *) = NULL;
#define OPEN_AUDIO_STREAM(dev, spec, cb, user) (test_open_stream ? test_open_stream(dev, spec, cb, user) : SDL_OpenAudioDeviceStream(dev, spec, cb, user))
#else
#define OPEN_AUDIO_STREAM(dev, spec, cb, user) SDL_OpenAudioDeviceStream(dev, spec, cb, user)
#endif

/* Channels of the default playback device in the current backend; 0 when SDL cannot tell. */
static int default_device_channels(void) {
    SDL_AudioSpec device; int device_frames;
    return SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&device,&device_frames) ? device.channels : 0;
}

#ifdef _WIN32
static void try_audio_fallback(Port *p, int id) {
    const char *driver = SDL_GetCurrentAudioDriver();
    /* When WASAPI fails on Windows and the user hasn't forced a driver, try DirectSound. */
    if (!driver || strcmp(driver, "wasapi") || getenv("SDL_AUDIO_DRIVER")) return;

    /* Only switch backend if no other port currently has an active stream. */
    for (int i = 0; i < PORTS; ++i) {
        if (ports[i].used && ports[i].stream) return;
    }

    int has_ds = 0, n = SDL_GetNumAudioDrivers();
    for (int i = 0; i < n; ++i) {
        const char *d = SDL_GetAudioDriver(i);
        if (d && !strcmp(d, "directsound")) { has_ds = 1; break; }
    }
    if (!has_ds) return;

    fprintf(stderr, "Runtime: SDL audio stream failed on wasapi (%s); trying directsound fallback for port %d\n", SDL_GetError(), id);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "directsound", SDL_HINT_OVERRIDE);
    sdl_ready = SDL_InitSubSystem(SDL_INIT_AUDIO);
    const char *new_driver = sdl_ready ? SDL_GetCurrentAudioDriver() : NULL;
    if (sdl_ready && new_driver && !strcmp(new_driver, "directsound")) {
        printf("Runtime: audio backend %s (fallback from wasapi)\n", new_driver);
        /* The new driver may have another channel count: lay the mix out for it. */
        p->out_channels = p->channels == 8 ? output_channels(spartanportgpu_audio_channels(), default_device_channels()) : p->channels;
        SDL_AudioSpec spec = {p->is_float ? SDL_AUDIO_F32 : SDL_AUDIO_S16, p->out_channels, 48000};
        p->stream = OPEN_AUDIO_STREAM(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    } else {
        fprintf(stderr, "Runtime: failed to initialize directsound audio backend (current: %s, error: %s) for port %d\n",
                new_driver ? new_driver : "none", SDL_GetError(), id);
    }
}
#endif

static ABI int32_t audio_open(int32_t user, int32_t type, int32_t index, uint32_t length, uint32_t freq, uint32_t param) {
    (void)user; (void)index;
    if (!initialized) return ERR_NOT_INIT;
    if (!length || length>2048 || (length & 0xff)) return ERR_INVALID_SIZE;
    if (freq!=48000) return ERR_INVALID_FREQ;
    uint32_t format=param & 0xff;
    if (format>7) return ERR_INVALID_FORMAT;
    int first, last;
    if (!port_range(type,&first,&last)) return ERR_INVALID_TYPE;
    static const int channels[8]={1,2,8,1,2,8,8,8};
    pthread_mutex_lock(&table_lock);
    int id=-1;
    for (int i=first;i<=last;++i) if (!ports[i].used) { id=i; break; }
    if (id<0) { pthread_mutex_unlock(&table_lock); return ERR_PORT_FULL; }
    Port *p=&ports[id];
    memset(p,0,sizeof(*p));
    pthread_mutex_init(&p->lock,NULL);
    p->used=1; p->type=type; p->channels=channels[format]; p->is_float=format>=3 && format!=6;
    p->sample_bytes=p->is_float ? 4 : 2; p->frames=(int)length; p->std_layout=format>=6;
    for (int c=0;c<8;++c) p->volume[c]=VOLUME_0DB;
    int device_channels=sdl_audio() ? default_device_channels() : 0;
    p->out_channels=p->channels==8 ? output_channels(spartanportgpu_audio_channels(),device_channels) : p->channels;
    if (sdl_audio()) {
        SDL_AudioSpec spec={p->is_float ? SDL_AUDIO_F32 : SDL_AUDIO_S16, p->out_channels, 48000};
        p->stream=OPEN_AUDIO_STREAM(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);
#ifdef _WIN32
        if (!p->stream) { try_audio_fallback(p, id); device_channels=default_device_channels(); }
#endif
        if (p->stream) SDL_ResumeAudioStreamDevice(p->stream);
        else fprintf(stderr,"Runtime: SDL audio stream failed (%s); port %d uses the timer sink\n",SDL_GetError(),id);
    }
    p->stats=getenv("SPARTAN_AUDIO_STATS")!=NULL;
    const char *dump=getenv("SPARTAN_AUDIO_DUMP");
    if (dump) {
        char path[4096];
        snprintf(path,sizeof(path),"%s.port%d.%dch.%s",dump,id,p->out_channels,p->is_float ? "f32" : "s16");
        p->dump=fopen(path,"wb");
    }
    ++ports_opened;
    pthread_mutex_unlock(&table_lock);
    char layout[64]="";
    if (p->channels==8) snprintf(layout,sizeof(layout),", %s to %s (device %d ch)",
                                 p->out_channels<8 ? "downmixed" : "sent",layout_name(p->out_channels),device_channels);
    printf("Runtime: audio port %d opened (type %d, %d ch, %s, %u frames%s)\n",id,type,p->channels,p->is_float ? "float" : "s16",length,layout);
    return (type<<16) | id | 0x20000000;
}
static ABI int32_t audio_close(int32_t handle) {
    pthread_mutex_lock(&table_lock);
    int32_t error=0;
    Port *p=port_of(handle,&error);
    if (p) {
        pthread_mutex_lock(&p->lock);
        if (p->stream) SDL_DestroyAudioStream(p->stream);
        if (p->dump) fclose(p->dump);
        p->dump=NULL;
        p->stream=NULL; p->used=0;
        pthread_mutex_unlock(&p->lock);
    }
    pthread_mutex_unlock(&table_lock);
    return p ? 0 : error;
}
/* `pace`: wait for this port's next period. sceAudioOutOutputs waits once for all its ports. */
static int32_t output_port(int32_t handle, const void *data, int pace) {
    int32_t error=0;
    pthread_mutex_lock(&table_lock);
    Port *p=port_of(handle,&error);
    pthread_mutex_unlock(&table_lock);
    if (!p) return error;
    pthread_mutex_lock(&p->lock);
    size_t samples=(size_t)p->frames*(size_t)p->channels, bytes=(size_t)p->frames*(size_t)p->out_channels*(size_t)p->sample_bytes;
    uint64_t period=(uint64_t)p->frames*1000000000u/48000u;
    if (data) {
        /* Port volumes and master volume, PS4 8ch to SDL 7.1 order, folded to the device's layout. */
        unsigned char converted[2048*8*4];
        shape_buffer(p,data,converted,master_gain());
        if (track_peaks(p,data)) report_peaks(p,(int)(p-ports));
        if (p->dump) fwrite(converted,1,bytes,p->dump);
        if (p->stats) {
            uint64_t now=now_ns();
            if (!p->stat_start_ns) { p->stat_start_ns=now; p->stat_min_queued=1<<30; }
            else if (now-p->stat_last_ns>p->stat_max_gap_ns) p->stat_max_gap_ns=now-p->stat_last_ns;
            p->stat_last_ns=now; ++p->stat_buffers;
            if (p->stream) {
                int queued=SDL_GetAudioStreamQueued(p->stream);
                if (queued<p->stat_min_queued) p->stat_min_queued=queued;
                if (queued==0) ++p->stat_starved;
            }
            if (now-p->stat_start_ns>=5000000000u) {
                printf("Audio stats port %d: %u buffers/5s (expected %u), max gap %.2f ms, min queued %d bytes, starved %u\n",
                       (int)(p-ports),p->stat_buffers,(unsigned)(5u*48000u/(unsigned)p->frames),p->stat_max_gap_ns/1e6,
                       p->stat_min_queued,p->stat_starved);
                p->stat_start_ns=now; p->stat_max_gap_ns=0; p->stat_starved=0; p->stat_buffers=0; p->stat_min_queued=1<<30;
            }
        }
        if (pace) {
            uint64_t now=now_ns();
            if (!p->next_deadline_ns || now>p->next_deadline_ns+8*period) p->next_deadline_ns=now; /* start or stall: restart cadence */
            else sleep_until(p->next_deadline_ns);
            p->next_deadline_ns+=period;
        }
        if (p->stream) {
            /* The device drains the queue in quanta (21 ms on PipeWire), so the level
               is a sawtooth: its minimum over ~32 buffers is what gets controlled. */
            int low=2*(int)bytes, queued=SDL_GetAudioStreamQueued(p->stream);
            if (queued<(int)bytes) {
                static const unsigned char silence[2048*8*4];
                SDL_PutAudioStreamData(p->stream,silence,low-queued<(int)bytes ? low-queued : (int)bytes);
                queued=low;
            }
            if (!p->window_count || queued<p->window_min) p->window_min=queued;
            if (++p->window_count==32) {
                p->adjust_ns=p->window_min>low+2*(int)bytes ? (int64_t)(period/32) : p->window_min<low ? -(int64_t)(period/32) : 0;
                p->window_count=0;
            }
            p->next_deadline_ns+=(uint64_t)p->adjust_ns;
            SDL_PutAudioStreamData(p->stream,converted,(int)bytes);
        }
        ++buffers_out;
    }
    p->last_output_us=now_ns()/1000;
    pthread_mutex_unlock(&p->lock);
    return data ? (int32_t)samples : 0;
}
static ABI int32_t audio_output(int32_t handle, const void *data) { return output_port(handle,data,1); }
typedef struct { int32_t handle; const void *data; } OutputParam;
static ABI int32_t audio_outputs(const OutputParam *params, uint32_t count) {
    if (!params) return ERR_INVALID_POINTER;
    if (!count || count>PORTS) return ERR_PORT_FULL;
    int32_t result=0;
    /* One period per call for all ports (FMOD feeds main and BGM together): pacing every
       port would halve the rate and starve both. */
    for (uint32_t i=0;i<count;++i) { int32_t r=output_port(params[i].handle,params[i].data,i==0); if (r<0) return r; result=r; }
    return result;
}
static ABI int32_t audio_volume(int32_t handle, int32_t flags, const int32_t *volume) {
    int32_t error=0;
    pthread_mutex_lock(&table_lock);
    Port *p=port_of(handle,&error);
    pthread_mutex_unlock(&table_lock);
    if (!p) return error;
    if (!volume) return ERR_INVALID_POINTER;
    pthread_mutex_lock(&p->lock);
    for (int c=0;c<8;++c) if (flags & (1<<c)) {
        if (volume[c]<0 || volume[c]>VOLUME_0DB) { pthread_mutex_unlock(&p->lock); return ERR_INVALID_VOLUME; }
        p->volume[c]=volume[c];
    }
    pthread_mutex_unlock(&p->lock);
    return 0;
}
static ABI int32_t audio_state(int32_t handle, PortState *state) {
    int32_t error=0;
    pthread_mutex_lock(&table_lock);
    Port *p=port_of(handle,&error);
    pthread_mutex_unlock(&table_lock);
    if (!p) return error;
    if (!state) return ERR_INVALID_POINTER;
    memset(state,0,sizeof(*state));
    switch (p->type) {
    case 2: case 3: state->output=0x40; state->channel=1; break;          /* headphone */
    case 4: state->output=0x04; state->channel=1; state->volume=127; break; /* pad speaker */
    default:
        /* The channel count of the output: a game that asks (FMOD does) mixes for it, so a 5.1 or 7.1
           device has to say so, or the game keeps its surround channels empty. */
        state->output=0x01; state->channel=(uint8_t)(p->channels>2 ? p->out_channels : p->channels); break;
    }
    if (!p->state_logged) {
        p->state_logged=1;
        printf("Runtime: audio port %d state queried by the game (reports %d ch)\n",(int)(p-ports),(int)state->channel);
    }
    return 0;
}
static ABI int32_t audio_last_time(int32_t handle, uint64_t *time) {
    int32_t error=0;
    pthread_mutex_lock(&table_lock);
    Port *p=port_of(handle,&error);
    pthread_mutex_unlock(&table_lock);
    if (!p) return error;
    if (!time) return ERR_INVALID_POINTER;
    *time=p->last_output_us; return 0;
}

static const RuntimeExport exports[]={
    {"sceAudioOutInit",audio_init}, {"sceAudioOutOpen",audio_open}, {"sceAudioOutClose",audio_close},
    {"sceAudioOutOutput",audio_output}, {"sceAudioOutOutputs",audio_outputs},
    {"sceAudioOutSetVolume",audio_volume}, {"sceAudioOutGetPortState",audio_state},
    {"sceAudioOutGetLastOutputTime",audio_last_time},
};
uintptr_t runtime_audio_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_audio_report(void) { printf("Runtime: audio ports opened=%zu, buffers output=%zu\n",ports_opened,buffers_out); }
