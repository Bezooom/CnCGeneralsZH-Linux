#include "PAL_Font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>

namespace {

const uint32_t GDI_MAGIC_FONT   = 0x464F4E54; // 'FONT'
const uint32_t GDI_MAGIC_BITMAP = 0x4249544D; // 'BITM'
const uint32_t GDI_MAGIC_DC     = 0x44434443; // 'DCDC'

struct GdiObjHeader {
    uint32_t magic;
};

struct PAL_Font : GdiObjHeader {
    FT_Face face;
    FT_Face fallbackFace;
    int pixelHeight;
    int charHeight;
    int charAscent;
    int charDescent;
    bool isBold;
    char faceName[64];
};

struct PAL_Bitmap : GdiObjHeader {
    void *bits;
    int width;
    int height;
    int stride;
    bool ownsBits;
};

struct PAL_DC : GdiObjHeader {
    PAL_Font *currentFont;
    PAL_Bitmap *currentBitmap;
};

static FT_Library s_ftLibrary = nullptr;

static bool EnsureFT(void) {
    if (!s_ftLibrary) {
        if (FT_Init_FreeType(&s_ftLibrary) != 0) {
            return false;
        }
    }
    return true;
}

static const char *ChooseFontFile(const char *faceName, bool bold) {
    if (bold) {
        if (access("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", R_OK) == 0)
            return "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf";
        if (access("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", R_OK) == 0)
            return "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    } else {
        if (access("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", R_OK) == 0)
            return "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
        if (access("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", R_OK) == 0)
            return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    }

    const char *fallbacks[] = {
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        nullptr
    };
    for (int i = 0; fallbacks[i]; ++i) {
        if (access(fallbacks[i], R_OK) == 0) return fallbacks[i];
    }
    return nullptr;
}

static const char *ChooseFallbackFontFile(bool bold) {
    if (bold) {
        if (access("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", R_OK) == 0)
            return "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    } else {
        if (access("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", R_OK) == 0)
            return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    }
    return nullptr;
}

} // namespace

