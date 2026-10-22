/* The 7.1 fold to stereo / 5.1, the soft limiter, the master volume and the speaker layout choice
   (src/runtime_audio.c). The last test runs the real AudioOut entry points on SDL's dummy audio
   driver and reads back the converted PCM through SPARTAN_AUDIO_DUMP. */
#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#define PEAK_INTERVAL 3 /* a level report every 3 buffers instead of every 3 seconds */
#include "../src/runtime_audio.c"

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}
/* The menu's settings, as spartanportgpu.cpp would serve them. */
static int stub_volume=100, stub_mode=MODE_AUTO;
int spartanportgpu_audio_volume_percent(void) { return stub_volume; }
int spartanportgpu_audio_channels(void) { return stub_mode; }

static int near(float a, float b) { return fabsf(a-b)<1e-3f; }
static void set_env(const char *name, const char *value) {
#ifdef _WIN32
    _putenv_s(name,value);
#else
    setenv(name,value,1);
#endif
}

/* Which layout each setting picks for which device. */
static void test_layout_choice(void) {
    assert(output_channels(MODE_AUTO,0)==OUT_71);   /* unknown device: SDL converts */
    assert(output_channels(MODE_AUTO,1)==OUT_STEREO);
    assert(output_channels(MODE_AUTO,2)==OUT_STEREO);
    assert(output_channels(MODE_AUTO,4)==OUT_STEREO);
    assert(output_channels(MODE_AUTO,6)==OUT_51);
    assert(output_channels(MODE_AUTO,7)==OUT_51);
    assert(output_channels(MODE_AUTO,8)==OUT_71);
    assert(output_channels(MODE_AUTO,12)==OUT_71);
    for (int device=0;device<=8;++device) {         /* a forced layout ignores the device */
        assert(output_channels(MODE_STEREO,device)==OUT_STEREO);
        assert(output_channels(MODE_51,device)==OUT_51);
        assert(output_channels(MODE_71,device)==OUT_71);
    }
}

static void test_soft_limit(void) {
    /* Untouched up to the knee, odd, continuous, rising and never past full scale. */
    for (float x=-0.8f;x<=0.8f;x+=0.05f) assert(soft_limit(x)==x);
    assert(near(soft_limit(0.8001f),0.8f));
    assert(near(soft_limit(1.0f),0.9f));
    assert(soft_limit(-1.0f)==-soft_limit(1.0f));
    float last=0.8f;
    for (float x=0.8f;x<=50.0f;x+=0.1f) {
        float y=soft_limit(x);
        assert(y>=last && y<=1.0f);
        assert(soft_limit(-x)==-y);
        last=y;
    }
    assert(soft_limit(1e30f)==1.0f && soft_limit(-1e30f)==-1.0f);
    assert(soft_limit(INFINITY)==1.0f && soft_limit(-INFINITY)==-1.0f);
    assert(soft_limit(NAN)==0.0f);
}

/* One frame in SDL order (FL FR C LFE BL BR SL SR) folded; `out` gets 2 or 6 samples. */
static void test_fold(void) {
    float out[8];
    /* Stereo: fronts at full level (SDL kept only ~0.21 of them), center and surrounds at -3 dB, LFE dropped. */
    fold_frame((const float[8]){0.5f,0,0,0,0,0,0,0},out,OUT_STEREO); assert(near(out[0],0.5f) && near(out[1],0));
    fold_frame((const float[8]){0,0.5f,0,0,0,0,0,0},out,OUT_STEREO); assert(near(out[0],0) && near(out[1],0.5f));
    fold_frame((const float[8]){0,0,0.5f,0,0,0,0,0},out,OUT_STEREO); assert(near(out[0],0.35355f) && near(out[1],0.35355f));
    fold_frame((const float[8]){0,0,0,1,0,0,0,0},out,OUT_STEREO); assert(near(out[0],0) && near(out[1],0));
    fold_frame((const float[8]){0,0,0,0,0.5f,0,0.5f,0},out,OUT_STEREO); assert(near(out[0],0.70711f) && near(out[1],0));
    fold_frame((const float[8]){0,0,0,0,0,0.5f,0,0.5f},out,OUT_STEREO); assert(near(out[0],0) && near(out[1],0.70711f));

    /* 5.1: front, center and LFE as they are; each surround is the side channel plus the back one at -3 dB. */
    fold_frame((const float[8]){0.1f,0.2f,0.3f,0.4f,0,0,0,0},out,OUT_51);
    assert(near(out[0],0.1f) && near(out[1],0.2f) && near(out[2],0.3f) && near(out[3],0.4f) && near(out[4],0) && near(out[5],0));
    fold_frame((const float[8]){0,0,0,0,0.5f,0,0.5f,0},out,OUT_51); assert(near(out[4],0.85355f) && near(out[5],0));
    fold_frame((const float[8]){0,0,0,0,0,0.5f,0,0.5f},out,OUT_51); assert(near(out[4],0) && near(out[5],0.85355f));
    fold_frame((const float[8]){0,0,0,0,1,0,0,0},out,OUT_51); assert(near(out[4],0.70711f));
    fold_frame((const float[8]){0,0,0,0,0,0,1,0},out,OUT_51); assert(near(out[4],1.0f));
}

