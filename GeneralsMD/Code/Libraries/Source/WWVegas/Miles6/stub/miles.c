/*
**  Miles Sound System Stub for Linux
**  Exports match the real mss32.dll decorated stdcall names
*/

#include <stddef.h>
#include <stdlib.h>

#if defined(_WIN32)
#define MILESSTUBCALL __stdcall
#else
#define MILESSTUBCALL
#endif

#if defined(__linux__)
/* Native unavailable backend. Compile against the consumer declarations so
 * symbol names, handle types and output widths cannot silently diverge. */
#include "MSS/MSS.h"

S32 AILCALL AIL_startup(void) { return 0; }
void AILCALL AIL_shutdown(void) {}
S32 AILCALL AIL_quick_startup(S32 digital, S32 midi, U32 rate, S32 bits, S32 channels) { return 0; }
void AILCALL AIL_quick_handles(HDIGDRIVER *dig, HMDIDRIVER *mdi, HDLSDEVICE *dls)
{
    if (dig) *dig = NULL;
    if (mdi) *mdi = NULL;
    if (dls) *dls = NULL;
}
char *AILCALL AIL_set_redist_directory(const char *dir) { return NULL; }
void AILCALL AIL_lock(void) {}
void AILCALL AIL_unlock(void) {}
S32 AILCALL AIL_get_timer_highest_delay(void) { return 0; }
void AILCALL AIL_set_file_callbacks(AILFILEOPENCB open, AILFILECLOSECB close, AILFILESEEKCB seek, AILFILEREADCB read) {}

S32 AILCALL AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
    if (next) *next = HPROENUM_FIRST;
    if (dest) *dest = 0;
    if (name) *name = NULL;
    return 0;
}
/* Miles M3D_NOERR is zero: a nonzero result means unavailable. */
S32 AILCALL AIL_open_3D_provider(HPROVIDER lib) { return -1; }
void AILCALL AIL_close_3D_provider(HPROVIDER lib) {}
S32 AILCALL AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **name)
{
    return AIL_enumerate_3D_providers(next, dest, name);
}
void AILCALL AIL_set_3D_speaker_type(HPROVIDER lib, S32 type) {}
HPROVIDER AILCALL AIL_set_sample_processor(HSAMPLE sample, S32 stage, HPROVIDER provider) { return 0; }
void AILCALL AIL_set_filter_sample_preference(HSAMPLE sample, const char *name, const void *value) {}

HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER dig) { return NULL; }
void AILCALL AIL_release_sample_handle(HSAMPLE sample) {}
void AILCALL AIL_init_sample(HSAMPLE sample) {}
S32 AILCALL AIL_set_sample_file(HSAMPLE sample, const void *image, S32 block) { return 0; }
void AILCALL AIL_start_sample(HSAMPLE sample) {}
void AILCALL AIL_stop_sample(HSAMPLE sample) {}
void AILCALL AIL_resume_sample(HSAMPLE sample) {}
void AILCALL AIL_end_sample(HSAMPLE sample) {}
void AILCALL AIL_set_sample_volume_pan(HSAMPLE sample, F32 volume, F32 pan) {}
void AILCALL AIL_sample_volume_pan(HSAMPLE sample, F32 *volume, F32 *pan)
{
    if (volume) *volume = 0;
    if (pan) *pan = 0;
}
void AILCALL AIL_set_sample_playback_rate(HSAMPLE sample, S32 rate) {}
S32 AILCALL AIL_sample_playback_rate(HSAMPLE sample) { return 0; }
void AILCALL AIL_set_sample_user_data(HSAMPLE sample, U32 index, S32 value) {}
S32 AILCALL AIL_sample_user_data(HSAMPLE sample, U32 index) { return 0; }
AILSAMPLECB AILCALL AIL_register_EOS_callback(HSAMPLE sample, AILSAMPLECB callback) { return NULL; }
void AILCALL AIL_get_DirectSound_info(HSAMPLE sample, AILLPDIRECTSOUND *ds, AILLPDIRECTSOUNDBUFFER *buffer)
{
    if (ds) *ds = NULL;
    if (buffer) *buffer = NULL;
}