extern "C" {

HFONT PAL_CreateFont(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight,
                     DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet,
                     DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality,
                     DWORD fdwPitchAndFamily, LPCSTR lpszFace)
{
    if (!EnsureFT()) return nullptr;

    bool bold = (fnWeight >= 700);
    const char *fontPath = ChooseFontFile(lpszFace, bold);
    if (!fontPath) return nullptr;

    FT_Face face = nullptr;
    if (FT_New_Face(s_ftLibrary, fontPath, 0, &face) != 0) {
        return nullptr;
    }

    int px = nHeight < 0 ? -nHeight : nHeight;
    if (px <= 0) px = 12;

    if (FT_Set_Pixel_Sizes(face, 0, px) != 0) {
        FT_Done_Face(face);
        return nullptr;
    }

    PAL_Font *f = new PAL_Font();
    f->magic = GDI_MAGIC_FONT;
    f->face = face;
    f->fallbackFace = nullptr;
    f->pixelHeight = px;
    f->isBold = bold;
    if (lpszFace) {
        strncpy(f->faceName, lpszFace, sizeof(f->faceName) - 1);
        f->faceName[sizeof(f->faceName) - 1] = 0;
    } else {
        f->faceName[0] = 0;
    }

    const char *fallbackPath = ChooseFallbackFontFile(bold);
    if (fallbackPath && strcmp(fallbackPath, fontPath) != 0) {
        FT_Face fb = nullptr;
        if (FT_New_Face(s_ftLibrary, fallbackPath, 0, &fb) == 0) {
            if (FT_Set_Pixel_Sizes(fb, 0, px) == 0) {
                f->fallbackFace = fb;
            } else {
                FT_Done_Face(fb);
            }
        }
    }

    f->charHeight = (int)(face->size->metrics.height >> 6);
    if (f->charHeight <= 0) f->charHeight = px;
    f->charAscent = (int)(face->size->metrics.ascender >> 6);
    if (f->charAscent <= 0) f->charAscent = (px * 3) / 4;
    f->charDescent = (int)(-(face->size->metrics.descender >> 6));
    if (f->charDescent < 0) f->charDescent = 0;

    return (HFONT)f;
}

HDC PAL_CreateCompatibleDC(HDC) {
    PAL_DC *dc = new PAL_DC();
    dc->magic = GDI_MAGIC_DC;
    dc->currentFont = nullptr;
    dc->currentBitmap = nullptr;
    return (HDC)dc;
}

HBITMAP PAL_CreateDIBSection(HDC, const BITMAPINFO *pbmi, UINT, void **ppvBits, HANDLE, DWORD) {
    int w = (pbmi && pbmi->bmiHeader.biWidth > 0) ? pbmi->bmiHeader.biWidth : 256;
    int h = (pbmi && pbmi->bmiHeader.biHeight != 0) ? abs(pbmi->bmiHeader.biHeight) : 256;
    int stride = (((w * 3) + 3) & ~3);

    void *bits = calloc(1, (size_t)stride * h + 1024);
    if (ppvBits) {
        *ppvBits = bits;
    }

    PAL_Bitmap *bm = new PAL_Bitmap();
    bm->magic = GDI_MAGIC_BITMAP;
    bm->bits = bits;
    bm->width = w;
    bm->height = h;
    bm->stride = stride;
    bm->ownsBits = true;
    return (HBITMAP)bm;
}

HGDIOBJ PAL_SelectObject(HDC hdc, HGDIOBJ hgdiobj) {
    PAL_DC *dc = (PAL_DC *)hdc;
    if (!dc || dc->magic != GDI_MAGIC_DC || !hgdiobj) return nullptr;

    GdiObjHeader *hdr = (GdiObjHeader *)hgdiobj;
    if (hdr->magic == GDI_MAGIC_FONT) {
        PAL_Font *prev = dc->currentFont;
        dc->currentFont = (PAL_Font *)hgdiobj;
        return (HGDIOBJ)prev;
    } else if (hdr->magic == GDI_MAGIC_BITMAP) {
        PAL_Bitmap *prev = dc->currentBitmap;
        dc->currentBitmap = (PAL_Bitmap *)hgdiobj;
        return (HGDIOBJ)prev;
    }
    return nullptr;
}

BOOL PAL_DeleteObject(HGDIOBJ hObject) {
    if (!hObject) return TRUE;
    GdiObjHeader *hdr = (GdiObjHeader *)hObject;
    if (hdr->magic == GDI_MAGIC_FONT) {
        PAL_Font *f = (PAL_Font *)hObject;
        if (f->face) {
            FT_Done_Face(f->face);
            f->face = nullptr;
        }
        if (f->fallbackFace) {
            FT_Done_Face(f->fallbackFace);
            f->fallbackFace = nullptr;
        }
        f->magic = 0;
        delete f;
        return TRUE;
    } else if (hdr->magic == GDI_MAGIC_BITMAP) {
        PAL_Bitmap *bm = (PAL_Bitmap *)hObject;
        if (bm->ownsBits && bm->bits) {
            free(bm->bits);
            bm->bits = nullptr;
        }
        bm->magic = 0;
        delete bm;
        return TRUE;
    }
    return TRUE;
}

BOOL PAL_DeleteDC(HDC hdc) {
    if (!hdc) return TRUE;
    PAL_DC *dc = (PAL_DC *)hdc;
    if (dc->magic == GDI_MAGIC_DC) {
        dc->magic = 0;
        delete dc;
    }
    return TRUE;
}

BOOL PAL_GetTextMetrics(HDC hdc, TEXTMETRICA *tm) {
    if (!tm) return FALSE;
    memset(tm, 0, sizeof(*tm));
    PAL_DC *dc = (PAL_DC *)hdc;
    if (!dc || dc->magic != GDI_MAGIC_DC || !dc->currentFont) {
        tm->tmHeight = 16;
        tm->tmAscent = 12;
        return TRUE;
    }
    PAL_Font *f = dc->currentFont;
    tm->tmHeight = f->charHeight;
    tm->tmAscent = f->charAscent;
    tm->tmDescent = f->charDescent;
    tm->tmOverhang = 0;
    return TRUE;
}

BOOL PAL_GetTextExtentPoint32W(HDC hdc, const wchar_t *lpString, int c, SIZE *psz) {
    if (!psz) return FALSE;
    psz->cx = 0;
    psz->cy = 16;

    PAL_DC *dc = (PAL_DC *)hdc;
    if (!dc || dc->magic != GDI_MAGIC_DC || !dc->currentFont || !lpString || c <= 0) {
        psz->cx = 8 * c;
        return TRUE;
    }

    PAL_Font *f = dc->currentFont;
    psz->cy = f->charHeight;

    int totalAdv = 0;
    for (int i = 0; i < c; ++i) {
        FT_Face faceToUse = f->face;
        FT_UInt glyph_index = FT_Get_Char_Index(faceToUse, (FT_ULong)lpString[i]);
        if (glyph_index == 0 && f->fallbackFace) {
            FT_UInt fbIndex = FT_Get_Char_Index(f->fallbackFace, (FT_ULong)lpString[i]);
            if (fbIndex != 0) {
                faceToUse = f->fallbackFace;
                glyph_index = fbIndex;
            }
        }

        if (FT_Load_Glyph(faceToUse, glyph_index, FT_LOAD_DEFAULT) == 0) {
            totalAdv += (int)(faceToUse->glyph->advance.x >> 6);
        } else {
            totalAdv += (f->pixelHeight / 2);
        }
    }
    psz->cx = totalAdv;
    return TRUE;
}

BOOL PAL_ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT *lprc,
                     const wchar_t *lpString, UINT c, const int *)
{
    PAL_DC *dc = (PAL_DC *)hdc;
    if (!dc || dc->magic != GDI_MAGIC_DC || !dc->currentBitmap || !dc->currentBitmap->bits) {
        return FALSE;
    }

    PAL_Bitmap *bm = dc->currentBitmap;

    if (lprc) {
        int rTop = lprc->top < 0 ? 0 : lprc->top;
        int rBottom = lprc->bottom > bm->height ? bm->height : lprc->bottom;
        int rLeft = lprc->left < 0 ? 0 : lprc->left;
        int rRight = lprc->right > bm->width ? bm->width : lprc->right;
        for (int r = rTop; r < rBottom; ++r) {
            uint8_t *rowPtr = (uint8_t *)bm->bits + r * bm->stride;
            memset(rowPtr + rLeft * 3, 0, (rRight - rLeft) * 3);
        }
    } else {
        memset(bm->bits, 0, (size_t)bm->stride * bm->height);
    }

    if (!dc->currentFont || !dc->currentFont->face || !lpString || c == 0) {
        return TRUE;
    }

    PAL_Font *f = dc->currentFont;

    int curX = x;
    for (UINT i = 0; i < c; ++i) {
        FT_Face faceToUse = f->face;
        FT_UInt glyph_index = FT_Get_Char_Index(faceToUse, (FT_ULong)lpString[i]);
        if (glyph_index == 0 && f->fallbackFace) {
            FT_UInt fbIndex = FT_Get_Char_Index(f->fallbackFace, (FT_ULong)lpString[i]);
            if (fbIndex != 0) {
                faceToUse = f->fallbackFace;
                glyph_index = fbIndex;
            }
        }

        if (FT_Load_Glyph(faceToUse, glyph_index, FT_LOAD_RENDER) != 0) {
            continue;
        }

        FT_GlyphSlot slot = faceToUse->glyph;
        int bmpLeft = slot->bitmap_left;
        int bmpTop = slot->bitmap_top;
        int bmpWidth = slot->bitmap.width;
        int bmpRows = slot->bitmap.rows;
        int bmpPitch = slot->bitmap.pitch;
        const uint8_t *bmpBuf = slot->bitmap.buffer;

        int startY = y + f->charAscent - bmpTop;
        int startX = curX + bmpLeft;

        for (int r = 0; r < bmpRows; ++r) {
            int destY = startY + r;
            if (destY < 0 || destY >= bm->height) continue;

            uint8_t *destRow = (uint8_t *)bm->bits + destY * bm->stride;
            const uint8_t *srcRow = bmpBuf + r * bmpPitch;

            for (int col = 0; col < bmpWidth; ++col) {
                int destX = startX + col;
                if (destX < 0 || destX >= bm->width) continue;

                uint8_t val = srcRow[col];
                if (val > 0) {
                    int dstIdx = destX * 3;
                    if (val > destRow[dstIdx]) {
                        destRow[dstIdx + 0] = val;
                        destRow[dstIdx + 1] = val;
                        destRow[dstIdx + 2] = val;
                    }
                }
            }
        }

        curX += (int)(slot->advance.x >> 6);
    }

    return TRUE;
}

} // extern "C"