static Port make_port(int channels, int is_float, int std_layout, int out_channels, int frames) {
    Port p; memset(&p,0,sizeof(p));
    p.used=1; p.channels=channels; p.is_float=is_float; p.sample_bytes=is_float ? 4 : 2; p.std_layout=std_layout;
    p.out_channels=out_channels; p.frames=frames;
    for (int c=0;c<8;++c) p.volume[c]=VOLUME_0DB;
    return p;
}

static void test_shape_buffer(void) {
    float out[16];
    /* PS4 order L R C LFE SL SR BL BR in: channel 4 (SL) must come out as SDL's SL (index 6). */
    const float ps4[8]={0.1f,0.2f,0.3f,0.4f,0.5f,0.6f,0.7f,0.8f};
    Port p=make_port(8,1,0,8,1);
    shape_buffer(&p,ps4,out,1.0f);
    const float sdl[8]={0.1f,0.2f,0.3f,0.4f,0.7f,0.8f,0.5f,0.6f};
    for (int c=0;c<8;++c) assert(out[c]==sdl[c]);
    /* The standard-layout formats (6, 7) are already in SDL order. */
    p=make_port(8,1,1,8,1);
    shape_buffer(&p,ps4,out,1.0f);
    for (int c=0;c<8;++c) assert(out[c]==ps4[c]);

    /* Stereo: the PS4 side channel (4) and back channel (6) both reach the left ear at -3 dB. */
    p=make_port(8,1,0,OUT_STEREO,1);
    shape_buffer(&p,(const float[8]){0,0,0,0,0.5f,0,0,0},out,1.0f); assert(near(out[0],0.35355f) && near(out[1],0));
    shape_buffer(&p,(const float[8]){0,0,0,0,0,0,0.5f,0},out,1.0f); assert(near(out[0],0.35355f) && near(out[1],0));
    /* 5.1: the PS4 side channel lands on the left surround as it is. */
    p=make_port(8,1,0,OUT_51,1);
    shape_buffer(&p,(const float[8]){0,0,0,0,0.5f,0,0,0},out,1.0f); assert(near(out[4],0.5f) && near(out[5],0));
    assert(near(out[0],0) && near(out[1],0) && near(out[2],0) && near(out[3],0));

    /* A hot mix folded to stereo bends instead of clipping: left = 0.8 + 0.7071 * 0.8 (center) = 1.37. */
    p=make_port(8,1,0,OUT_STEREO,1);
    shape_buffer(&p,(const float[8]){0.8f,0.8f,0.8f,0,0,0,0,0},out,1.0f);
    assert(out[0]>0.9f && out[0]<1.0f && out[0]==out[1]);

    /* Per-channel volumes and the master volume multiply; 0 is silence. */
    p=make_port(2,1,0,2,1);
    p.volume[0]=VOLUME_0DB/2;
    shape_buffer(&p,(const float[2]){0.5f,0.5f},out,1.0f); assert(near(out[0],0.25f) && near(out[1],0.5f));
    shape_buffer(&p,(const float[2]){0.4f,0.4f},out,0.5f); assert(near(out[0],0.1f) && near(out[1],0.2f));
    shape_buffer(&p,(const float[2]){0.4f,0.4f},out,0.0f); assert(out[0]==0 && out[1]==0);
    /* Boosting 200% past full scale is limited; below the knee it is exactly double. */
    p=make_port(2,1,0,2,1);
    shape_buffer(&p,(const float[2]){0.3f,0.9f},out,2.0f); assert(near(out[0],0.6f) && out[1]<1.0f && out[1]>0.9f);
    /* Without a boost, a port that is passed on as it is stays bit-exact, even near full scale. */
    shape_buffer(&p,(const float[2]){0.95f,-1.0f},out,1.0f); assert(out[0]==0.95f && out[1]==-1.0f);

    /* s16: exact at unity, rounded, clamped at the rails after a boost. */
    int16_t in16[2]={16384,-32768}, out16[2];
    p=make_port(2,0,0,2,1);
    shape_buffer(&p,in16,out16,1.0f); assert(out16[0]==16384 && out16[1]==-32768);
    shape_buffer(&p,in16,out16,0.5f); assert(out16[0]==8192 && out16[1]==-16384);
    shape_buffer(&p,in16,out16,2.0f); assert(out16[0]>=29000 && out16[0]<=32767 && out16[1]>=-32768 && out16[1]<-29000);
    /* s16 8ch folded to stereo, several frames in one buffer. */
    int16_t in8[2*8]={16384,0,0,0,0,0,0,0, 32767,32767,32767,0,32767,32767,32767,32767}, st[2*2];
    p=make_port(8,0,0,OUT_STEREO,2);
    shape_buffer(&p,in8,st,1.0f);
    assert(st[0]==16384 && st[1]==0);
    assert(st[2]>29000 && st[2]<=32767 && st[3]>29000 && st[3]<=32767);
}