H3DSAMPLE AILCALL AIL_allocate_3D_sample_handle(HPROVIDER lib) { return NULL; }
void AILCALL AIL_release_3D_sample_handle(H3DSAMPLE sample) {}
S32 AILCALL AIL_set_3D_sample_file(H3DSAMPLE sample, const void *image) { return 0; }
void AILCALL AIL_start_3D_sample(H3DSAMPLE sample) {}
void AILCALL AIL_stop_3D_sample(H3DSAMPLE sample) {}
void AILCALL AIL_resume_3D_sample(H3DSAMPLE sample) {}
void AILCALL AIL_end_3D_sample(H3DSAMPLE sample) {}
void AILCALL AIL_set_3D_sample_volume(H3DSAMPLE sample, F32 volume) {}
void AILCALL AIL_set_3D_sample_playback_rate(H3DSAMPLE sample, S32 rate) {}
S32 AILCALL AIL_3D_sample_playback_rate(H3DSAMPLE sample) { return 0; }
void AILCALL AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max, F32 min) {}
void AILCALL AIL_set_3D_sample_occlusion(H3DSAMPLE sample, F32 occlusion) {}
void AILCALL AIL_set_3D_user_data(H3DPOBJECT obj, U32 index, S32 value) {}
S32 AILCALL AIL_3D_user_data(H3DPOBJECT obj, U32 index) { return 0; }
AIL3DSAMPLECB AILCALL AIL_register_3D_EOS_callback(H3DSAMPLE sample, AIL3DSAMPLECB callback) { return NULL; }
H3DPOBJECT AILCALL AIL_open_3D_listener(HPROVIDER lib) { return NULL; }
void AILCALL AIL_close_3D_listener(H3DPOBJECT listener) {}
void AILCALL AIL_set_3D_position(H3DPOBJECT obj, F32 x, F32 y, F32 z) {}
void AILCALL AIL_set_3D_orientation(H3DPOBJECT obj, F32 x, F32 y, F32 z, F32 ux, F32 uy, F32 uz) {}
void AILCALL AIL_set_3D_velocity_vector(H3DPOBJECT obj, F32 x, F32 y, F32 z) {}

HSTREAM AILCALL AIL_open_stream(HDIGDRIVER dig, const char *name, S32 memory) { return NULL; }
void AILCALL AIL_close_stream(HSTREAM stream) {}
void AILCALL AIL_start_stream(HSTREAM stream) {}
void AILCALL AIL_pause_stream(HSTREAM stream, S32 pause) {}
void AILCALL AIL_set_stream_loop_count(HSTREAM stream, S32 count) {}
S32 AILCALL AIL_stream_loop_count(HSTREAM stream) { return 0; }
void AILCALL AIL_set_stream_volume_pan(HSTREAM stream, F32 volume, F32 pan) {}
void AILCALL AIL_stream_volume_pan(HSTREAM stream, F32 *volume, F32 *pan)
{
    AIL_sample_volume_pan(NULL, volume, pan);
}
void AILCALL AIL_stream_ms_position(HSTREAM stream, S32 *total, S32 *current)
{
    if (total) *total = 0;
    if (current) *current = 0;
}
AILSTREAMCB AILCALL AIL_register_stream_callback(HSTREAM stream, AILSTREAMCB callback) { return NULL; }
HAUDIO AILCALL AIL_quick_load_and_play(const char *name, U32 loops, S32 wait) { return NULL; }
void AILCALL AIL_quick_unload(HAUDIO audio) {}
void AILCALL AIL_quick_set_volume(HAUDIO audio, F32 volume, F32 extra) {}

