/* Native stub contract, not the real Bink DLL/decoder test. */
#include <bink.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static int BINKCALL sound_open(BINKSNDSYSPARAM param)
{
    (void)param;
    /* The unavailable stub must not start the caller's sound system. */
    return 1;
}

int main(void)
{
    unsigned char pixels[32], expected[32];
    unsigned int tracks[] = {0, 3};
    CHECK(sizeof(u8) == 1 && sizeof(u16) == 2);
    CHECK(sizeof(u32) == 4 && sizeof(s32) == 4);
    CHECK(offsetof(BINK, FrameRateDiv) == 6 * sizeof(u32));
    CHECK(BinkOpen(NULL, 0) == NULL);
    CHECK(BinkOpen("", BINKPRELOADALL) == NULL);
    CHECK(BinkOpen("unavailable-stub-movie.bik", 0) == NULL);
    BinkClose(NULL);
    CHECK(BinkWait(NULL) == 0);
    CHECK(BinkDoFrame(NULL) == 0);
    BinkNextFrame(NULL);
    CHECK(BinkGoto(NULL, 1, 0) == 0);
    memset(pixels, 0xa5, sizeof(pixels));
    memcpy(expected, pixels, sizeof(pixels));
    CHECK(BinkCopyToBuffer(NULL, pixels, 16, 2, 0, 0,
                          BINKSURFACE32 | BINKCOPYALL) == 0);
    CHECK(memcmp(pixels, expected, sizeof(pixels)) == 0);
    CHECK(BinkCopyToBuffer(NULL, NULL, 0, 0, 0, 0, 0) == 0);
    CHECK(BinkSetVolume(NULL, 0, 32768) == 0);
    BinkSetSoundTrack(0, NULL);
    BinkSetSoundTrack(2, tracks);
    CHECK(tracks[0] == 0 && tracks[1] == 3);
    CHECK(BinkSetSoundSystem(NULL, 0) == 0);
    CHECK(BinkSetSoundSystem(sound_open, 0) == 0);
    CHECK(BinkOpenDirectSound(0) == 0);
    BinkClose(NULL);
    puts("Bink public header: linked, unavailable, inert");
    return 0;
}
