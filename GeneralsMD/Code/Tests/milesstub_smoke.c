/* Only public headers: no private declarations or underscore-symbol aliases. */
#include <MSS/MSS.h>
#include <Mss.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static void AILCALLBACK sample_done(HSAMPLE sample) { (void)sample; }
static void AILCALLBACK sample3d_done(H3DSAMPLE sample) { (void)sample; }
static void AILCALLBACK stream_done(HSTREAM stream) { (void)stream; }
static U32 AILCALLBACK file_open(const char *name, U32 *handle)
{
    (void)name; (void)handle; return 0;
}
static void AILCALLBACK file_close(U32 handle) { (void)handle; }
static S32 AILCALLBACK file_seek(U32 handle, S32 offset, U32 type)
{
    (void)handle; (void)offset; (void)type; return 0;
}
static U32 AILCALLBACK file_read(U32 handle, void *buffer, U32 bytes)
{
    (void)handle; (void)buffer; (void)bytes; return 0;
}

int main(void)
{
    int sentinel = 42;
    HDIGDRIVER dig = (HDIGDRIVER)&sentinel;
    HMDIDRIVER mdi = (HMDIDRIVER)&sentinel;
    HDLSDEVICE dls = (HDLSDEVICE)&sentinel;
    HPROENUM next = 99;
    HPROVIDER provider = 99;
    char marker[] = "unchanged";
    char *name = marker;
    char version[] = "unknown";
    F32 volume = -1, pan = -1;
    struct { S32 total, current; U32 canary; } position = {-1, -1, 0xabcdef01U};
    AILLPDIRECTSOUND ds = &sentinel;
    AILLPDIRECTSOUNDBUFFER dsbuffer = &sentinel;
    AILSOUNDINFO info;
    void *decoded = &sentinel;
    U32 decoded_size = 99;
    unsigned char *memory;

    CHECK(sizeof(S8) == 1 && sizeof(U8) == 1);
    CHECK(sizeof(S16) == 2 && sizeof(U16) == 2);
    CHECK(sizeof(S32) == 4 && sizeof(U32) == 4);
    CHECK((S32)-1 < 0 && (U32)-1 > 0);
    CHECK(sizeof(HDIGDRIVER) == sizeof(void *));
    AIL_MSS_version(version, 0);
    CHECK(strcmp(version, "unknown") == 0);
    AIL_MSS_version(NULL, 0);
    AIL_MSS_version(NULL, 8);
    AIL_MSS_version(version, sizeof(version));
    CHECK(version[0] == '\0' && version[1] == 'n');

    AIL_shutdown(); /* Cleanup is safe even before startup. */
    CHECK(AIL_startup() == 0);
    CHECK(AIL_quick_startup(1, 1, 44100, 16, 2) == 0);
    AIL_quick_handles(&dig, &mdi, &dls);
    CHECK(dig == NULL && mdi == NULL && dls == NULL);
    AIL_quick_handles(NULL, NULL, NULL);
    CHECK(AIL_set_redist_directory(NULL) == NULL);
    CHECK(AIL_set_redist_directory(".") == NULL);
    AIL_lock(); AIL_unlock();
    CHECK(AIL_get_timer_highest_delay() == 0);
    AIL_set_file_callbacks(file_open, file_close, file_seek, file_read);
    AIL_set_file_callbacks(NULL, NULL, NULL, NULL);

    CHECK(AIL_enumerate_3D_providers(&next, &provider, &name) == 0);
    CHECK(next == HPROENUM_FIRST && provider == 0 && name == NULL);
    CHECK(AIL_enumerate_3D_providers(NULL, NULL, NULL) == 0);
    next = 99; provider = 99; name = marker;
    CHECK(AIL_enumerate_filters(&next, &provider, &name) == 0);
    CHECK(next == HPROENUM_FIRST && provider == 0 && name == NULL);
    CHECK(AIL_enumerate_filters(NULL, NULL, NULL) == 0);
    CHECK(AIL_open_3D_provider(0) != 0); /* M3D_NOERR is zero. */
    CHECK(AIL_open_3D_provider(99) != 0);
    AIL_set_3D_speaker_type(0, AIL_3D_2_SPEAKER);
    AIL_close_3D_provider(0);
    CHECK(AIL_set_sample_processor(NULL, DP_ASI_DECODER, 0) == 0);
    AIL_set_filter_sample_preference(NULL, "unavailable", NULL);

    CHECK(AIL_allocate_sample_handle(NULL) == NULL);
    AIL_init_sample(NULL);
    CHECK(AIL_set_sample_file(NULL, NULL, 0) == 0);
    CHECK(AIL_set_sample_file(NULL, marker, 0) == 0);
    AIL_start_sample(NULL); AIL_stop_sample(NULL);
    AIL_resume_sample(NULL); AIL_end_sample(NULL);
    AIL_set_sample_volume_pan(NULL, 1, 0.5f);
    AIL_sample_volume_pan(NULL, &volume, &pan);
    CHECK(volume == 0 && pan == 0);
    AIL_sample_volume_pan(NULL, NULL, NULL);
    AIL_set_sample_playback_rate(NULL, 44100);
    CHECK(AIL_sample_playback_rate(NULL) == 0);
    AIL_set_sample_user_data(NULL, 0, -7);
    CHECK(AIL_sample_user_data(NULL, 0) == 0);
    CHECK(AIL_register_EOS_callback(NULL, sample_done) == NULL);
    CHECK(AIL_register_EOS_callback(NULL, NULL) == NULL);
    AIL_get_DirectSound_info(NULL, &ds, &dsbuffer);
    CHECK(ds == NULL && dsbuffer == NULL);
    AIL_get_DirectSound_info(NULL, NULL, NULL);
    AIL_release_sample_handle(NULL);

    CHECK(AIL_allocate_3D_sample_handle(0) == NULL);
    CHECK(AIL_set_3D_sample_file(NULL, NULL) == 0);
    CHECK(AIL_set_3D_sample_file(NULL, marker) == 0);
    AIL_start_3D_sample(NULL); AIL_stop_3D_sample(NULL);
    AIL_resume_3D_sample(NULL); AIL_end_3D_sample(NULL);
    AIL_set_3D_sample_volume(NULL, 1);
    AIL_set_3D_sample_playback_rate(NULL, 44100);
    CHECK(AIL_3D_sample_playback_rate(NULL) == 0);
    AIL_set_3D_sample_distances(NULL, 100, 1);
    AIL_set_3D_sample_occlusion(NULL, 0.5f);
    AIL_set_3D_user_data(NULL, 0, -7);
    CHECK(AIL_3D_user_data(NULL, 0) == 0);
    CHECK(AIL_register_3D_EOS_callback(NULL, sample3d_done) == NULL);
    CHECK(AIL_register_3D_EOS_callback(NULL, NULL) == NULL);
    CHECK(AIL_open_3D_listener(0) == NULL);
    AIL_set_3D_position(NULL, 1, 2, 3);
    AIL_set_3D_orientation(NULL, 1, 0, 0, 0, 1, 0);
    AIL_set_3D_velocity_vector(NULL, 1, 2, 3);
    AIL_close_3D_listener(NULL);
    AIL_release_3D_sample_handle(NULL);

    CHECK(AIL_open_stream(NULL, NULL, 0) == NULL);
    CHECK(AIL_open_stream(NULL, "", 0) == NULL);
    CHECK(AIL_open_stream(NULL, "unavailable-stub-audio.wav", 0) == NULL);
    AIL_start_stream(NULL); AIL_pause_stream(NULL, 1);
    AIL_set_stream_loop_count(NULL, 3);
    CHECK(AIL_stream_loop_count(NULL) == 0);
    AIL_set_stream_volume_pan(NULL, 1, 0.5f);
    volume = -1; pan = -1;
    AIL_stream_volume_pan(NULL, &volume, &pan);
    CHECK(volume == 0 && pan == 0);
    AIL_stream_volume_pan(NULL, NULL, NULL);
    AIL_stream_ms_position(NULL, &position.total, &position.current);
    CHECK(position.total == 0 && position.current == 0);
    CHECK(position.canary == 0xabcdef01U);
    AIL_stream_ms_position(NULL, NULL, NULL);
    CHECK(AIL_register_stream_callback(NULL, stream_done) == NULL);
    CHECK(AIL_register_stream_callback(NULL, NULL) == NULL);
    AIL_close_stream(NULL);
    CHECK(AIL_quick_load_and_play(NULL, 1, 0) == NULL);
    CHECK(AIL_quick_load_and_play("", 1, 0) == NULL);
    CHECK(AIL_quick_load_and_play("unavailable-stub-audio.wav", 1, 0) == NULL);
    AIL_quick_set_volume(NULL, 1, 1);
    AIL_quick_unload(NULL);

    memset(&info, 0xa5, sizeof(info));
    CHECK(AIL_WAV_info(NULL, &info) == 0);
    CHECK(info.format == 0 && info.data_ptr == NULL && info.data_len == 0);
    CHECK(info.rate == 0 && info.bits == 0 && info.channels == 0);
    CHECK(info.samples == 0 && info.block_size == 0 && info.initial_ptr == NULL);
    CHECK(AIL_WAV_info(marker, NULL) == 0);
    CHECK(AIL_decompress_ADPCM(&info, &decoded, &decoded_size) == 0);
    CHECK(decoded == NULL && decoded_size == 0);
    CHECK(AIL_decompress_ADPCM(NULL, NULL, NULL) == 0);
    AIL_mem_free_lock(NULL);
    /* This public subset exposes free, not alloc. Native stub uses free(3). */
    memory = (unsigned char *)malloc(257);
    CHECK(memory != NULL);
    memset(memory, 0xa5, 257);
    CHECK(memory[0] == 0xa5 && memory[256] == 0xa5);
    AIL_mem_free_lock(memory);
    CHECK(sentinel == 42 && strcmp(marker, "unchanged") == 0);
    AIL_shutdown(); AIL_shutdown();
    puts("Miles public headers: linked, unavailable, inert, malloc/free paired");
    return 0;
}
