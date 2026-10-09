/*
 * No-op stand-in for binkw32.dll.  Nothing here ever runs: it exists only so
 * the linker can produce an import library whose symbols carry the decorated
 * names the shipped BINKW32.DLL exports, which is what generals.exe binds to
 * at run time.  (`lib /def` alone cannot make that import library - it prefixes
 * a second underscore onto the already-decorated names.)
 *
 * The stub DLL built from this file must never be copied next to generals.exe.
 */
#include "bink.h"

HBINK BINKCALL BinkOpen(const char *name, unsigned int flags) { return 0; }
void  BINKCALL BinkClose(HBINK bnk) {}
int   BINKCALL BinkWait(HBINK bnk) { return 0; }
int   BINKCALL BinkDoFrame(HBINK bnk) { return 0; }
void  BINKCALL BinkNextFrame(HBINK bnk) {}
int   BINKCALL BinkGoto(HBINK bnk, unsigned int frame, int flags) { return 0; }
int   BINKCALL BinkCopyToBuffer(HBINK bnk, void *dest, int destpitch,
                                 unsigned int destheight, unsigned int destx,
                                 unsigned int desty, unsigned int flags) { return 0; }
int   BINKCALL BinkSetVolume(HBINK bnk, unsigned int trackid, int volume) { return 0; }
void  BINKCALL BinkSetSoundTrack(unsigned int total_tracks, unsigned int *tracks) {}
int   BINKCALL BinkSetSoundSystem(BINKSNDSYSOPEN open, BINKSNDSYSPARAM param) { return 0; }
int   BINKCALL BinkOpenDirectSound(BINKSNDSYSPARAM param) { return 0; }