/* The levels the game sends, per channel in the PS4's order, are collected and reported on a schedule. */
static void test_peaks(void) {
    Port p=make_port(8,1,0,OUT_71,2);
    const float a[16]={0.1f,-0.2f,0,0,0.9f,0,0,0, 0.3f,0,-0.4f,0,0,0,0,0.05f};
    const float b[16]={0};
    assert(!track_peaks(&p,a) && !track_peaks(&p,b));      /* buffers 1 and 2 */
    assert(track_peaks(&p,b));                               /* the 3rd is due */
    const float expect[8]={0.3f,0.2f,0.4f,0,0.9f,0,0,0.05f};
    for (int c=0;c<8;++c) assert(near(p.peak[c],expect[c]));
    report_peaks(&p,0);
    for (int c=0;c<8;++c) assert(p.peak[c]==0);
    assert(p.peak_buffers==0 && p.peak_reports==1);
    for (int i=1;i<PEAK_REPORTS;++i) { for (int k=0;k<PEAK_INTERVAL;++k) track_peaks(&p,a); report_peaks(&p,0); }
    assert(p.peak_reports==PEAK_REPORTS && !track_peaks(&p,a)); /* only the first half minute is logged */
    Port stereo=make_port(2,1,0,2,2);
    assert(!track_peaks(&stereo,a) && !track_peaks(&stereo,a) && !track_peaks(&stereo,a)); /* 8ch ports only */
}

/* AudioOut end to end on SDL's dummy driver: open a 7.1 float port, output one period, read the
   PCM that was handed to the device back from the dump file. */
static size_t run_port(const char *dump, int mode, int volume, const float *frame_ps4, float *got, int *out_channels, int *state_channels) {
    stub_mode=mode; stub_volume=volume;
    int32_t handle=audio_open(0,0,0,256,48000,5); /* main port, F32 8ch */
    assert(handle>=0);
    int32_t id=handle & 0xff;
    *out_channels=ports[id].out_channels;
    PortState state;
    assert(audio_state(handle,&state)==0);
    *state_channels=state.channel;   /* what the game is told about the output */
    static float period[256*8];
    for (int f=0;f<256;++f) memcpy(&period[f*8],frame_ps4,8*sizeof(float));
    assert(audio_output(handle,period)==256*8);
    assert(audio_close(handle)==0);
    char path[4096];
    snprintf(path,sizeof(path),"%s.port%d.%dch.f32",dump,(int)id,*out_channels);
    FILE *file=fopen(path,"rb");
    assert(file);
    size_t n=fread(got,sizeof(float),256*8,file);
    fclose(file); remove(path);
    return n;
}
static void test_end_to_end(void) {
    const char *dump="spartanport_audio_test_dump"; /* in the working directory; each run removes its file */
    set_env("SDL_AUDIODRIVER","dummy");
    set_env("SPARTAN_AUDIO_DUMP",dump);
    assert(audio_init()==0);

    /* PS4 order L R C LFE SL SR BL BR. */
    const float ps4[8]={0.1f,0.2f,0.3f,0.4f,0.5f,0.0f,0.0f,0.0f};
    float got[256*8]; int out_channels, state_channels;
    size_t n;

    n=run_port(dump,MODE_STEREO,100,ps4,got,&out_channels,&state_channels);
    assert(out_channels==2 && n==256*2 && state_channels==2);
    assert(near(got[0],0.1f+0.70711f*(0.3f+0.5f)) && near(got[1],0.2f+0.70711f*0.3f));
    assert(near(got[2*255],got[0]) && near(got[2*255+1],got[1])); /* every frame of the period */

    n=run_port(dump,MODE_51,100,ps4,got,&out_channels,&state_channels);
    assert(out_channels==6 && n==256*6 && state_channels==6);   /* a 5.1 output says so, so the game mixes for it */
    assert(near(got[0],0.1f) && near(got[1],0.2f) && near(got[2],0.3f) && near(got[3],0.4f) && near(got[4],0.5f) && near(got[5],0));

    n=run_port(dump,MODE_71,100,ps4,got,&out_channels,&state_channels);
    assert(out_channels==8 && n==256*8 && state_channels==8);
    assert(near(got[0],0.1f) && near(got[4],0) && near(got[6],0.5f)); /* SL moved to SDL's index 6 */

    n=run_port(dump,MODE_51,50,ps4,got,&out_channels,&state_channels);   /* live master volume */
    assert(n==256*6 && near(got[0],0.05f) && near(got[4],0.25f));

    /* Automatic follows whatever the dummy device reports. */
    SDL_AudioSpec device; int frames;
    assert(SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&device,&frames));
    n=run_port(dump,MODE_AUTO,100,ps4,got,&out_channels,&state_channels);
    assert(out_channels==output_channels(MODE_AUTO,device.channels) && n==(size_t)256*(size_t)out_channels);
}

int main(void) {
    test_layout_choice();
    test_soft_limit();
    test_fold();
    test_shape_buffer();
    test_peaks();
    test_end_to_end();
    puts("audio downmix ok");
    return 0;
}
