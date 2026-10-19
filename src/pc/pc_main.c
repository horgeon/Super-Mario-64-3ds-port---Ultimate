#include <stdlib.h>

#ifdef TARGET_N3DS
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <3ds/archive.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8
#endif

#ifdef TARGET_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "sm64.h"

#include "game/memory.h"
#include "audio/external.h"

#include "gfx/gfx_pc.h"
#include "gfx/gfx_opengl.h"
#include "gfx/gfx_direct3d11.h"
#include "gfx/gfx_direct3d12.h"
#include "gfx/gfx_dxgi.h"
#include "gfx/gfx_glx.h"
#include "gfx/gfx_sdl.h"
#include "gfx/gfx_3ds.h"
#include "gfx/gfx_citro3d.h"

#include "audio/audio_api.h"
#include "audio/audio_wasapi.h"
#include "audio/audio_pulse.h"
#include "audio/audio_alsa.h"
#include "audio/audio_sdl.h"
#include "audio/audio_null.h"
#include "audio/audio_3ds.h"

#include "controller/controller_keyboard.h"

#include "configfile.h"

#include "compat.h"

#include "pc/utils_3ds.h"

OSMesg D_80339BEC;
OSMesgQueue gSIEventMesgQueue;

s8 gResetTimer;
s8 D_8032C648;
s8 gDebugLevelSelect;
s8 gShowProfiler;
s8 gShowDebugText;

static struct AudioAPI *audio_api;
static struct GfxWindowManagerAPI *wm_api;
static struct GfxRenderingAPI *rendering_api;

extern void gfx_run(Gfx *commands);
extern void thread5_game_loop(void *arg);
extern void create_next_audio_buffer(s16 *samples, u32 num_samples);
void game_loop_one_iteration(void);

void dispatch_audio_sptask(UNUSED struct SPTask *spTask) {
}

void set_vblank_handler(UNUSED s32 index, UNUSED struct VblankHandler *handler, UNUSED OSMesgQueue *queue, UNUSED OSMesg *msg) {
}

static uint8_t inited = 0;

#include "game/game_init.h" // for gGlobalTimer
#ifdef TARGET_N3DS
u32 gN3DSRenderedFrameCounter = 0;
#endif

void send_display_list(struct SPTask *spTask) {
    if (!inited) {
        return;
    }
    gfx_run((Gfx *)spTask->task.t.data_ptr);
}

#define printf

#ifdef VERSION_EU
#define SAMPLES_HIGH 656
#define SAMPLES_LOW 640
#else
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#endif

static void patch_interpolations(void) {
    extern void mtx_patch_interpolated(void);
    extern void patch_screen_transition_interpolated(void);
    extern void patch_title_screen_scales(void);
    extern void patch_interpolated_dialog(void);
    extern void patch_interpolated_hud(void);
    extern void patch_interpolated_paintings(void);
    extern void patch_interpolated_bubble_particles(void);
    extern void patch_interpolated_snow_particles(void);
    mtx_patch_interpolated();
    patch_screen_transition_interpolated();
    patch_title_screen_scales();
    patch_interpolated_dialog();
    patch_interpolated_hud();
    patch_interpolated_paintings();
    patch_interpolated_bubble_particles();
    patch_interpolated_snow_particles();
}

void produce_one_frame(void) {
#ifdef TARGET_N3DS
    // Reset gMtxTblSize before each frame so that if the second render pass
    // is skipped (audio not buffered), stale gMtxTbl[].pos pointers into
    // previously-freed display list buffers don't accumulate.
    extern s32 gMtxTblSize;
    gMtxTblSize = 0;
#endif
    gfx_start_frame();
#ifdef TARGET_N3DS
    gN3DSRenderedFrameCounter++;
#endif
    game_loop_one_iteration();

#ifndef TARGET_N3DS
    int samples_left = audio_api->buffered();
    u32 num_audio_samples = samples_left < audio_api->get_desired_buffered() ? SAMPLES_HIGH : SAMPLES_LOW;
    s16 audio_buffer[SAMPLES_HIGH * 2 * 2];
    for (int i = 0; i < 2; i++) {
        create_next_audio_buffer(audio_buffer + i * (num_audio_samples * 2), num_audio_samples);
    }
    audio_api->play((u8 *)audio_buffer, 2 * num_audio_samples * 4);
#endif

    gfx_end_frame();

#ifdef TARGET_N3DS
    if (audio_api->buffered() < audio_api->get_desired_buffered()) {
        return;
    }
#endif

    gfx_start_frame();
#ifdef TARGET_N3DS
    gN3DSRenderedFrameCounter++;
#endif
    patch_interpolations();
    send_display_list(gGfxSPTask);
    gfx_end_frame();
}

#ifdef TARGET_WEB
static void em_main_loop(void) {
}

static void request_anim_frame(void (*func)(double time)) {
    EM_ASM(requestAnimationFrame(function(time) {
        dynCall("vd", $0, [time]);
    }), func);
}