S32 AILCALL AIL_WAV_info(const void *data, AILSOUNDINFO *info)
{
    if (info) { const AILSOUNDINFO empty = {0}; *info = empty; }
    return 0;
}
S32 AILCALL AIL_decompress_ADPCM(const AILSOUNDINFO *info, void **data, U32 *size)
{
    if (data) *data = NULL;
    if (size) *size = 0;
    return 0;
}
void AILCALL AIL_mem_free_lock(void *ptr) { free(ptr); }
#else
// AIL startup/shutdown
long MILESSTUBCALL _AIL_startup(void) { return 1; }
void MILESSTUBCALL _AIL_shutdown(void) {}

// Digital driver
long MILESSTUBCALL _AIL_open_digital_driver(long rate, long bits, long channels) { return 1; }
void MILESSTUBCALL _AIL_close_digital_driver(long hDriver) {}

// Sample handling
long MILESSTUBCALL _AIL_allocate_sample_handle(long hDriver) { return 1; }
void MILESSTUBCALL _AIL_release_sample_handle(long hSample) {}
void MILESSTUBCALL _AIL_init_sample(long hSample) {}
void MILESSTUBCALL _AIL_set_sample_file(long hSample, const char* filename, long block) {}
void MILESSTUBCALL _AIL_set_sample_type(long hSample, long type, long format) {}
void MILESSTUBCALL _AIL_set_sample_playback_rate(long hSample, long rate) {}
void MILESSTUBCALL _AIL_set_sample_volume(long hSample, long volume) {}
void MILESSTUBCALL _AIL_set_sample_pan(long hSample, long pan) {}
void MILESSTUBCALL _AIL_start_sample(long hSample) {}
void MILESSTUBCALL _AIL_stop_sample(long hSample) {}
long MILESSTUBCALL _AIL_sample_status(long hSample) { return 0; }

// Memory provider
void* MILESSTUBCALL _AIL_mem_alloc_lock(long size) { return malloc(size); }
void MILESSTUBCALL _AIL_mem_free_lock(void* ptr) { free(ptr); }

// Timer
double MILESSTUBCALL _AIL_get_preference(long preference) { return 0.0; }
void MILESSTUBCALL _AIL_set_preference(long preference, double value) {}

// Error handling
const char* MILESSTUBCALL _AIL_error(void) { return "Miles stub"; }
void MILESSTUBCALL _AIL_set_error_handler(void (*handler)(const char*)) {}

// Redbook (CD audio)
long MILESSTUBCALL _AIL_redbook_open(long drive) { return 0; }
void MILESSTUBCALL _AIL_redbook_close(long hRedbook) {}
long MILESSTUBCALL _AIL_redbook_play(long hRedbook, long start_track, long end_track) { return 0; }
void MILESSTUBCALL _AIL_redbook_stop(long hRedbook) {}
long MILESSTUBCALL _AIL_redbook_status(long hRedbook) { return 0; }
long MILESSTUBCALL _AIL_redbook_track_count(long hRedbook) { return 0; }
long MILESSTUBCALL _AIL_redbook_track_time(long hRedbook, long track) { return 0; }

// XMIDI
long MILESSTUBCALL _AIL_open_xmidi_driver(void) { return 1; }
void MILESSTUBCALL _AIL_close_xmidi_driver(long hDriver) {}
long MILESSTUBCALL _AIL_allocate_xmidi_handle(long hDriver) { return 1; }
void MILESSTUBCALL _AIL_release_xmidi_handle(long hXMIDI) {}
void MILESSTUBCALL _AIL_init_xmidi(long hXMIDI) {}
void MILESSTUBCALL _AIL_set_xmidi_file(long hXMIDI, const char* filename) {}
void MILESSTUBCALL _AIL_start_xmidi(long hXMIDI) {}
void MILESSTUBCALL _AIL_stop_xmidi(long hXMIDI) {}
long MILESSTUBCALL _AIL_xmidi_status(long hXMIDI) { return 0; }

// MSS version
const char* MILESSTUBCALL _AIL_get_version(void) { return "Miles Stub 1.0"; }

// Other common exports
void MILESSTUBCALL _AIL_lock(void) {}
void MILESSTUBCALL _AIL_unlock(void) {}
long MILESSTUBCALL _AIL_get_dll_version(void) { return 0x0400; }  // v4.0
#endif /* __linux__ */