static void on_anim_frame(double time) {
    static double target_time;

    time *= 0.03; // milliseconds to frame count (33.333 ms -> 1)

    if (time >= target_time + 10.0) {
        // We are lagging 10 frames behind, probably due to coming back after inactivity,
        // so reset, with a small margin to avoid potential jitter later.
        target_time = time - 0.010;
    }

    for (int i = 0; i < 2; i++) {
        // If refresh rate is 15 Hz or something we might need to generate two frames
        if (time >= target_time) {
            produce_one_frame();
            target_time = target_time + 1.0;
        }
    }

    request_anim_frame(on_anim_frame);
}
#endif

static void save_config(void) {
    configfile_save(CONFIG_FILE);
}

#ifdef TARGET_N3DS
static bool mount_save_data(void) {
    const FS_Path saveDataPath = fsMakePath(PATH_EMPTY, "");
    Result result = archiveMount(ARCHIVE_SAVEDATA, saveDataPath, "save");

    if (R_FAILED(result) && R_MODULE(result) == RM_FS && R_SUMMARY(result) == RS_INVALIDSTATE &&
        R_DESCRIPTION(result) == 340) {
        // "blocks" argument is a block size on real firmware (0x200 on official titles). Size is specified in TMD.
        result = FSUSER_FormatSaveData(ARCHIVE_SAVEDATA, fsMakePath(PATH_EMPTY, ""), 0x200, 8, 32, 8, 32, false);
        if (R_SUCCEEDED(result)) {
            result = archiveMount(ARCHIVE_SAVEDATA, saveDataPath, "save");
        }
    }

    if (R_FAILED(result)) {
        puts("Failed to initialize CIA save data archive.");
        error_box(true, true, "Failed to initialize CIA save data archive.");
        return false;
    }
    return true;
}
#endif

static void on_fullscreen_changed(bool is_now_fullscreen) {
    configFullscreen = is_now_fullscreen;
}

void main_func(void) {
    static u8 pool[DOUBLE_SIZE_ON_64_BIT(0x165000)] __attribute__ ((aligned(16)));
    main_pool_init(pool, pool + sizeof(pool));
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);

#ifdef TARGET_N3DS
    if (!mount_save_data()) {
        return;
    }
#endif

    configfile_load(CONFIG_FILE);
    atexit(save_config);

#ifdef TARGET_WEB
    emscripten_set_main_loop(em_main_loop, 0, 0);
    request_anim_frame(on_anim_frame);
#endif

#if defined(ENABLE_DX12)
    rendering_api = &gfx_direct3d12_api;
    wm_api = &gfx_dxgi_api;
#elif defined(ENABLE_DX11)
    rendering_api = &gfx_direct3d11_api;
    wm_api = &gfx_dxgi_api;
#elif defined(ENABLE_OPENGL)
    rendering_api = &gfx_opengl_api;
    #if defined(__linux__) || defined(__BSD__)
        wm_api = &gfx_glx;
    #else
        wm_api = &gfx_sdl;
    #endif
#elif defined(TARGET_N3DS)
    wm_api = &gfx_3ds;
    rendering_api = &gfx_citro3d_api;
#endif

    gfx_init(wm_api, rendering_api, "Super Mario 64 Port", configFullscreen);

    wm_api->set_fullscreen_changed_callback(on_fullscreen_changed);
    wm_api->set_keyboard_callbacks(keyboard_on_key_down, keyboard_on_key_up, keyboard_on_all_keys_up);

#if HAVE_WASAPI
    if (audio_api == NULL && audio_wasapi.init()) {
        audio_api = &audio_wasapi;
    }
#endif
#if HAVE_PULSE_AUDIO
    if (audio_api == NULL && audio_pulse.init()) {
        audio_api = &audio_pulse;
    }
#endif
#if HAVE_ALSA
    if (audio_api == NULL && audio_alsa.init()) {
        audio_api = &audio_alsa;
    }
#endif
#ifdef TARGET_WEB
    if (audio_api == NULL && audio_sdl.init()) {
        audio_api = &audio_sdl;
    }
#endif

#if defined TARGET_N3DS && !defined DISABLE_AUDIO
    if (audio_api == NULL && audio_3ds.init()) {
        audio_api = &audio_3ds;
    }
#endif

    if (audio_api == NULL) {
        audio_api = &audio_null;
    }

    audio_init();
    sound_init();

    thread5_game_loop(NULL);
#ifdef TARGET_WEB
    inited = 1;
#elif defined(TARGET_N3DS)
    inited = 1;
    //the 3ds version has its own main loop
    wm_api->main_loop(produce_one_frame);
    audio_api->stop();
#else
    inited = 1;
    while (1) {
        wm_api->main_loop(produce_one_frame);
    }
#endif
}

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
int WINAPI WinMain(UNUSED HINSTANCE hInstance, UNUSED HINSTANCE hPrevInstance, UNUSED LPSTR pCmdLine, UNUSED int nCmdShow) {
    main_func();
    return 0;
}
#else
int main(UNUSED int argc, UNUSED char *argv[]) {
    main_func();
    return 0;
}
#endif
