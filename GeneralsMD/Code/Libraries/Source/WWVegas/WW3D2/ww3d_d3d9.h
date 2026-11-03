// WW3D consumer declarations, not a Direct3D implementation.
// Windows uses the SDK ABI. Other hosts cannot call methods on the opaque
// interfaces: resource/device operations still require a real renderer backend.
#ifndef WW3D_D3D9_H
#define WW3D_D3D9_H
#ifdef _WIN32
#include <d3d9.h>
#else
#include "win.h"
#include "ffshader_d3d.h"
#include <execinfo.h>
#include <map>
#include <cstdio>
#include <vector>
#include <cmath>
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
#include "PAL_Window.h"
#include "PAL_Vulkan.h"
#include "vkvertexformat.h"
#endif

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3DVolumeTexture9;
struct IDirect3DSurface9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DVertexDeclaration9;
struct IDirect3DSwapChain9;
struct D3DCAPS9;
struct D3DADAPTER_IDENTIFIER9;
struct D3DDISPLAYMODE;
struct D3DPRESENT_PARAMETERS;
using LPDIRECT3DDEVICE9 = IDirect3DDevice9*;
using LPDIRECT3DBASETEXTURE9 = IDirect3DBaseTexture9*;
using LPDIRECT3DTEXTURE9 = IDirect3DTexture9*;
using LPDIRECT3DCUBETEXTURE9 = IDirect3DCubeTexture9*;
using LPDIRECT3DVOLUMETEXTURE9 = IDirect3DVolumeTexture9*;
using LPDIRECT3DSURFACE9 = IDirect3DSurface9*;
using LPDIRECT3DVERTEXBUFFER9 = IDirect3DVertexBuffer9*;
using LPDIRECT3DINDEXBUFFER9 = IDirect3DIndexBuffer9*;
using D3DCOLOR = DWORD;
using D3DTEXTUREOP = DWORD;
using D3DCMPFUNC = DWORD;
// Distinct enum types are important: texture-stage and sampler overloads
// share numeric values but do not address the same state cache.
enum D3DTRANSFORMSTATETYPE : unsigned { D3DTS_VIEW=2, D3DTS_PROJECTION=3,
 D3DTS_TEXTURE0=16, D3DTS_TEXTURE1=17, D3DTS_TEXTURE2=18, D3DTS_TEXTURE3=19, D3DTS_WORLD=256 };
enum D3DPOOL : unsigned { D3DPOOL_DEFAULT=0, D3DPOOL_MANAGED=1, D3DPOOL_SYSTEMMEM=2, D3DPOOL_SCRATCH=3 };
enum D3DPRIMITIVETYPE : unsigned { D3DPT_POINTLIST=1, D3DPT_LINELIST=2, D3DPT_LINESTRIP=3, D3DPT_TRIANGLELIST=4, D3DPT_TRIANGLESTRIP=5, D3DPT_TRIANGLEFAN=6 };
enum D3DFORMAT : unsigned { D3DFMT_UNKNOWN=0, D3DFMT_R8G8B8=20,
 D3DFMT_A8R8G8B8=21, D3DFMT_X8R8G8B8=22, D3DFMT_R5G6B5=23,
 D3DFMT_X1R5G5B5=24, D3DFMT_A1R5G5B5=25, D3DFMT_A4R4G4B4=26,
 D3DFMT_R3G3B2=27, D3DFMT_A8=28, D3DFMT_A8R3G3B2=29, D3DFMT_X4R4G4B4=30,
 D3DFMT_A2B10G10R10=31, D3DFMT_G16R16=34,
 D3DFMT_A8P8=40, D3DFMT_P8=41, D3DFMT_L8=50, D3DFMT_A8L8=51, D3DFMT_A4L4=52,
 D3DFMT_V8U8=60, D3DFMT_L6V5U5=61, D3DFMT_X8L8V8U8=62, D3DFMT_Q8W8V8U8=63, D3DFMT_V16U16=64,
 D3DFMT_UYVY=0x59565955, D3DFMT_YUY2=0x32595559,
 D3DFMT_DXT1=0x31545844, D3DFMT_DXT2=0x32545844, D3DFMT_DXT3=0x33545844,
 D3DFMT_DXT4=0x34545844, D3DFMT_DXT5=0x35545844,
 D3DFMT_D16_LOCKABLE=70, D3DFMT_D32=71, D3DFMT_D15S1=73, D3DFMT_D24S8=75,
 D3DFMT_D24X8=77, D3DFMT_D24X4S4=79, D3DFMT_D16=80, D3DFMT_VERTEXDATA=100, D3DFMT_INDEX16=101, D3DFMT_INDEX32=102 };
enum D3DRESOURCETYPE : unsigned { D3DRTYPE_SURFACE=1, D3DRTYPE_VOLUME=2, D3DRTYPE_TEXTURE=3,
 D3DRTYPE_VOLUMETEXTURE=4, D3DRTYPE_CUBETEXTURE=5, D3DRTYPE_VERTEXBUFFER=6, D3DRTYPE_INDEXBUFFER=7 };
enum D3DMULTISAMPLE_TYPE : unsigned { D3DMULTISAMPLE_NONE=0, D3DMULTISAMPLE_NONMASKABLE=1 };
enum D3DDEVTYPE : unsigned { D3DDEVTYPE_HAL=1, D3DDEVTYPE_REF=2, D3DDEVTYPE_SW=3, D3DDEVTYPE_NULLREF=4 };
enum D3DLIGHTTYPE : unsigned { D3DLIGHT_POINT=1, D3DLIGHT_SPOT=2, D3DLIGHT_DIRECTIONAL=3 };
enum D3DRENDERSTATETYPE : unsigned {
 D3DRS_ZENABLE=7, D3DRS_FILLMODE=8, D3DRS_SHADEMODE=9,
 D3DRS_ZWRITEENABLE=14, D3DRS_ALPHATESTENABLE=15, D3DRS_LASTPIXEL=16,
 D3DRS_SRCBLEND=19, D3DRS_DESTBLEND=20,
 D3DRS_CULLMODE=22, D3DRS_ZFUNC=23, D3DRS_ALPHAREF=24, D3DRS_ALPHAFUNC=25,
 D3DRS_DITHERENABLE=26, D3DRS_ALPHABLENDENABLE=27, D3DRS_FOGENABLE=28, D3DRS_SPECULARENABLE=29,
 D3DRS_FOGCOLOR=34, D3DRS_FOGTABLEMODE=35, D3DRS_FOGSTART=36, D3DRS_FOGEND=37,
 D3DRS_FOGDENSITY=38, D3DRS_RANGEFOGENABLE=48,
 D3DRS_STENCILENABLE=52, D3DRS_STENCILFAIL=53, D3DRS_STENCILZFAIL=54, D3DRS_STENCILPASS=55,
 D3DRS_STENCILFUNC=56, D3DRS_STENCILREF=57, D3DRS_STENCILMASK=58, D3DRS_STENCILWRITEMASK=59,
 D3DRS_TEXTUREFACTOR=60,
 D3DRS_WRAP0=128, D3DRS_WRAP1=129, D3DRS_WRAP2=130, D3DRS_WRAP3=131,
 D3DRS_WRAP4=132, D3DRS_WRAP5=133, D3DRS_WRAP6=134, D3DRS_WRAP7=135,
 D3DRS_CLIPPING=136, D3DRS_LIGHTING=137, D3DRS_AMBIENT=139, D3DRS_FOGVERTEXMODE=140, D3DRS_COLORVERTEX=141,
 D3DRS_LOCALVIEWER=142, D3DRS_NORMALIZENORMALS=143,
 D3DRS_DIFFUSEMATERIALSOURCE=145, D3DRS_SPECULARMATERIALSOURCE=146, D3DRS_AMBIENTMATERIALSOURCE=147, D3DRS_EMISSIVEMATERIALSOURCE=148,
 D3DRS_VERTEXBLEND=151, D3DRS_CLIPPLANEENABLE=152,
 D3DRS_POINTSIZE=154, D3DRS_POINTSIZE_MIN=155, D3DRS_POINTSPRITEENABLE=156,
 D3DRS_POINTSCALEENABLE=157, D3DRS_POINTSCALE_A=158, D3DRS_POINTSCALE_B=159, D3DRS_POINTSCALE_C=160,
 D3DRS_MULTISAMPLEANTIALIAS=161, D3DRS_MULTISAMPLEMASK=162, D3DRS_PATCHEDGESTYLE=163,
 D3DRS_DEBUGMONITORTOKEN=165, D3DRS_POINTSIZE_MAX=166,
 D3DRS_INDEXEDVERTEXBLENDENABLE=167, D3DRS_COLORWRITEENABLE=168,
 D3DRS_TWEENFACTOR=170, D3DRS_BLENDOP=171,
 D3DRS_SLOPESCALEDEPTHBIAS=175, D3DRS_DEPTHBIAS=195 };
enum D3DTEXTURESTAGESTATETYPE : unsigned { D3DTSS_COLOROP=1, D3DTSS_COLORARG1=2,
  D3DTSS_COLORARG2=3, D3DTSS_ALPHAOP=4, D3DTSS_ALPHAARG1=5, D3DTSS_ALPHAARG2=6,
  D3DTSS_BUMPENVMAT00=7, D3DTSS_BUMPENVMAT01=8, D3DTSS_BUMPENVMAT10=9,
  D3DTSS_BUMPENVMAT11=10, D3DTSS_TEXCOORDINDEX=11, D3DTSS_BUMPENVLSCALE=22,
  D3DTSS_BUMPENVLOFFSET=23, D3DTSS_TEXTURETRANSFORMFLAGS=24,
  D3DTSS_COLORARG0=26, D3DTSS_ALPHAARG0=27, D3DTSS_RESULTARG=28, D3DTSS_CONSTANT=32 };
enum : DWORD {
  D3DTSS_TCI_PASSTHRU = 0x00000000,
  D3DTSS_TCI_CAMERASPACENORMAL = 0x00010000,
  D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR = 0x00030000,
  D3DTSS_TCI_SPHEREMAP = 0x00040000
};
enum D3DSAMPLERSTATETYPE : unsigned { D3DSAMP_ADDRESSU=1, D3DSAMP_ADDRESSV=2,
 D3DSAMP_ADDRESSW=3, D3DSAMP_BORDERCOLOR=4, D3DSAMP_MAGFILTER=5,
 D3DSAMP_MINFILTER=6, D3DSAMP_MIPFILTER=7, D3DSAMP_MIPMAPLODBIAS=8,
 D3DSAMP_MAXMIPLEVEL=9, D3DSAMP_MAXANISOTROPY=10, D3DSAMP_SRGBTEXTURE=11,
 D3DSAMP_ELEMENTINDEX=12, D3DSAMP_DMAPOFFSET=13 };

#ifndef _D3DVECTOR_DEFINED
#define _D3DVECTOR_DEFINED
struct D3DVECTOR { float x, y, z; };
#endif
struct D3DCOLORVALUE { float r, g, b, a; };
struct D3DMATERIAL9 { D3DCOLORVALUE Diffuse, Ambient, Specular, Emissive; float Power; };
struct D3DLIGHT9 { D3DLIGHTTYPE Type; D3DCOLORVALUE Diffuse, Specular, Ambient;
 D3DVECTOR Position, Direction; float Range, Falloff, Attenuation0, Attenuation1, Attenuation2, Theta, Phi; };
#ifndef _D3DMATRIX_DEFINED
#define _D3DMATRIX_DEFINED
struct D3DMATRIX { union { float m[4][4]; struct {
 float _11,_12,_13,_14,_21,_22,_23,_24,_31,_32,_33,_34,_41,_42,_43,_44;
 }; }; };
#endif
struct D3DVIEWPORT9 { DWORD X,Y,Width,Height; float MinZ,MaxZ; };
struct D3DLOCKED_RECT { int Pitch; void* pBits; };
struct D3DSURFACE_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool;
 D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; UINT Width,Height; };

#ifndef CONST
#define CONST const
#endif
#ifndef _RECT_DEFINED
#define _RECT_DEFINED
typedef struct tagRECT { int32_t left, top, right, bottom; } RECT;
#endif
#ifndef _POINT_DEFINED
#define _POINT_DEFINED
typedef struct tagPOINT { int32_t x, y; } POINT;
#endif
constexpr unsigned D3DDP_MAXTEXCOORD=8;
constexpr DWORD D3D_OK=0;
constexpr DWORD D3DFVF_XYZ=0x002, D3DFVF_XYZRHW=0x004, D3DFVF_NORMAL=0x010,
 D3DFVF_DIFFUSE=0x040, D3DFVF_SPECULAR=0x080, D3DFVF_TEX0=0x000,
 D3DFVF_TEX1=0x100, D3DFVF_TEX2=0x200, D3DFVF_TEX3=0x300, D3DFVF_TEX4=0x400,
 D3DFVF_TEXCOUNT_MASK=0xf00, D3DFVF_TEXCOUNT_SHIFT=8, D3DFVF_POSITION_MASK=0x400e,
 D3DFVF_PSIZE=0x020;
#define D3DFVF_TEXCOORDSIZE1(i) (3u << (16u + (i)*2u))
#define D3DFVF_TEXCOORDSIZE2(i) (0u)
#define D3DFVF_TEXCOORDSIZE3(i) (1u << (16u + (i)*2u))
#define D3DFVF_TEXCOORDSIZE4(i) (2u << (16u + (i)*2u))
#define D3DFVF_TEXTUREFORMAT2 0

// D3D9 constants not covered by stub enums (used by W3DDevice code on Linux)
enum D3DCULL : unsigned {
  D3DCULL_NONE = 1,
  D3DCULL_CW = 2,
  D3DCULL_CCW = 3
};

enum D3DBLEND : unsigned {
  D3DBLEND_ZERO = 1,
  D3DBLEND_ONE = 2,
  D3DBLEND_SRCCOLOR = 3,
  D3DBLEND_INVSRCCOLOR = 4,
  D3DBLEND_SRCALPHA = 5,
  D3DBLEND_INVSRCALPHA = 6,
  D3DBLEND_DESTALPHA = 7,
  D3DBLEND_INVDESTALPHA = 8,
  D3DBLEND_DESTCOLOR = 9,
  D3DBLEND_INVDESTCOLOR = 10,
  D3DBLEND_SRCALPHASAT = 11,
  D3DBLEND_BOTHSRCALPHA = 12,
  D3DBLEND_BOTHINVSRCALPHA = 13
};

enum D3DBLENDOP : unsigned {
  D3DBLENDOP_ADD = 1,
  D3DBLENDOP_SUBTRACT = 2,
  D3DBLENDOP_REVSUBTRACT = 3,
  D3DBLENDOP_MIN = 4,
  D3DBLENDOP_MAX = 5
};

enum D3DSTENCILOP : unsigned {
  D3DSTENCILOP_KEEP = 1,
  D3DSTENCILOP_ZERO = 2,
  D3DSTENCILOP_REPLACE = 3,
  D3DSTENCILOP_INCRSAT = 4,
  D3DSTENCILOP_DECRSAT = 5,
  D3DSTENCILOP_INVERT = 6,
  D3DSTENCILOP_INCR = 7,
  D3DSTENCILOP_DECR = 8
};

enum D3DZBUFFERTYPE : unsigned {
  D3DZB_FALSE = 0,
  D3DZB_TRUE = 1,
  D3DZB_USEW = 2
};

enum D3DSHADEMODE : unsigned {
  D3DSHADE_FLAT = 1,
  D3DSHADE_GOURAUD = 2,
  D3DSHADE_PHONG = 3
};

enum D3DFOGMODE : unsigned {
  D3DFOG_NONE = 0,
  D3DFOG_EXP = 1,
  D3DFOG_EXP2 = 2,
  D3DFOG_LINEAR = 3
};

enum D3DTEXTUREADDRESS : unsigned {
  D3DTADDRESS_WRAP = 1,
  D3DTADDRESS_MIRROR = 2,
  D3DTADDRESS_CLAMP = 3,
  D3DTADDRESS_BORDER = 4,
  D3DTADDRESS_MIRRORONCE = 5
};

enum D3DTEXTUREFILTERTYPE : unsigned {
  D3DTEXF_NONE = 0,
  D3DTEXF_POINT = 1,
  D3DTEXF_LINEAR = 2,
  D3DTEXF_ANISOTROPIC = 3,
  D3DTEXF_PYRAMIDALQUAD = 6,
  D3DTEXF_GAUSSIANQUAD = 7
};

enum D3DVERTEXBLENDFLAGS : unsigned {
  D3DVBF_DISABLE = 0,
  D3DVBF_1WEIGHTS = 1,
  D3DVBF_2WEIGHTS = 2,
  D3DVBF_3WEIGHTS = 3,
  D3DVBF_TWEENING = 255,
  D3DVBF_0WEIGHTS = 256
};

enum D3DPATCHEDGESTYLE : unsigned {
  D3DPATCHEDGE_DISCRETE = 0,
  D3DPATCHEDGE_CONTINUOUS = 1
};

enum D3DDEBUGMONITORTOKENS : unsigned {
  D3DDMT_ENABLE = 0,
  D3DDMT_DISABLE = 1
};

enum D3DBACKBUFFER_TYPE : unsigned {
  D3DBACKBUFFER_TYPE_MONO = 0,
  D3DBACKBUFFER_TYPE_LEFT = 1,
  D3DBACKBUFFER_TYPE_RIGHT = 2
};

enum : DWORD {
  D3DCOLORWRITEENABLE_RED = 0x8,
  D3DCOLORWRITEENABLE_GREEN = 0x4,
  D3DCOLORWRITEENABLE_BLUE = 0x2,
  D3DCOLORWRITEENABLE_ALPHA = 0x1,
  D3DLOCK_READONLY = 0x00000010,
  D3DLOCK_DISCARD = 0x00002000,
  D3DLOCK_NOOVERWRITE = 0x00001000,
  D3DLOCK_NOSYSLOCK = 0x00000800,
  D3DLOCK_NO_DIRTY_UPDATE = 0x00004000,
  D3DUSAGE_DEPTHSTENCIL = 0x00000002,
  D3DUSAGE_WRITEONLY = 0x00000008,
  D3DUSAGE_SOFTWAREPROCESSING = 0x00000010,
  D3DUSAGE_DONOTCLIP = 0x00000020,
  D3DUSAGE_POINTS = 0x00000040,
  D3DUSAGE_RTPATCHES = 0x00000080,
  D3DUSAGE_NPATCHES = 0x00000100,
  D3DUSAGE_DYNAMIC = 0x00000200,
  D3DUSAGE_AUTOGENMIPMAP = 0x00000400,
  D3DUSAGE_RENDERTARGET = 0x00000001,
  D3DCURSOR_IMMEDIATE_UPDATE = 0x8000,
  D3DWRAP_U = 0x00000001,
  D3DWRAP_V = 0x00000002,
  D3DWRAP_W = 0x00000004,
  D3DTTFF_DISABLE = 0,
  D3DTTFF_COUNT1 = 1,
  D3DTTFF_COUNT2 = 2,
  D3DTTFF_COUNT3 = 3,
  D3DTTFF_COUNT4 = 4,
  D3DTTFF_PROJECTED = 256,
  D3DCLEAR_TARGET = 0x00000001,
  D3DCLEAR_ZBUFFER = 0x00000002,
  D3DCLEAR_STENCIL = 0x00000004,
  D3DFILL_POINT = 1,
  D3DFILL_WIREFRAME = 2,
  D3DFILL_SOLID = 3,
  D3DFVF_XYZB1 = 0x006,
  D3DFVF_XYZB2 = 0x008,
  D3DFVF_XYZB3 = 0x00a,
  D3DFVF_XYZB4 = 0x00c,
  D3DFVF_XYZB5 = 0x00e,
  D3DFVF_LASTBETA_UBYTE4 = 0x1000,
  D3DFVF_LASTBETA_D3DCOLOR = 0x8000,
  D3DFVF_TEX5 = 0x0500,
  D3DFVF_TEX6 = 0x0600,
  D3DFVF_TEX7 = 0x0700,
  D3DFVF_TEX8 = 0x0800,
  D3DTOP_PREMODULATE = 17,
  D3DTOP_BUMPENVMAPLUMINANCE = 23,
  D3DTA_TEMP = 0x5,
  D3DPRESENT_INTERVAL_DEFAULT = 0x00000000,
  D3DPRESENT_INTERVAL_ONE = 0x00000001,
  D3DPRESENT_INTERVAL_TWO = 0x00000002,
  D3DPRESENT_INTERVAL_THREE = 0x00000003,
  D3DPRESENT_INTERVAL_FOUR = 0x00000004,
  D3DPRESENT_INTERVAL_IMMEDIATE = 0x80000000,
};

#define D3DADAPTER_DEFAULT 0

#define D3DERR_WRONGTEXTUREFORMAT ((HRESULT)0x88760818L)
#define D3DERR_UNSUPPORTEDCOLOROPERATION ((HRESULT)0x88760821L)
#define D3DERR_UNSUPPORTEDCOLORARG ((HRESULT)0x88760822L)
#define D3DERR_UNSUPPORTEDALPHAOPERATION ((HRESULT)0x8876081fL)
#define D3DERR_UNSUPPORTEDALPHAARG ((HRESULT)0x88760820L)
#define D3DERR_TOOMANYOPERATIONS ((HRESULT)0x8876081dL)
#define D3DERR_CONFLICTINGTEXTUREFILTER ((HRESULT)0x8876081eL)
#define D3DERR_UNSUPPORTEDFACTORVALUE ((HRESULT)0x88760823L)
#define D3DERR_CONFLICTINGTEXTUREPALETTE ((HRESULT)0x88760826L)
#define D3DERR_UNSUPPORTEDTEXTUREFILTER ((HRESULT)0x88760825L)
#define D3DERR_OUTOFVIDEOMEMORY ((HRESULT)0x8876017cL)
enum : DWORD {
  D3DMCS_MATERIAL = 0,
  D3DMCS_COLOR1 = 1,
  D3DMCS_COLOR2 = 2,
};
#ifndef D3D_OK
#define D3D_OK 0L
#endif
#ifndef D3DERR_DEVICELOST
#define D3DERR_DEVICELOST ((HRESULT)0x88760868L)
#endif
#ifndef D3DERR_DEVICENOTRESET
#define D3DERR_DEVICENOTRESET ((HRESULT)0x88760869L)
#endif
#ifndef D3DERR_NOTAVAILABLE
#define D3DERR_NOTAVAILABLE ((HRESULT)0x8876086AL)
#endif
#ifndef D3DERR_INVALIDCALL
#define D3DERR_INVALIDCALL ((HRESULT)0x80000003L)
#endif
#define D3D_SDK_VERSION 32
#define D3DPS_VERSION(function, version) ((function) * 1000 + (version))
#define D3DVS_VERSION(function, version) ((function) * 1000 + (version))
enum : DWORD {
  D3DDECLTYPE_FLOAT1 = 0, D3DDECLTYPE_FLOAT2 = 1, D3DDECLTYPE_FLOAT3 = 2,
  D3DDECLTYPE_FLOAT4 = 3, D3DDECLTYPE_D3DCOLOR = 4, D3DDECLTYPE_UBYTE4 = 5,
  D3DDECLTYPE_SHORT2 = 6, D3DDECLTYPE_SHORT4 = 7, D3DDECLTYPE_UNUSED = 15
};
enum : DWORD {
  D3DDECLUSAGE_POSITION = 0, D3DDECLUSAGE_BLENDWEIGHT = 1, D3DDECLUSAGE_BLENDINDICES = 2,
  D3DDECLUSAGE_NORMAL = 3, D3DDECLUSAGE_PSIZE = 4, D3DDECLUSAGE_TESSFACTOR = 5,
  D3DDECLUSAGE_POSITIONT = 6, D3DDECLUSAGE_COLOR = 7, D3DDECLUSAGE_FOG = 8,
  D3DDECLUSAGE_DEPTH = 9, D3DDECLUSAGE_TEXCOORD = 10, D3DDECLUSAGE_TEXCOORD0 = 10,
  D3DDECLUSAGE_TEXCOORD1 = 11, D3DDECLUSAGE_TEXCOORD2 = 12, D3DDECLUSAGE_TEXCOORD3 = 13,
  D3DDECLUSAGE_TEXCOORD4 = 14, D3DDECLUSAGE_TEXCOORD5 = 15, D3DDECLUSAGE_TEXCOORD6 = 16,
  D3DDECLUSAGE_TEXCOORD7 = 17
};
enum : DWORD {
  D3DDECLMETHOD_DEFAULT = 0, D3DDECLMETHOD_PARTIAL = 1,
  D3DDECLMETHOD_TEXCOORDINDEX = 2, D3DDECLMETHOD_OPTIMIZED = 3
};
#define D3DDECL_END() { D3DDECLTYPE_UNUSED, 0, 0, 0 }
#define D3DPRASTERCAPS_FOGRANGE 0x00010000L
#define D3DCREATE_FPU_PRESERVE              0x00000002L
#define D3DCREATE_MULTITHREADED             0x00000004L
#define D3DCREATE_PUREDEVICE                0x00000010L
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020L
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040L
#define D3DCREATE_MIXED_VERTEXPROCESSING    0x00000080L

static const GUID IID_IDirect3DTexture9 = { 0x85dc72d0, 0x0ac4, 0x4db0, { 0xbb, 0xdb, 0x97, 0xf1, 0xa6, 0x02, 0x71, 0x00 } };


// D3D9 interface method stubs for Linux compile gate
// (methods are never called on Linux; declarations allow code using
// forward-declared interfaces to compile)
struct D3DGAMMARAMP {
  WORD red[256];
  WORD green[256];
  WORD blue[256];
};
#define D3DSGR_NO_CALIBRATION 0x00000000
#define D3DSGR_CALIBRATE 0x00000001

struct ID3DXBuffer;
struct D3DVERTEXELEMENT9;
struct IDirect3DSwapChain9 {
  HRESULT Present(const RECT*, const RECT*, HWND, const void*, DWORD) { return 0; }
  HRESULT GetBackBuffer(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**) { return 0; }
  ULONG Release() { return 0; }
};

struct IDirect3DDevice9 {
  IDirect3DVertexBuffer9* m_currentVB = nullptr;
  uint32_t m_vbOffset = 0;
  uint32_t m_vbStride = 32;
  IDirect3DIndexBuffer9* m_currentIB = nullptr;
  bool m_inScene = false;
  uint32_t m_currentImageIndex = 0;
  // Fixed-function state the Vulkan path has to reproduce. The engine sets these once per mesh and
  // then draws, so a draw cannot know what to read without them.
  DWORD m_fvf = 0;
  // The four texture stages the Vulkan pipeline can evaluate. Direct3D 9 has eight; the engine uses
  // more than four only in the terrain's eight-stage driver workaround, whose extra stages compute
  // lighting from the diffuse colour rather than sampling a texture.
  static const int kStages = 4;
  IDirect3DBaseTexture9* m_stageTex[kStages] = { nullptr, nullptr, nullptr, nullptr };
  DWORD m_stageColorOp[kStages] = { D3DTOP_MODULATE, D3DTOP_DISABLE, D3DTOP_DISABLE, D3DTOP_DISABLE };
  DWORD m_stageColorArg0[kStages] = { D3DTA_CURRENT, D3DTA_CURRENT, D3DTA_CURRENT, D3DTA_CURRENT };
  DWORD m_stageColorArg1[kStages] = { D3DTA_TEXTURE, D3DTA_TEXTURE, D3DTA_TEXTURE, D3DTA_TEXTURE };
  DWORD m_stageColorArg2[kStages] = { D3DTA_CURRENT, D3DTA_CURRENT, D3DTA_CURRENT, D3DTA_CURRENT };
  DWORD m_stageAlphaOp[kStages] = { D3DTOP_MODULATE, D3DTOP_DISABLE, D3DTOP_DISABLE, D3DTOP_DISABLE };
  DWORD m_stageAlphaArg1[kStages] = { D3DTA_TEXTURE, D3DTA_TEXTURE, D3DTA_TEXTURE, D3DTA_TEXTURE };
  DWORD m_stageAlphaArg2[kStages] = { D3DTA_CURRENT, D3DTA_CURRENT, D3DTA_CURRENT, D3DTA_CURRENT };
  // TEXCOORDINDEX keeps its high bits: TCI_CAMERASPACEPOSITION is what makes a fog, cloud or noise
  // stage sample a projected coordinate instead of a vertex attribute.
  DWORD m_stageTexCoordIndex[kStages] = { 0, 1, 2, 3 };
  DWORD m_stageTransformFlags[kStages] = { 0, 0, 0, 0 };
  D3DMATRIX m_textureTransform[kStages] = {
    { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } },
    { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } },
    { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } },
    { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } }
  };
  // Per-stage sampler state, recorded for the descriptor's sampler. D3DTADDRESS_WRAP and
  // D3DTEXF_LINEAR are what an unset stage uses.
  DWORD m_stageAddressU[kStages] = { 1, 1, 1, 1 };
  DWORD m_stageAddressV[kStages] = { 1, 1, 1, 1 };
  DWORD m_stageMinFilter[kStages] = { 2, 2, 2, 2 };
  DWORD m_stageMagFilter[kStages] = { 2, 2, 2, 2 };
  DWORD m_stageMipFilter[kStages] = { 2, 2, 2, 2 };
  DWORD m_alphaTestEnable = 0;
  DWORD m_alphaRef = 0;
  DWORD m_alphaFunc = 4;          // D3DCMP_LESSEQUAL
  DWORD m_textureFactor = 0xFFFFFFFFu;
  DWORD m_zEnable = 1;
  DWORD m_zFunc = 4;
  DWORD m_zWrite = 1;
  DWORD m_alphaBlendEnable = 0;
  DWORD m_colorWriteEnable = 0x0000000f;
  DWORD m_srcBlend = 2;
  DWORD m_destBlend = 1;
  IDirect3DSurface9 *m_currentRenderTarget = nullptr;
  D3DMATRIX m_world = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
  D3DMATRIX m_view = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
  D3DMATRIX m_projection = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
  D3DMATERIAL9 m_material = { {1,1,1,1}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, 0 };
  DWORD m_lighting = 1;
  DWORD m_colorVertex = 1;
  DWORD m_diffuseMaterialSource = D3DMCS_MATERIAL;
  DWORD m_ambientMaterialSource = D3DMCS_MATERIAL;
  DWORD m_emissiveMaterialSource = D3DMCS_MATERIAL;
  DWORD m_globalAmbient = 0;
  D3DLIGHT9 m_lights[4] = {};
  bool m_lightEnabled[4] = {};
  void sync_vulkan_texture();
  HRESULT CreateIndexBuffer(UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE);
  HRESULT CreateVertexBuffer(UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE);
  HRESULT CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
  HRESULT CreatePixelShader(const DWORD*, IDirect3DPixelShader9**);
  HRESULT CreateVertexShader(const DWORD*, IDirect3DVertexShader9**);
  HRESULT CreateVertexDeclaration(const D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9**);
  HRESULT SetIndices(IDirect3DIndexBuffer9* ib) { m_currentIB = ib; return 0; }
  HRESULT SetTransform(D3DTRANSFORMSTATETYPE which, const D3DMATRIX *m) {
    if (!m) return 0;
    if (which == D3DTS_WORLD) m_world = *m;
    else if (which == D3DTS_VIEW) m_view = *m;
    else if (which == D3DTS_PROJECTION) m_projection = *m;
    else if (which >= D3DTS_TEXTURE0 && which <= D3DTS_TEXTURE3) {
      // The stage's texture matrix: the fog, cloud and noise passes multiply camera-space position
      // by one of these to get their coordinates.
      m_textureTransform[(unsigned)which - (unsigned)D3DTS_TEXTURE0] = *m;
    }
    return 0;
  }
  HRESULT GetTransform(D3DTRANSFORMSTATETYPE which, D3DMATRIX *out) {
    if (!out) return 0;
    if (which == D3DTS_WORLD) *out = m_world;
    else if (which == D3DTS_VIEW) *out = m_view;
    else if (which == D3DTS_PROJECTION) *out = m_projection;
    return 0;
  }
  HRESULT SetStreamSource(UINT stream, IDirect3DVertexBuffer9* vb, UINT offset, UINT stride) {
    if (stream == 0) {
      m_currentVB = vb;
      m_vbOffset = offset;
      m_vbStride = stride ? stride : 32;
    }
    return 0;
  }
  HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE, int, int, UINT, UINT, UINT);
  HRESULT SetMaterial(const D3DMATERIAL9 *mat) { if (mat) m_material = *mat; return 0; }
  HRESULT SetLight(DWORD index, const D3DLIGHT9 *light) {
    if (index < 4 && light) m_lights[index] = *light;
    return 0;
  }
  HRESULT LightEnable(DWORD index, BOOL enable) {
    if (index < 4) m_lightEnabled[index] = enable != 0;
    return 0;
  }
  HRESULT SetClipPlane(DWORD, const float*) { return 0; }
  HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (state == D3DRS_LIGHTING) { m_lighting = value; return 0; }
    if (state == D3DRS_COLORVERTEX) { m_colorVertex = value; return 0; }
    if (state == D3DRS_DIFFUSEMATERIALSOURCE) { m_diffuseMaterialSource = value; return 0; }
    if (state == D3DRS_AMBIENTMATERIALSOURCE) { m_ambientMaterialSource = value; return 0; }
    if (state == D3DRS_EMISSIVEMATERIALSOURCE) { m_emissiveMaterialSource = value; return 0; }
    if (state == D3DRS_AMBIENT) { m_globalAmbient = value; return 0; }
    if (state == D3DRS_ZENABLE) m_zEnable = value;
    else if (state == D3DRS_ZWRITEENABLE) m_zWrite = value;
    else if (state == D3DRS_ZFUNC) m_zFunc = value;
    else if (state == D3DRS_ALPHABLENDENABLE) {
      m_alphaBlendEnable = value;
      return 0;
    } else if (state == D3DRS_SRCBLEND) {
      m_srcBlend = value;
      return 0;
    } else if (state == D3DRS_DESTBLEND) {
      m_destBlend = value;
      return 0;
    } else if (state == D3DRS_COLORWRITEENABLE) {
      m_colorWriteEnable = value;
      return 0;
    } else if (state == D3DRS_ALPHATESTENABLE) {
      m_alphaTestEnable = value;
      return 0;
    } else if (state == D3DRS_ALPHAREF) {
      m_alphaRef = value;
      return 0;
    } else if (state == D3DRS_ALPHAFUNC) {
      m_alphaFunc = value;
      return 0;
    } else if (state == D3DRS_TEXTUREFACTOR) {
      m_textureFactor = value;
      return 0;
    }
    Platform_SetVulkanDepthState(m_zEnable != 0, m_zWrite != 0, (int)m_zFunc);
#else
    (void)state;
    (void)value;
#endif
    return 0;
  }
  HRESULT SetTextureStageState(UINT, D3DTEXTURESTAGESTATETYPE, DWORD);
  HRESULT SetSamplerState(DWORD stage, D3DSAMPLERSTATETYPE type, DWORD value) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (stage < (DWORD)kStages) {
      switch (type) {
      case D3DSAMP_ADDRESSU: m_stageAddressU[stage] = value; break;
      case D3DSAMP_ADDRESSV: m_stageAddressV[stage] = value; break;
      case D3DSAMP_MINFILTER: m_stageMinFilter[stage] = value; break;
      case D3DSAMP_MAGFILTER: m_stageMagFilter[stage] = value; break;
      case D3DSAMP_MIPFILTER: m_stageMipFilter[stage] = value; break;
      default: break;
      }
    }
#endif
    return 0;
  }
  HRESULT TestCooperativeLevel() { return 0; }
  HRESULT SetFVF(DWORD fvf) { m_fvf = fvf; return 0; }
  HRESULT GetRenderState(D3DRENDERSTATETYPE state, DWORD *val) {
    if (!val) return 0;
    if (state == D3DRS_COLORWRITEENABLE) *val = m_colorWriteEnable;
    else if (state == D3DRS_ALPHABLENDENABLE) *val = m_alphaBlendEnable;
    else if (state == D3DRS_ZENABLE) *val = m_zEnable;
    else if (state == D3DRS_ZWRITEENABLE) *val = m_zWrite;
    else if (state == D3DRS_ZFUNC) *val = m_zFunc;
    else *val = 0;
    return 0;
  }
  HRESULT GetRenderTarget(UINT, IDirect3DSurface9**);
  HRESULT GetDepthStencilSurface(IDirect3DSurface9**);
  HRESULT SetPixelShaderConstantF(UINT, const float*, UINT) { return 0; }
  HRESULT SetVertexShaderConstantF(UINT, const float*, UINT) { return 0; }
  HRESULT Clear(UINT, const RECT*, DWORD flags, DWORD color, float, DWORD) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (flags & 1) { // D3DCLEAR_TARGET
      float a = ((color >> 24) & 0xFF) / 255.0f;
      float r = ((color >> 16) & 0xFF) / 255.0f;
      float g = ((color >> 8) & 0xFF) / 255.0f;
      float b = (color & 0xFF) / 255.0f;
      Platform_SetVulkanClearColor(r, g, b, a);
    }
#endif
    return 0;
  }
  HRESULT ShowCursor(BOOL) { return 0; }
  HRESULT SetCursorProperties(int, int, IDirect3DSurface9*) { return 0; }
  HRESULT SetCursorPosition(int, int, POINT*) { return 0; }
  HRESULT CreateOffscreenPlainSurface(UINT, UINT, D3DFORMAT, D3DPOOL, IDirect3DSurface9**, HANDLE*);
  HRESULT GetRenderTargetData(IDirect3DSurface9*, IDirect3DSurface9*);
  HRESULT DrawPrimitive(D3DPRIMITIVETYPE, UINT, UINT);
  HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9*) { return 0; }
  HRESULT SetVertexShader(IDirect3DVertexShader9*) { return 0; }
  HRESULT SetPixelShader(IDirect3DPixelShader9*) { return 0; }
  HRESULT SetSoftwareVertexProcessing(BOOL) { return 0; }
  HRESULT GetDeviceCaps(D3DCAPS9* caps);
  HRESULT EvictManagedResources() { return 0; }
  HRESULT BeginScene() {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (!m_inScene) {
      if (Platform_BeginVulkanFrame(&m_currentImageIndex)) {
        m_inScene = true;
      }
    }
#endif
    return 0;
  }
  HRESULT EndScene() { return 0; }
  HRESULT Present(const RECT*, const RECT*, HWND, const void*) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)

    if (m_inScene) {
      Platform_EndVulkanFrame(m_currentImageIndex);
      m_inScene = false;
    } else {
      uint32_t img = 0;
      if (Platform_BeginVulkanFrame(&img)) {
        Platform_EndVulkanFrame(img);
      }
    }
    Platform_PumpEvents();
#endif
    return 0;
  }
  HRESULT SetViewport(const D3DVIEWPORT9*) { return 0; }
  HRESULT GetPixelShader(IDirect3DPixelShader9**) { return 0; }
  HRESULT GetVertexShader(IDirect3DVertexShader9**) { return 0; }
  HRESULT ValidateDevice(DWORD*) { return 0; }
  HRESULT ValidateDevice(unsigned long*) { return 0; }
  HRESULT SetRenderTarget(DWORD, IDirect3DSurface9*);
  HRESULT SetDepthStencilSurface(IDirect3DSurface9*);
  HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE, UINT, const void*, UINT);
  HRESULT UpdateSurface(IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const POINT*);
  HRESULT StretchRect(IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);
  HRESULT UpdateTexture(IDirect3DBaseTexture9*, IDirect3DBaseTexture9*);
  HRESULT GetDisplayMode(UINT, D3DDISPLAYMODE*) { return 0; }
  HRESULT GetFrontBufferData(UINT, IDirect3DSurface9*) { return 0; }
  HRESULT GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**);
  HRESULT CreateDepthStencilSurface(UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
  HRESULT CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain9**) { return 0; }
  UINT GetAvailableTextureMem() { return 0; }
  void SetGammaRamp(UINT, DWORD, const D3DGAMMARAMP*) {}
  HRESULT GetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD*) { return 0; }
  HRESULT GetTexture(DWORD, IDirect3DBaseTexture9**) { return 0; }
  HRESULT SetTexture(DWORD, IDirect3DBaseTexture9*);
  HRESULT Reset(D3DPRESENT_PARAMETERS*) { return 0; }
  ULONG Release() { return 0; }
};
enum D3DCUBEMAP_FACES : unsigned {
  D3DCUBEMAP_FACE_POSITIVE_X = 0,
  D3DCUBEMAP_FACE_NEGATIVE_X = 1,
  D3DCUBEMAP_FACE_POSITIVE_Y = 2,
  D3DCUBEMAP_FACE_NEGATIVE_Y = 3,
  D3DCUBEMAP_FACE_POSITIVE_Z = 4,
  D3DCUBEMAP_FACE_NEGATIVE_Z = 5
};

struct D3DLOCKED_BOX {
  int RowPitch;
  int SlicePitch;
  void* pBits;
};

struct D3DBOX {
  UINT Left, Top, Right, Bottom, Front, Back;
};

struct D3DVOLUME_DESC {
  D3DFORMAT Format;
  D3DRESOURCETYPE Type;
  DWORD Usage;
  D3DPOOL Pool;
  UINT Width;
  UINT Height;
  UINT Depth;
};
static inline uint8_t* Get_Dummy_D3D_Scratch() {
  static uint8_t s_scratch[2048 * 2048 * 4];
  return s_scratch;
}

static inline uint32_t d3d_format_bpp(D3DFORMAT fmt) {
  if (fmt == D3DFMT_A4R4G4B4 || fmt == D3DFMT_R5G6B5 || fmt == D3DFMT_A1R5G5B5 ||
      fmt == D3DFMT_X1R5G5B5 || fmt == D3DFMT_X4R4G4B4 || fmt == D3DFMT_A8L8 || fmt == D3DFMT_V8U8) {
    return 2;
  }
  if (fmt == D3DFMT_R8G8B8) return 3;
  if (fmt == D3DFMT_A8 || fmt == D3DFMT_L8 || fmt == D3DFMT_P8) return 1;
  return 4;
}
static inline size_t d3d_calc_surface_size(uint32_t w, uint32_t h, D3DFORMAT fmt) {
  if (!w) w = 1;
  if (!h) h = 1;
  if (fmt == D3DFMT_DXT1) {
    return (size_t)((w + 3) / 4) * ((h + 3) / 4) * 8;
  } else if (fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3 || fmt == D3DFMT_DXT4 || fmt == D3DFMT_DXT5) {
    return (size_t)((w + 3) / 4) * ((h + 3) / 4) * 16;
  }
  return (size_t)w * h * d3d_format_bpp(fmt);
}
static inline UINT d3d_calc_surface_pitch(uint32_t w, D3DFORMAT fmt) {
  if (!w) w = 1;
  if (fmt == D3DFMT_DXT1) {
    return ((w + 3) / 4) * 8;
  } else if (fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3 || fmt == D3DFMT_DXT4 || fmt == D3DFMT_DXT5) {
    return ((w + 3) / 4) * 16;
  }
  return w * d3d_format_bpp(fmt);
}
static inline size_t d3d_calc_rect_offset(const RECT* pRect, UINT pitch, D3DFORMAT fmt) {
  if (!pRect) return 0;
  int x = pRect->left > 0 ? pRect->left : 0;
  int y = pRect->top > 0 ? pRect->top : 0;
  if (fmt == D3DFMT_DXT1) {
    return (size_t)(y / 4) * pitch + (size_t)(x / 4) * 8;
  } else if (fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3 || fmt == D3DFMT_DXT4 || fmt == D3DFMT_DXT5) {
    return (size_t)(y / 4) * pitch + (size_t)(x / 4) * 16;
  }
  return (size_t)y * pitch + (size_t)x * d3d_format_bpp(fmt);
}
static inline void d3d_read_pixel_bgra(const uint8_t* p, D3DFORMAT fmt, uint8_t& b, uint8_t& g, uint8_t& r, uint8_t& a) {
  switch (fmt) {
  case D3DFMT_A8R8G8B8:
    b = p[0]; g = p[1]; r = p[2]; a = p[3]; break;
  case D3DFMT_X8R8G8B8:
    b = p[0]; g = p[1]; r = p[2]; a = 255; break;
  case D3DFMT_R8G8B8:
    b = p[0]; g = p[1]; r = p[2]; a = 255; break;
  case D3DFMT_A4R4G4B4: {
    uint16_t v = *(const uint16_t*)p;
    a = ((v >> 12) & 0xF) * 17; r = ((v >> 8) & 0xF) * 17; g = ((v >> 4) & 0xF) * 17; b = (v & 0xF) * 17;
    break;
  }
  case D3DFMT_X4R4G4B4: {
    uint16_t v = *(const uint16_t*)p;
    a = 255; r = ((v >> 8) & 0xF) * 17; g = ((v >> 4) & 0xF) * 17; b = (v & 0xF) * 17;
    break;
  }
  case D3DFMT_R5G6B5: {
    uint16_t v = *(const uint16_t*)p;
    a = 255; r = ((v >> 11) & 0x1F) * 255 / 31; g = ((v >> 5) & 0x3F) * 255 / 63; b = (v & 0x1F) * 255 / 31;
    break;
  }
  case D3DFMT_A1R5G5B5:
  case D3DFMT_X1R5G5B5: {
    uint16_t v = *(const uint16_t*)p;
    a = (fmt == D3DFMT_X1R5G5B5 || (v & 0x8000)) ? 255 : 0;
    r = ((v >> 10) & 0x1F) * 255 / 31; g = ((v >> 5) & 0x1F) * 255 / 31; b = (v & 0x1F) * 255 / 31;
    break;
  }
  case D3DFMT_L8:
    b = g = r = p[0]; a = 255; break;
  case D3DFMT_A8:
    b = g = r = 255; a = p[0]; break;
  default:
    b = p[0]; g = p[1]; r = p[2]; a = p[3]; break;
  }
}
static inline void d3d_write_pixel_bgra(uint8_t* p, D3DFORMAT fmt, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
  switch (fmt) {
  case D3DFMT_A8R8G8B8:
    p[0] = b; p[1] = g; p[2] = r; p[3] = a; break;
  case D3DFMT_X8R8G8B8:
    p[0] = b; p[1] = g; p[2] = r; p[3] = 255; break;
  case D3DFMT_R8G8B8:
    p[0] = b; p[1] = g; p[2] = r; break;
  case D3DFMT_A4R4G4B4:
  case D3DFMT_X4R4G4B4:
    *(uint16_t*)p = (uint16_t)(((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
    break;
  case D3DFMT_R5G6B5:
    *(uint16_t*)p = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    break;
  case D3DFMT_A1R5G5B5:
  case D3DFMT_X1R5G5B5:
    *(uint16_t*)p = (uint16_t)((a ? 0x8000 : 0) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    break;
  case D3DFMT_L8:
    p[0] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8); break;
  case D3DFMT_A8:
    p[0] = a; break;
  default:
    p[0] = b; p[1] = g; p[2] = r; p[3] = a; break;
  }
}

struct IDirect3DBaseTexture9 {
  uint32_t m_refCount = 1;
  bool m_heapAllocated = false;
  virtual ~IDirect3DBaseTexture9() {}
  DWORD SetLOD(DWORD) { return 0; }
  // Virtual because the loader asks a texture for its chain before it locks every level: reporting
  // one level made it load only the top one, which is why minified ground shimmered.
  virtual DWORD GetLevelCount() { return 1; }
  DWORD SetPriority(DWORD) { return 0; }
  DWORD GetPriority() { return 0; }
  D3DRESOURCETYPE GetType() { return D3DRTYPE_TEXTURE; }
  virtual ULONG AddRef() { return ++m_refCount; }
  virtual ULONG Release() {
    if (m_refCount > 0) --m_refCount;
    if (m_refCount == 0 && m_heapAllocated) {
      delete this;
      return 0;
    }
    return m_refCount;
  }
};
struct IDirect3DTexture9 : IDirect3DBaseTexture9 {
  void *m_vulkanTex = nullptr;
  uint32_t m_width = 256;
  uint32_t m_height = 256;
  D3DFORMAT m_format = D3DFMT_A8R8G8B8;
  D3DPOOL m_pool = D3DPOOL_MANAGED;
  std::vector<uint8_t> m_buffer;
  // One scratch per mip level: the loader locks every level before it unlocks any of them, so a
  // single shared scratch would alias them all onto the same bytes.
  std::vector<std::vector<uint8_t>> m_mipLevels;
  uint32_t m_levels = 1;
  uint32_t m_unlockedLevels = 0;
  // Level-0 write tracking for the in-place upload: LockRect accumulates the touched rectangle,
  // UnlockRect ships exactly that to the GPU.
  RECT m_dirtyRect = { 0, 0, 0, 0 };
  bool m_hasDirtyRect = false;

  IDirect3DTexture9() : m_vulkanTex(nullptr), m_width(256), m_height(256), m_format(D3DFMT_A8R8G8B8) {}

  DWORD GetLevelCount() override { return m_levels ? m_levels : 1; }

  // The mip chain is usable for filtering only once every level has reached the image.
  bool Mips_Ready() const { return m_levels > 1 && m_unlockedLevels >= m_levels; }

  ~IDirect3DTexture9() override {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (m_vulkanTex) {
      Platform_DestroyVulkanTexture(m_vulkanTex);
      m_vulkanTex = nullptr;
    }
#endif
  }

  size_t calc_size() const {
    return d3d_calc_surface_size(m_width, m_height, m_format);
  }

  UINT calc_pitch() const {
    return d3d_calc_surface_pitch(m_width, m_format);
  }

  HRESULT GetSurfaceLevel(UINT, IDirect3DSurface9**);
  HRESULT GetLevelDesc(UINT level, D3DSURFACE_DESC* desc) {
    if (desc) {
      uint32_t lw = m_width >> level;
      uint32_t lh = m_height >> level;
      desc->Width = lw ? lw : 1;
      desc->Height = lh ? lh : 1;
      desc->Format = m_format;
      desc->Pool = m_pool;
      desc->Type = D3DRTYPE_SURFACE;
    }
    return 0;
  }
  HRESULT LockRect(UINT level, D3DLOCKED_RECT* rect, const RECT* pRect, DWORD) {
    if (level == 0) {
      size_t sz = calc_size();
      if (m_buffer.size() < sz) m_buffer.resize(sz);
      if (rect) {
        UINT pitch = calc_pitch();
        size_t off = d3d_calc_rect_offset(pRect, pitch, m_format);
        if (off >= m_buffer.size()) off = 0;
        rect->Pitch = pitch;
        rect->pBits = m_buffer.data() + off;
      }
      if (pRect) {
        // Remember which part the engine touched: the fog/shroud path locks one small rect per
        // call, and the upload must cover exactly that, not the whole 512x512 texture.
        if (m_hasDirtyRect) {
          m_dirtyRect.left = std::min(m_dirtyRect.left, pRect->left);
          m_dirtyRect.top = std::min(m_dirtyRect.top, pRect->top);
          m_dirtyRect.right = std::max(m_dirtyRect.right, pRect->right);
          m_dirtyRect.bottom = std::max(m_dirtyRect.bottom, pRect->bottom);
        } else {
          m_dirtyRect = *pRect;
          m_hasDirtyRect = true;
        }
      } else {
        m_hasDirtyRect = true;
        m_dirtyRect = RECT{ 0, 0, (int)m_width, (int)m_height };
      }
    } else {
      uint32_t lw = m_width >> level; if (!lw) lw = 1;
      uint32_t lh = m_height >> level; if (!lh) lh = 1;
      size_t sz = d3d_calc_surface_size(lw, lh, m_format);
      if (m_mipLevels.size() < m_levels) m_mipLevels.resize(m_levels);
      std::vector<uint8_t> &storage = m_mipLevels[level];
      if (storage.size() < sz) storage.resize(sz);
      if (rect) {
        rect->Pitch = d3d_calc_surface_pitch(lw, m_format);
        rect->pBits = storage.data();
      }
    }
    return 0;
  }
  HRESULT UnlockRect(UINT level) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (level == 0 && m_buffer.size() > 0) {
      // Vulkan holds these as they are. A1R5G5B5 / X1R5G5B5 are deliberately not here: their one
      // alpha bit is the terrain's blend mask, and the conversion path below reproduces what the
      // device does with it.
      const bool native = (m_format == D3DFMT_DXT1 || m_format == D3DFMT_DXT2 ||
                           m_format == D3DFMT_DXT3 || m_format == D3DFMT_DXT4 ||
                           m_format == D3DFMT_DXT5 || m_format == D3DFMT_A8R8G8B8 ||
                           m_format == D3DFMT_R5G6B5);
      // The Vulkan image is created once per texture, not once per upload. The fog/shroud path
      // used to destroy and recreate a whole device-local texture on every UnlockRect, with a
      // device-wide wait at both ends of every lock - the single biggest per-frame stall when
      // panning the camera. Creation is lazy but exactly once; uploads go through
      // Platform_UpdateVulkanTexture, which never waits on the queue.
      if (!m_vulkanTex) {
        m_vulkanTex = Platform_CreateVulkanTextureEx(m_width, m_height, m_levels, nullptr,
            native ? (uint32_t)m_format : (uint32_t)D3DFMT_A8R8G8B8);
      }
      if (!m_vulkanTex) return 0;

      RECT r = m_hasDirtyRect ? m_dirtyRect : RECT{ 0, 0, (int)m_width, (int)m_height };
      m_hasDirtyRect = false;
      if (r.left < 0) r.left = 0;
      if (r.top < 0) r.top = 0;
      if (r.right > (int)m_width) r.right = (int)m_width;
      if (r.bottom > (int)m_height) r.bottom = (int)m_height;
      const int rw = r.right - r.left, rh = r.bottom - r.top;
      if (rw <= 0 || rh <= 0) return 0;

      const UINT pitch = calc_pitch();
      uint32_t bw = (uint32_t)rw, bh = (uint32_t)rh, rowBytes = (uint32_t)rw * d3d_format_bpp(m_format);
      if (m_format == D3DFMT_DXT1) { bw = (rw + 3) / 4; bh = (rh + 3) / 4; rowBytes = bw * 8; }
      else if (m_format == D3DFMT_DXT2 || m_format == D3DFMT_DXT3 ||
               m_format == D3DFMT_DXT4 || m_format == D3DFMT_DXT5) { bw = (rw + 3) / 4; bh = (rh + 3) / 4; rowBytes = bw * 16; }
      const uint8_t *srcBase = m_buffer.data() + d3d_calc_rect_offset(&r, pitch, m_format);

      if (native && rw == (int)m_width && rh == (int)m_height) {
        // Whole-texture upload of an already-native format: the rows are contiguous, hand the
        // buffer straight over.
        Platform_UpdateVulkanTexture(m_vulkanTex, r.left, r.top, rw, rh, m_buffer.data());
      } else if (native) {
        std::vector<uint8_t> packed((size_t)rowBytes * bh);
        uint8_t *dst = packed.data();
        for (uint32_t row = 0; row < bh; ++row) {
          memcpy(dst + (size_t)row * rowBytes, srcBase + (size_t)row * pitch, rowBytes);
        }
        Platform_UpdateVulkanTexture(m_vulkanTex, r.left, r.top, rw, rh, packed.data());
      } else {
        // Everything else (555/444/L8/A8/X8...) becomes BGRA here, the format the image was made
        // with, so odd D3D formats cost one conversion but no texture object churn.
        const uint32_t bpp = d3d_format_bpp(m_format);
        std::vector<uint8_t> packed((size_t)rw * rh * 4);
        for (int y = 0; y < rh; ++y) {
          const uint8_t *srcRow = srcBase + (size_t)y * pitch;
          uint8_t *dstRow = packed.data() + (size_t)y * rw * 4;
          for (int x = 0; x < rw; ++x) {
            uint8_t b, g, rr, a;
            d3d_read_pixel_bgra(srcRow + (size_t)x * bpp, m_format, b, g, rr, a);
            dstRow[(size_t)x * 4 + 0] = b;
            dstRow[(size_t)x * 4 + 1] = g;
            dstRow[(size_t)x * 4 + 2] = rr;
            dstRow[(size_t)x * 4 + 3] = a;
          }
        }
        Platform_UpdateVulkanTexture(m_vulkanTex, r.left, r.top, rw, rh, packed.data());
      }
      ++m_unlockedLevels;
    } else if (level > 0 && level < m_levels && level < m_mipLevels.size() &&
               m_mipLevels[level].size() > 0) {
      // The loader locks every level of the chain and unlocks them in turn; each one becomes a
      // copy into its own level of the image, so minification can filter across the chain instead
      // of aliasing the top level.
      // Vulkan holds these as they are. A1R5G5B5 / X1R5G5B5 are deliberately not here: their one
      // alpha bit is the terrain's blend mask, and the conversion path below reproduces what the
      // device does with it.
      const bool native = (m_format == D3DFMT_DXT1 || m_format == D3DFMT_DXT2 ||
                           m_format == D3DFMT_DXT3 || m_format == D3DFMT_DXT4 ||
                           m_format == D3DFMT_DXT5 || m_format == D3DFMT_A8R8G8B8 ||
                           m_format == D3DFMT_R5G6B5);
      if (!m_vulkanTex) {
        m_vulkanTex = Platform_CreateVulkanTextureEx(m_width, m_height, m_levels, nullptr,
            native ? (uint32_t)m_format : (uint32_t)D3DFMT_A8R8G8B8);
      }
      if (!m_vulkanTex) return 0;
      uint32_t lw = m_width >> level; if (!lw) lw = 1;
      uint32_t lh = m_height >> level; if (!lh) lh = 1;
      const std::vector<uint8_t> &source = m_mipLevels[level];
      if (native) {
        Platform_UpdateVulkanTextureLevel(m_vulkanTex, level, 0, 0, (int)lw, (int)lh, source.data());
      } else {
        const uint32_t bpp = d3d_format_bpp(m_format);
        const UINT pitch = d3d_calc_surface_pitch(lw, m_format);
        std::vector<uint8_t> packed((size_t)lw * lh * 4);
        for (uint32_t y = 0; y < lh; ++y) {
          const uint8_t *srcRow = source.data() + (size_t)y * pitch;
          uint8_t *dstRow = packed.data() + (size_t)y * lw * 4;
          for (uint32_t x = 0; x < lw; ++x) {
            uint8_t b, g, rr, a;
            d3d_read_pixel_bgra(srcRow + (size_t)x * bpp, m_format, b, g, rr, a);
            dstRow[(size_t)x * 4 + 0] = b;
            dstRow[(size_t)x * 4 + 1] = g;
            dstRow[(size_t)x * 4 + 2] = rr;
            dstRow[(size_t)x * 4 + 3] = a;
          }
        }
        Platform_UpdateVulkanTextureLevel(m_vulkanTex, level, 0, 0, (int)lw, (int)lh, packed.data());
      }
      ++m_unlockedLevels;
    }
#endif
    return 0;
  }
  HRESULT AddDirtyRect(const RECT*) { return 0; }
};
struct IDirect3DCubeTexture9 : IDirect3DBaseTexture9 {
  HRESULT GetCubeMapSurface(D3DCUBEMAP_FACES, UINT, IDirect3DSurface9**) { return 0; }
  HRESULT GetLevelDesc(UINT, D3DSURFACE_DESC*) { return 0; }
  HRESULT LockRect(D3DCUBEMAP_FACES, UINT, D3DLOCKED_RECT*, const RECT*, DWORD) { return 0; }
  HRESULT UnlockRect(D3DCUBEMAP_FACES, UINT) { return 0; }
};
struct IDirect3DVolumeTexture9 : IDirect3DBaseTexture9 {
  HRESULT GetLevelDesc(UINT, D3DVOLUME_DESC*) { return 0; }
  HRESULT LockBox(UINT, D3DLOCKED_BOX*, const D3DBOX*, DWORD) { return 0; }
  HRESULT UnlockBox(UINT) { return 0; }
};
struct IDirect3DSurface9 {
  IDirect3DTexture9 *m_ownerTex = nullptr;
  UINT m_level = 0;
  uint32_t m_width = 1280;
  uint32_t m_height = 720;
  D3DFORMAT m_format = D3DFMT_X8R8G8B8;
  D3DPOOL m_pool = D3DPOOL_DEFAULT;
  std::vector<uint8_t> m_buffer;
  uint32_t m_refCount = 1;
  bool m_heapAllocated = false;

  HRESULT GetDesc(D3DSURFACE_DESC* desc) {
    if (m_ownerTex) return m_ownerTex->GetLevelDesc(m_level, desc);
    if (desc) {
      desc->Width = m_width;
      desc->Height = m_height;
      desc->Format = m_format;
      desc->Pool = m_pool;
      desc->Type = D3DRTYPE_SURFACE;
    }
    return 0;
  }
  HRESULT LockRect(D3DLOCKED_RECT* rect, const RECT* pRect, DWORD flags) {
    if (m_ownerTex) return m_ownerTex->LockRect(m_level, rect, pRect, flags);
    size_t sz = d3d_calc_surface_size(m_width, m_height, m_format);
    if (m_buffer.size() < sz) m_buffer.resize(sz);
    if (rect) {
      UINT pitch = d3d_calc_surface_pitch(m_width, m_format);
      size_t off = d3d_calc_rect_offset(pRect, pitch, m_format);
      if (off >= m_buffer.size()) off = 0;
      rect->Pitch = pitch;
      rect->pBits = m_buffer.data() + off;
    }
    return 0;
  }
  HRESULT UnlockRect() {
    if (m_ownerTex) return m_ownerTex->UnlockRect(m_level);
    return 0;
  }
  HRESULT GetContainer(const GUID&, void** ppContainer) {
    if (!ppContainer) return D3DERR_INVALIDCALL;
    if (m_ownerTex) {
      m_ownerTex->AddRef();
      *ppContainer = m_ownerTex;
      return 0;
    }
    *ppContainer = nullptr;
    return D3DERR_INVALIDCALL;
  }
  ULONG AddRef() { return ++m_refCount; }
  ULONG Release() {
    if (!m_heapAllocated) return 1;
    if (m_refCount > 0) --m_refCount;
    if (m_refCount == 0) {
      if (m_ownerTex) m_ownerTex->Release();
      delete this;
      return 0;
    }
    return m_refCount;
  }
};
static inline HRESULT D3D_Copy_Surface_Rects(IDirect3DSurface9* dst, const RECT* dstRect, IDirect3DSurface9* src, const RECT* srcRect) {
  if (!dst || !src) return D3DERR_INVALIDCALL;
  D3DSURFACE_DESC srcDesc = {}, dstDesc = {};
  src->GetDesc(&srcDesc);
  dst->GetDesc(&dstDesc);
  if (!srcDesc.Width || !srcDesc.Height || !dstDesc.Width || !dstDesc.Height) return 0;
  D3DLOCKED_RECT srcLock = {}, dstLock = {};
  src->LockRect(&srcLock, nullptr, D3DLOCK_READONLY);
  dst->LockRect(&dstLock, nullptr, 0);
  if (!srcLock.pBits || !dstLock.pBits) return D3DERR_INVALIDCALL;

  if (srcDesc.Format == D3DFMT_DXT1 || srcDesc.Format == D3DFMT_DXT2 || srcDesc.Format == D3DFMT_DXT3 ||
      srcDesc.Format == D3DFMT_DXT4 || srcDesc.Format == D3DFMT_DXT5) {
    if (srcDesc.Format == dstDesc.Format) {
      size_t sz = std::min(d3d_calc_surface_size(srcDesc.Width, srcDesc.Height, srcDesc.Format),
                           d3d_calc_surface_size(dstDesc.Width, dstDesc.Height, dstDesc.Format));
      memcpy(dstLock.pBits, srcLock.pBits, sz);
    }
    src->UnlockRect();
    dst->UnlockRect();
    return 0;
  }

  int sx0 = srcRect ? std::max<int>(0, srcRect->left) : 0;
  int sy0 = srcRect ? std::max<int>(0, srcRect->top) : 0;
  int sx1 = srcRect ? std::min<int>((int)srcDesc.Width, srcRect->right) : (int)srcDesc.Width;
  int sy1 = srcRect ? std::min<int>((int)srcDesc.Height, srcRect->bottom) : (int)srcDesc.Height;
  int dx0 = dstRect ? std::max<int>(0, dstRect->left) : 0;
  int dy0 = dstRect ? std::max<int>(0, dstRect->top) : 0;
  int dx1 = dstRect ? std::min<int>((int)dstDesc.Width, dstRect->right) : (int)dstDesc.Width;
  int dy1 = dstRect ? std::min<int>((int)dstDesc.Height, dstRect->bottom) : (int)dstDesc.Height;
  int sw = sx1 - sx0, sh = sy1 - sy0;
  int dw = dx1 - dx0, dh = dy1 - dy0;

  if (sw > 0 && sh > 0 && dw > 0 && dh > 0) {
    uint32_t srcBpp = d3d_format_bpp(srcDesc.Format);
    uint32_t dstBpp = d3d_format_bpp(dstDesc.Format);
    const uint8_t* srcBase = (const uint8_t*)srcLock.pBits;
    uint8_t* dstBase = (uint8_t*)dstLock.pBits;
    if (srcDesc.Format == dstDesc.Format && sw == dw && sh == dh) {
      for (int y = 0; y < dh; ++y) {
        memcpy(dstBase + (size_t)(dy0 + y) * dstLock.Pitch + (size_t)dx0 * dstBpp,
               srcBase + (size_t)(sy0 + y) * srcLock.Pitch + (size_t)sx0 * srcBpp,
               (size_t)dw * dstBpp);
      }
    } else {
      for (int y = 0; y < dh; ++y) {
        int sy = sy0 + (y * sh) / dh;
        const uint8_t* srcRow = srcBase + (size_t)sy * srcLock.Pitch;
        uint8_t* dstRow = dstBase + (size_t)(dy0 + y) * dstLock.Pitch;
        for (int x = 0; x < dw; ++x) {
          int sx = sx0 + (x * sw) / dw;
          uint8_t b, g, r, a;
          d3d_read_pixel_bgra(srcRow + (size_t)sx * srcBpp, srcDesc.Format, b, g, r, a);
          d3d_write_pixel_bgra(dstRow + (size_t)(dx0 + x) * dstBpp, dstDesc.Format, b, g, r, a);
        }
      }
    }
  }
  src->UnlockRect();
  dst->UnlockRect();
  return 0;
}
struct IDirect3DVertexBuffer9 {
  std::vector<uint8_t> m_buffer;
  uint32_t m_refCount = 1;
  HRESULT Lock(UINT offset, UINT size, void** ppbData, DWORD) {
    size_t needed = offset + (size ? size : 65536);
    if (m_buffer.size() < needed) m_buffer.resize(needed);
    if (ppbData) *ppbData = m_buffer.data() + offset;
    return 0;
  }
  HRESULT Unlock() { return 0; }
  ULONG AddRef() { return ++m_refCount; }
  ULONG Release() {
    if (m_refCount > 0) --m_refCount;
    if (m_refCount == 0) { delete this; return 0; }
    return m_refCount;
  }
};
struct IDirect3DIndexBuffer9 {
  std::vector<uint8_t> m_buffer;
  uint32_t m_refCount = 1;
  HRESULT Lock(UINT offset, UINT size, void** ppbData, DWORD) {
    size_t needed = offset + (size ? size : 65536);
    if (m_buffer.size() < needed) m_buffer.resize(needed);
    if (ppbData) *ppbData = m_buffer.data() + offset;
    return 0;
  }
  HRESULT Unlock() { return 0; }
  ULONG AddRef() { return ++m_refCount; }
  ULONG Release() {
    if (m_refCount > 0) --m_refCount;
    if (m_refCount == 0) { delete this; return 0; }
    return m_refCount;
  }
};
struct IDirect3DVertexShader9 {
  ULONG Release() { return 0; }
};
struct IDirect3DVertexDeclaration9 {
  ULONG Release() { return 0; }
};
struct IDirect3DPixelShader9 {
  ULONG Release() { return 0; }
};
inline HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT level, IDirect3DSurface9** ppSurface) {
  if (ppSurface) {
    auto surf = new IDirect3DSurface9();
    surf->m_heapAllocated = true;
    surf->m_ownerTex = this;
    surf->m_level = level;
    this->AddRef();
    *ppSurface = surf;
  }
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT size, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9** pp, HANDLE) {
  if (pp) {
    auto ib = new IDirect3DIndexBuffer9();
    if (size) ib->m_buffer.resize(size);
    *pp = ib;
  }
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT size, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9** pp, HANDLE) {
  if (pp) {
    auto vb = new IDirect3DVertexBuffer9();
    if (size) vb->m_buffer.resize(size);
    *pp = vb;
  }
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateTexture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** pp, HANDLE*) {
  if (usage & D3DUSAGE_RENDERTARGET) {
    if (pp) *pp = nullptr;
    return D3DERR_NOTAVAILABLE;
  }
  if (pp) {
    auto tex = new IDirect3DTexture9();
    tex->m_heapAllocated = true;
    tex->m_width = width ? width : 256;
    tex->m_height = height ? height : 256;
    tex->m_format = (format != D3DFMT_UNKNOWN) ? format : D3DFMT_A8R8G8B8;
    tex->m_pool = pool;
    // MIP_LEVELS_ALL arrives as 0: "as many as the size allows". A real chain is what lets the
    // sampler filter instead of aliasing, so the count has to survive into the image.
    uint32_t count = levels;
    if (count == 0) {
      count = 1;
      for (uint32_t extent = width > height ? width : height; extent > 1; extent >>= 1) ++count;
    }
    tex->m_levels = count ? count : 1;
    *pp = tex;
  }
  return 0;
}
inline HRESULT IDirect3DDevice9::CreatePixelShader(const DWORD*, IDirect3DPixelShader9** pp) {
  static IDirect3DPixelShader9 s_ps;
  if (pp) *pp = &s_ps;
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateVertexShader(const DWORD*, IDirect3DVertexShader9** pp) {
  static IDirect3DVertexShader9 s_vs;
  if (pp) *pp = &s_vs;
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9** pp) {
  static IDirect3DVertexDeclaration9 s_vd;
  if (pp) *pp = &s_vd;
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateOffscreenPlainSurface(UINT width, UINT height, D3DFORMAT format, D3DPOOL pool, IDirect3DSurface9** pp, HANDLE*) {
  if (pp) {
    auto surf = new IDirect3DSurface9();
    surf->m_heapAllocated = true;
    surf->m_width = width ? width : 256;
    surf->m_height = height ? height : 256;
    surf->m_format = (format != D3DFMT_UNKNOWN) ? format : D3DFMT_A8R8G8B8;
    surf->m_pool = pool;
    *pp = surf;
  }
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateDepthStencilSurface(UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9** pp, HANDLE*) {
  static IDirect3DSurface9 s_surf;
  if (pp) *pp = &s_surf;
  return 0;
}
static IDirect3DSurface9 s_defaultRenderTarget;
inline HRESULT IDirect3DDevice9::GetRenderTarget(UINT, IDirect3DSurface9** pp) {
  if (pp) {
    s_defaultRenderTarget.AddRef();
    *pp = &s_defaultRenderTarget;
  }
  return 0;
}
inline HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** pp) {
  static IDirect3DSurface9 s_surf;
  if (pp) *pp = &s_surf;
  return 0;
}
inline HRESULT IDirect3DDevice9::GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9** pp) {
  // The back buffer is the swapchain image this frame draws into, which the Vulkan path renders
  // directly - there is no separate surface object for it. Handing back the default render target
  // is what keeps a Begin_Scene after an offscreen pass drawing to the screen again: a distinct
  // surface object here made every later 2D draw look like a render-to-texture draw and vanish,
  // which is why the in-game HUD and control bar never appeared.
  if (pp) *pp = &s_defaultRenderTarget;
  return 0;
}
inline HRESULT IDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9*, IDirect3DSurface9*) { return 0; }
inline HRESULT IDirect3DDevice9::SetRenderTarget(DWORD, IDirect3DSurface9* surf) {
  m_currentRenderTarget = surf ? surf : &s_defaultRenderTarget;
  return 0;
}
inline HRESULT IDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9*) { return 0; }
inline HRESULT IDirect3DDevice9::UpdateSurface(IDirect3DSurface9* src, const RECT* srcRect, IDirect3DSurface9* dst, const POINT* dstPoint) {
  if (!src || !dst) return D3DERR_INVALIDCALL;
  if (dstPoint) {
    D3DSURFACE_DESC srcDesc = {};
    src->GetDesc(&srcDesc);
    int w = srcRect ? (srcRect->right - srcRect->left) : (int)srcDesc.Width;
    int h = srcRect ? (srcRect->bottom - srcRect->top) : (int)srcDesc.Height;
    RECT dstRect = { dstPoint->x, dstPoint->y, dstPoint->x + w, dstPoint->y + h };
    return D3D_Copy_Surface_Rects(dst, &dstRect, src, srcRect);
  }
  return D3D_Copy_Surface_Rects(dst, nullptr, src, srcRect);
}
inline HRESULT IDirect3DDevice9::StretchRect(IDirect3DSurface9* src, const RECT* srcRect, IDirect3DSurface9* dst, const RECT* dstRect, D3DTEXTUREFILTERTYPE) {
  return D3D_Copy_Surface_Rects(dst, dstRect, src, srcRect);
}
inline HRESULT IDirect3DDevice9::UpdateTexture(IDirect3DBaseTexture9* srcBase, IDirect3DBaseTexture9* dstBase) {
  if (!srcBase || !dstBase) return D3DERR_INVALIDCALL;
  IDirect3DSurface9 *srcSurf = nullptr, *dstSurf = nullptr;
  ((IDirect3DTexture9*)srcBase)->GetSurfaceLevel(0, &srcSurf);
  ((IDirect3DTexture9*)dstBase)->GetSurfaceLevel(0, &dstSurf);
  HRESULT hr = D3D_Copy_Surface_Rects(dstSurf, nullptr, srcSurf, nullptr);
  if (srcSurf) srcSurf->Release();
  if (dstSurf) dstSurf->Release();
  return hr;
}
inline void IDirect3DDevice9::sync_vulkan_texture() {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  // Four stages in one bind, each with the sampler state its stage was last given. A stage with no
  // texture samples the white dummy, which is what the fixed-function pipeline did with the stage
  // off, so the combiner arithmetic stays the same either way.
  void *textures[kStages] = { nullptr, nullptr, nullptr, nullptr };
  uint32_t keys[kStages] = { 0, 0, 0, 0 };
  for (int stage = 0; stage < kStages; ++stage) {
    IDirect3DTexture9 *texture = (IDirect3DTexture9 *)m_stageTex[stage];
    if (!texture) continue;
    textures[stage] = texture->m_vulkanTex;
    keys[stage] = Platform_MakeSamplerKey(m_stageAddressU[stage], m_stageAddressV[stage],
                                          m_stageMinFilter[stage], m_stageMagFilter[stage],
                                          m_stageMipFilter[stage], texture->Mips_Ready() ? 1 : 0);
  }
  Platform_BindVulkanTextures(textures, keys);
#endif
}
inline HRESULT IDirect3DDevice9::SetTextureStageState(UINT stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (stage < (UINT)kStages) {
    switch (type) {
    case D3DTSS_COLOROP: m_stageColorOp[stage] = value; break;
    case D3DTSS_COLORARG0: m_stageColorArg0[stage] = value; break;
    case D3DTSS_COLORARG1: m_stageColorArg1[stage] = value; break;
    case D3DTSS_COLORARG2: m_stageColorArg2[stage] = value; break;
    case D3DTSS_ALPHAOP: m_stageAlphaOp[stage] = value; break;
    case D3DTSS_ALPHAARG1: m_stageAlphaArg1[stage] = value; break;
    case D3DTSS_ALPHAARG2: m_stageAlphaArg2[stage] = value; break;
    case D3DTSS_TEXCOORDINDEX:
      // The whole word: the high bits name the coordinate source, and masking them off is what
      // made every fog and cloud stage sample a vertex attribute instead of a projected position.
      m_stageTexCoordIndex[stage] = value;
      break;
    case D3DTSS_TEXTURETRANSFORMFLAGS: m_stageTransformFlags[stage] = value; break;
    default: break;
    }
  }
#endif
  return 0;
}
inline HRESULT IDirect3DDevice9::SetTexture(DWORD stage, IDirect3DBaseTexture9* tex) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (stage < (DWORD)kStages) {
    m_stageTex[stage] = tex;
    sync_vulkan_texture();
  }
#endif
  return 0;
}
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
// A row-major D3D9 matrix multiplies a row vector from the left; the shader does mvp * vec4
// on a column-major mat4, so source row i is written into column i. Destination row 1
// (source column j) is negated: Vulkan's NDC y is up, Direct3D 9's is down.
static void build_vulkan_push_constants(const IDirect3DDevice9 *device, PlatformVulkanPushConstants &pc)
{
  const float (*world)[4] = device->m_world.m;
  const float (*view)[4] = device->m_view.m;
  const float (*projection)[4] = device->m_projection.m;

  float worldView[4][4];
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) sum += world[i][k] * view[k][j];
      worldView[i][j] = sum;
    }
  }

  float mvp[4][4];
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) sum += worldView[i][k] * projection[k][j];
      mvp[i][j] = sum;
    }
  }

  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      pc.mvp[i * 4 + j] = (j == 1) ? -mvp[i][j] : mvp[i][j];

  // With lighting off, Direct3D 9 multiplies the vertex colour by the material diffuse. A material
  // that is black in both places would paint the model black, so a preset carrying no colour falls
  // back to white.
  if (device->m_lighting) {
    for (int i = 0; i < 4; ++i) pc.materialColor[i] = 1.0f;
    return;
  }
  const D3DMATERIAL9 &mat = device->m_material;
  float color[4];
  if (mat.Diffuse.r > 0.0f || mat.Diffuse.g > 0.0f || mat.Diffuse.b > 0.0f) {
    color[0] = std::min(1.0f, mat.Diffuse.r);
    color[1] = std::min(1.0f, mat.Diffuse.g);
    color[2] = std::min(1.0f, mat.Diffuse.b);
    color[3] = std::min(1.0f, mat.Diffuse.a > 0.0f ? mat.Diffuse.a : 1.0f);
  } else if (mat.Ambient.r > 0.0f || mat.Ambient.g > 0.0f || mat.Ambient.b > 0.0f) {
    color[0] = std::min(1.0f, mat.Ambient.r);
    color[1] = std::min(1.0f, mat.Ambient.g);
    color[2] = std::min(1.0f, mat.Ambient.b);
    color[3] = std::min(1.0f, mat.Ambient.a > 0.0f ? mat.Ambient.a : 1.0f);
  } else {
    color[0] = color[1] = color[2] = color[3] = 1.0f;
  }
  for (int i = 0; i < 4; ++i) pc.materialColor[i] = color[i];
}

static std::vector<VKCanonicalVertex> s_canonicalVertices;

// Row-major D3D9 matrix times a row vector: out[j] = sum over i of v[i] * m[i][j]. Direct3D applies
// a texture transform to a camera-space position the same way, which is how the fog, cloud and
// noise stages get their coordinates.
static inline void transform_row_vector(const float v[4], const float m[4][4], float out[4])
{
  for (int j = 0; j < 4; ++j)
    out[j] = v[0] * m[0][j] + v[1] * m[1][j] + v[2] * m[2][j] + v[3] * m[3][j];
}

static inline void vk_build_normal_transform(const float source[4][4], float result[4][4])
{
  for (int row = 0; row < 4; ++row)
    for (int column = 0; column < 4; ++column)
      result[row][column] = row == column ? 1.0f : 0.0f;
  const float a = source[0][0], b = source[0][1], c = source[0][2];
  const float d = source[1][0], e = source[1][1], f = source[1][2];
  const float g = source[2][0], h = source[2][1], i = source[2][2];
  const float determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  if (determinant == 0.0f) return;
  const float scale = 1.0f / determinant;
  result[0][0] = (e * i - f * h) * scale;
  result[0][1] = (f * g - d * i) * scale;
  result[0][2] = (d * h - e * g) * scale;
  result[1][0] = (c * h - b * i) * scale;
  result[1][1] = (a * i - c * g) * scale;
  result[1][2] = (b * g - a * h) * scale;
  result[2][0] = (b * f - c * e) * scale;
  result[2][1] = (c * d - a * f) * scale;
  result[2][2] = (a * e - b * d) * scale;
}

struct VKFixedFunctionLightState {
  DWORD type;
  float position[4];
  float direction[4];
  float diffuse[4];
  float attenuation[4];
  float spot[4];
};

struct VKFixedFunctionLightingState {
  bool colorVertex;
  DWORD diffuseSource;
  DWORD ambientSource;
  DWORD emissiveSource;
  float globalAmbient[4];
  float materialDiffuse[4];
  float materialAmbient[4];
  float materialEmissive[4];
  unsigned lightCount;
  VKFixedFunctionLightState lights[4];
};

static inline float vk_saturate(float value)
{
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

static inline void vk_normalize3(float value[3])
{
  const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
  if (length > 1.0e-20f) {
    const float inverseLength = 1.0f / length;
    value[0] *= inverseLength;
    value[1] *= inverseLength;
    value[2] *= inverseLength;
  } else {
    value[0] = value[1] = value[2] = 0.0f;
  }
}


static inline void vk_build_lighting_state(const IDirect3DDevice9 *device,
                                            VKFixedFunctionLightingState &state)
{
  state = {};
  state.colorVertex = device->m_colorVertex != 0;
  state.diffuseSource = device->m_diffuseMaterialSource;
  state.ambientSource = device->m_ambientMaterialSource;
  state.emissiveSource = device->m_emissiveMaterialSource;
  memcpy(state.materialDiffuse, &device->m_material.Diffuse, sizeof(state.materialDiffuse));
  memcpy(state.materialAmbient, &device->m_material.Ambient, sizeof(state.materialAmbient));
  memcpy(state.materialEmissive, &device->m_material.Emissive, sizeof(state.materialEmissive));

  const DWORD ambient = device->m_globalAmbient;
  state.globalAmbient[0] = ((ambient >> 16) & 0xffu) / 255.0f;
  state.globalAmbient[1] = ((ambient >> 8) & 0xffu) / 255.0f;
  state.globalAmbient[2] = (ambient & 0xffu) / 255.0f;
  state.globalAmbient[3] = ((ambient >> 24) & 0xffu) / 255.0f;

  for (unsigned index = 0; index < 4; ++index) {
    if (!device->m_lightEnabled[index]) continue;
    const D3DLIGHT9 &source = device->m_lights[index];
    VKFixedFunctionLightState &light = state.lights[state.lightCount++];
    light.type = source.Type;
    const float position[4] = { source.Position.x, source.Position.y, source.Position.z, 1.0f };
    const float direction[4] = { source.Direction.x, source.Direction.y, source.Direction.z, 0.0f };
    transform_row_vector(position, device->m_view.m, light.position);
    transform_row_vector(direction, device->m_view.m, light.direction);
    memcpy(light.diffuse, &source.Diffuse, sizeof(light.diffuse));
    light.attenuation[0] = source.Attenuation0;
    light.attenuation[1] = source.Attenuation1;
    light.attenuation[2] = source.Attenuation2;
    light.attenuation[3] = source.Range;
    light.spot[0] = std::cos(source.Theta * 0.5f);
    light.spot[1] = std::cos(source.Phi * 0.5f);
    light.spot[2] = source.Falloff;
    light.spot[3] = 0.0f;
  }
}

static inline const float *vk_material_source(DWORD source, const float material[4],
                                               const float vertex[4], bool hasDiffuse,
                                               bool colorVertex)
{
  return source == D3DMCS_COLOR1 && hasDiffuse && colorVertex ? vertex : material;
}

static inline void vk_light_vertex(const VKFixedFunctionLightingState &state,
                                   const float viewPosition[4], const float viewNormal[4],
                                   bool hasDiffuse, const float vertexDiffuse[4],
                                   float output[4])
{
  const float *diffuseMaterial = vk_material_source(state.diffuseSource, state.materialDiffuse,
                                                     vertexDiffuse, hasDiffuse, state.colorVertex);
  const float *ambientMaterial = vk_material_source(state.ambientSource, state.materialAmbient,
                                                     vertexDiffuse, hasDiffuse, state.colorVertex);
  const float *emissiveMaterial = vk_material_source(state.emissiveSource, state.materialEmissive,
                                                      vertexDiffuse, hasDiffuse, state.colorVertex);
  float normal[3] = { viewNormal[0], viewNormal[1], viewNormal[2] };
  vk_normalize3(normal);
  float diffuseLight[3] = {};

  for (unsigned index = 0; index < state.lightCount; ++index) {
    const VKFixedFunctionLightState &light = state.lights[index];
    float toLight[3];
    float attenuation = 1.0f;
    if (light.type == D3DLIGHT_DIRECTIONAL) {
      toLight[0] = -light.direction[0];
      toLight[1] = -light.direction[1];
      toLight[2] = -light.direction[2];
      vk_normalize3(toLight);
    } else if (light.type == D3DLIGHT_POINT || light.type == D3DLIGHT_SPOT) {
      const float offset[3] = { light.position[0] - viewPosition[0],
                                light.position[1] - viewPosition[1],
                                light.position[2] - viewPosition[2] };
      const float distance = std::sqrt(offset[0] * offset[0] + offset[1] * offset[1] +
                                       offset[2] * offset[2]);
      const float inverseDistance = 1.0f / (distance > 0.0001f ? distance : 0.0001f);
      toLight[0] = offset[0] * inverseDistance;
      toLight[1] = offset[1] * inverseDistance;
      toLight[2] = offset[2] * inverseDistance;
      const float denominator = light.attenuation[0] + light.attenuation[1] * distance +
                                light.attenuation[2] * distance * distance;
      attenuation = 1.0f / (denominator > 0.0001f ? denominator : 0.0001f);
      if (distance > light.attenuation[3]) attenuation = 0.0f;
      if (light.type == D3DLIGHT_SPOT) {
        float direction[3] = { light.direction[0], light.direction[1], light.direction[2] };
        vk_normalize3(direction);
        const float spotCosine = -(toLight[0] * direction[0] + toLight[1] * direction[1] +
                                   toLight[2] * direction[2]);
        float spot = (spotCosine - light.spot[1]) /
                     (light.spot[0] - light.spot[1] > 0.0001f ?
                      light.spot[0] - light.spot[1] : 0.0001f);
        spot = vk_saturate(spot);
        attenuation *= std::pow(spot, light.spot[2]);
      }
    } else {
      continue;
    }

    float lambert = normal[0] * toLight[0] + normal[1] * toLight[1] + normal[2] * toLight[2];
    if (lambert < 0.0f) lambert = 0.0f;
    for (int channel = 0; channel < 3; ++channel)
      diffuseLight[channel] += light.diffuse[channel] * lambert * attenuation;
  }

  for (int channel = 0; channel < 3; ++channel)
    output[channel] = vk_saturate(diffuseMaterial[channel] * diffuseLight[channel] +
                                  ambientMaterial[channel] * state.globalAmbient[channel] +
                                  emissiveMaterial[channel]);
  output[3] = diffuseMaterial[3];
}
// The push constants carry packed stage state as float bits: a Vulkan float member keeps the
// block to the 128 bytes every implementation guarantees, and the shader reads it back with
// floatBitsToUint.
static inline float dword_as_float(DWORD bits)
{
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

// The engine writes its vertices by hand, so the bytes are the FVF it set. Decode them into the one
// layout the pipeline reads, work out what the four texture stages ask for, then draw.
static void draw_canonical(const IDirect3DDevice9 *device, const unsigned char *source,
                           uint32_t vertexCount, const uint16_t *indices, uint32_t indexCount)
{
  VKVertexLayout layout;
  // Always decode set 0 into uv and set 1 into uv1: which one a stage samples is decided below by
  // its TEXCOORDINDEX. Asking the decoder to swap them as well made a stage that asked for set 1
  // read the other set, which sampled fog and UI atlas coordinates from the wrong attributes.
  if (!VKLayout_From_FVF(device->m_fvf, layout, 0)) {
    return;
  }
  if ((device->m_colorWriteEnable & 0x0E) == 0) {
    return;
  }
  if (device->m_currentRenderTarget != nullptr && device->m_currentRenderTarget != &s_defaultRenderTarget) {
    // Render-to-texture is not implemented on this path; the pass is skipped, and the first skip
    // says so once so a missing HUD or mirror is diagnosable from the log.
    static bool s_reportedSkippedTarget = false;
    if (!s_reportedSkippedTarget) {
      s_reportedSkippedTarget = true;
      fprintf(stderr, "RTT draw skipped while target %p bound\n", (void *)device->m_currentRenderTarget);
    }
    return;
  }
  if (vertexCount == 0) return;
  // ---- what the four texture stages ask for
  uint32_t stageConfig[4];
  uint32_t stageAlphaConfig[4];
  bool projects[4] = { false, false, false, false };
  for (int stage = 0; stage < 4; ++stage) {
    const DWORD coordinateIndex = device->m_stageTexCoordIndex[stage];
    const DWORD transformFlags = device->m_stageTransformFlags[stage];
    uint32_t uvSource = PLATFORM_UV_SOURCE_UV0;
    if ((coordinateIndex & 0xFFFF0000u) == (DWORD)D3DTSS_TCI_CAMERASPACEPOSITION && transformFlags != 0) {
      // Camera-space position through the stage's texture matrix: the shroud, cloud and noise
      // passes. Sampling the vertex attribute instead is what painted white and dark patches
      // across the ground.
      projects[stage] = true;
      uvSource = (stage == 0) ? PLATFORM_UV_SOURCE_PROJECTED0 : PLATFORM_UV_SOURCE_PROJECTED1;
    } else if ((coordinateIndex & 0xFu) != 0) {
      // A stage may name set 1 when the format carries only one set. Sampling an attribute the
      // vertex has no bytes for reads (0,0) for every vertex, so the whole surface comes out as a
      // single texel - flat grey ground, black infantry, blue deck plating, all of it the material
      // colour with a constant texel on top. Clamp to set 0 in that case.
      uvSource = (layout.uv1Offset >= 0) ? PLATFORM_UV_SOURCE_UV1 : PLATFORM_UV_SOURCE_UV0;
    }

    stageConfig[stage] = (device->m_stageColorOp[stage] & 0xFFu)
                       | ((device->m_stageColorArg1[stage] & 0xFFu) << 8)
                       | ((device->m_stageColorArg2[stage] & 0xFFu) << 16)
                       | (uvSource << 24);
    stageAlphaConfig[stage] = (device->m_stageAlphaOp[stage] & 0xFFu)
                            | ((device->m_stageAlphaArg1[stage] & 0xFFu) << 8)
                            | ((device->m_stageAlphaArg2[stage] & 0xFFu) << 16)
                            | ((device->m_stageColorArg0[stage] & 0xFFu) << 24);
  }
  const bool projectStage0 = projects[0];
  int sharedProjectedStage = 1;
  while (sharedProjectedStage < 4 && !projects[sharedProjectedStage]) ++sharedProjectedStage;
  const bool projectShared = sharedProjectedStage < 4;

  const bool lightingEnabled = device->m_lighting != 0;
  float worldView[4][4];
  if (projectStage0 || projectShared || lightingEnabled) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        float sum = 0.0f;
        for (int k = 0; k < 4; ++k) sum += device->m_world.m[i][k] * device->m_view.m[k][j];
        worldView[i][j] = sum;
      }
    }
  }
  float normalTransform[4][4];
  VKFixedFunctionLightingState lightingState = {};
  if (lightingEnabled) {
    vk_build_normal_transform(worldView, normalTransform);
    vk_build_lighting_state(device, lightingState);
  }

  s_canonicalVertices.resize(vertexCount);
  for (uint32_t i = 0; i < vertexCount; ++i) {
    VKCanonicalVertex &out = s_canonicalVertices[i];
    VKDecodeVertex(source + (size_t)i * layout.stride, layout, out);
    if (!projectStage0 && !projectShared && !lightingEnabled) continue;
    const float position[4] = { out.position[0], out.position[1], out.position[2], 1.0f };
    float camera[4];
    transform_row_vector(position, worldView, camera);
    if (projectStage0) {
      float transformed[4];
      transform_row_vector(camera, device->m_textureTransform[0].m, transformed);
      const bool divide = (device->m_stageTransformFlags[0] & D3DTTFF_PROJECTED) != 0 && transformed[3] != 0.0f;
      const float w = divide ? transformed[3] : 1.0f;
      out.projected0[0] = transformed[0] / w;
      out.projected0[1] = transformed[1] / w;
    }
    if (projectShared) {
      float transformed[4];
      transform_row_vector(camera, device->m_textureTransform[sharedProjectedStage].m, transformed);
      const bool divide = (device->m_stageTransformFlags[sharedProjectedStage] & D3DTTFF_PROJECTED) != 0 &&
                          transformed[3] != 0.0f;
      const float w = divide ? transformed[3] : 1.0f;
      out.projected1[0] = transformed[0] / w;
      out.projected1[1] = transformed[1] / w;
    }
    if (lightingEnabled) {
      float objectNormal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
      if (layout.normalOffset >= 0) {
        const float *normal = (const float *)(source + (size_t)i * layout.stride + layout.normalOffset);
        objectNormal[0] = normal[0];
        objectNormal[1] = normal[1];
        objectNormal[2] = normal[2];
      }
      float viewNormal[4];
      transform_row_vector(objectNormal, normalTransform, viewNormal);
      float vertexDiffuse[4];
      memcpy(vertexDiffuse, out.color, sizeof(vertexDiffuse));
      vk_light_vertex(lightingState, camera, viewNormal, layout.colorOffset >= 0,
                      vertexDiffuse, out.color);
    }
  }

  PlatformVulkanPushConstants pc = {};
  build_vulkan_push_constants(device, pc);
  // XYZRHW is already in pixels. Multiplying it by the camera matrix turns a terrain tile into
  // the giant wedge on the menu. Direct3D skips world/view/projection for this FVF.
  if ((device->m_fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW) {
    Platform_SetVulkanDepthState(0, 0, 8);
    const PlatformVulkanContext *ctx = Platform_GetVulkanContext();
    const float width = (ctx && ctx->width > 0) ? (float)ctx->width : 800.0f;
    const float height = (ctx && ctx->height > 0) ? (float)ctx->height : 600.0f;
    for (int i = 0; i < 16; ++i) pc.mvp[i] = 0.0f;
    pc.mvp[0] = 2.0f / width;
    pc.mvp[5] = -2.0f / height;
    pc.mvp[10] = 1.0f;
    pc.mvp[12] = -1.0f;
    pc.mvp[13] = 1.0f;
    pc.mvp[15] = 1.0f;
  } else {
    Platform_SetVulkanDepthState(device->m_zEnable != 0, device->m_zWrite != 0, (int)device->m_zFunc);
  }
  for (int stage = 0; stage < 4; ++stage) {
    pc.stageConfig[stage] = dword_as_float(stageConfig[stage]);
    pc.stageAlphaConfig[stage] = dword_as_float(stageAlphaConfig[stage]);
  }
  pc.alphaReference = (float)(device->m_alphaRef & 0xFFu) / 255.0f;
  pc.alphaConfig = dword_as_float((device->m_alphaTestEnable ? 1u : 0u) |
                                  ((device->m_alphaFunc & 0xFFu) << 8));
  pc.textureFactor = dword_as_float(device->m_textureFactor);

  const_cast<IDirect3DDevice9*>(device)->sync_vulkan_texture();
  int blendMode = 0;
  if (device->m_alphaBlendEnable) {
    // Two spellings of "destination = source * destination": the shroud pass uses
    // SRC=ZERO, DEST=SRCCOLOR, while the water mirror uses SRC=DESTCOLOR, DEST=ZERO.
    // Only the second was recognised, so the shroud fell through to alpha blending and
    // replaced the terrain with its grey texel instead of multiplying into it.
    if ((device->m_srcBlend == D3DBLEND_DESTCOLOR && device->m_destBlend == D3DBLEND_ZERO) ||
        (device->m_srcBlend == D3DBLEND_ZERO && device->m_destBlend == D3DBLEND_SRCCOLOR)) {
      blendMode = 2;
    } else if (device->m_destBlend == D3DBLEND_ONE) {
      blendMode = 3;
    } else {
      blendMode = 1;
    }
  }
  Platform_SetVulkanBlendState(blendMode);
  Platform_SetVulkanPushConstants(&pc);
  Platform_DrawVulkanCanonical((const float *)s_canonicalVertices.data(), vertexCount, indices, indexCount);
}
#endif

inline HRESULT IDirect3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE type, int base_vertex_index, int min_vertex_index, UINT num_vertices, UINT start_index, UINT prim_count) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (m_currentVB && m_currentIB && prim_count > 0 && m_currentVB->m_buffer.size() > 0 && m_currentIB->m_buffer.size() > 0) {
    uint32_t rawIndexCount = (type == D3DPT_TRIANGLELIST) ? (prim_count * 3) : (prim_count + 2);
    size_t vOffset = m_vbOffset + (size_t)base_vertex_index * m_vbStride;
    if (vOffset < m_currentVB->m_buffer.size() && (start_index + rawIndexCount) * sizeof(uint16_t) <= m_currentIB->m_buffer.size()) {
      size_t avail = (m_currentVB->m_buffer.size() - vOffset) / m_vbStride;
      if (avail == 0) return 0;
      const uint16_t *iData = (const uint16_t *)(m_currentIB->m_buffer.data() + start_index * sizeof(uint16_t));
      uint16_t minIdx, maxIdx;
      // The D3D9 contract requires every index to lie inside [MinVertexIndex,
      // MinVertexIndex + NumVertices), and the wrapper above passes values it computed from the
      // render state itself. When the hint is consistent with the buffer, the per-draw min/max
      // rescan of the whole index range is dead CPU work - and it ran for every single draw.
      const bool trusted = (num_vertices > 0 && (size_t)num_vertices <= avail && min_vertex_index >= 0 &&
                            (size_t)min_vertex_index + (size_t)num_vertices <= avail);
      if (trusted) {
        minIdx = (uint16_t)min_vertex_index;
        maxIdx = (uint16_t)(min_vertex_index + (int)num_vertices - 1);
      } else {
        minIdx = 0xFFFF; maxIdx = 0;
        for (uint32_t i = 0; i < rawIndexCount; ++i) {
          uint16_t idx = iData[i];
          if (idx < avail) {
            if (idx < minIdx) minIdx = idx;
            if (idx > maxIdx) maxIdx = idx;
          }
        }
        if (minIdx > maxIdx) return 0;
      }
      uint32_t sliceVertices = (uint32_t)(maxIdx - minIdx) + 1;
      const unsigned char *vData = m_currentVB->m_buffer.data() + vOffset + (size_t)minIdx * m_vbStride;
      static std::vector<uint16_t> s_rebasedIndices;
      if (type == D3DPT_TRIANGLESTRIP) {
        s_rebasedIndices.resize(prim_count * 3);
        uint32_t out = 0;
        for (UINT p = 0; p < prim_count; ++p) {
          uint16_t i0 = iData[p] >= minIdx ? (uint16_t)(iData[p] - minIdx) : 0;
          uint16_t i1 = iData[p + 1] >= minIdx ? (uint16_t)(iData[p + 1] - minIdx) : 0;
          uint16_t i2 = iData[p + 2] >= minIdx ? (uint16_t)(iData[p + 2] - minIdx) : 0;
          if (p & 1) {
            s_rebasedIndices[out++] = i0;
            s_rebasedIndices[out++] = i2;
            s_rebasedIndices[out++] = i1;
          } else {
            s_rebasedIndices[out++] = i0;
            s_rebasedIndices[out++] = i1;
            s_rebasedIndices[out++] = i2;
          }
        }
        draw_canonical(this, vData, sliceVertices, s_rebasedIndices.data(), out);
      } else if (minIdx == 0 && trusted) {
        // Trusted hint with min = 0: the contract puts every u16 index below num_vertices, which
        // is sliceVertices, so the raw index buffer can go to the GPU unwritten. The old
        // untrusted minIdx == 0 fast path skipped that guarantee and let stale out-of-range
        // indices through as wild triangles - it is gone.
        draw_canonical(this, vData, sliceVertices, iData, rawIndexCount);
      } else {
        s_rebasedIndices.resize(rawIndexCount);
        for (uint32_t i = 0; i < rawIndexCount; ++i) {
          uint16_t idx = iData[i];
          s_rebasedIndices[i] = (idx >= minIdx && idx <= maxIdx) ? (uint16_t)(idx - minIdx) : 0;
        }
        draw_canonical(this, vData, sliceVertices, s_rebasedIndices.data(), rawIndexCount);
      }
    }
  }
#endif
  return 0;
}
static void build_strip_or_fan_indices(D3DPRIMITIVETYPE type, UINT prim_count, std::vector<uint16_t> &indices) {
  indices.resize(prim_count * 3);
  if (type == D3DPT_POINTLIST) {
    // The pipeline draws triangles only, so a point becomes a degenerate triangle on its own
    // vertex. It covers no pixels, which is what a point sprite without point size support did
    // anyway; the old code expanded a point list to prim_count + 2 vertices and drew whatever
    // triangle that happened to name.
    for (UINT p = 0; p < prim_count; ++p) {
      indices[p * 3 + 0] = (uint16_t)p;
      indices[p * 3 + 1] = (uint16_t)p;
      indices[p * 3 + 2] = (uint16_t)p;
    }
    return;
  }
  if (type == D3DPT_TRIANGLESTRIP) {
    for (UINT p = 0; p < prim_count; ++p) {
      indices[p * 3 + 0] = (p % 2 == 0) ? (uint16_t)p : (uint16_t)(p + 1);
      indices[p * 3 + 1] = (p % 2 == 0) ? (uint16_t)(p + 1) : (uint16_t)p;
      indices[p * 3 + 2] = (uint16_t)(p + 2);
    }
  } else if (type == D3DPT_TRIANGLEFAN) {
    for (UINT p = 0; p < prim_count; ++p) {
      indices[p * 3 + 0] = 0;
      indices[p * 3 + 1] = (uint16_t)(p + 1);
      indices[p * 3 + 2] = (uint16_t)(p + 2);
    }
  }
}
inline HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE type, UINT start_vertex, UINT prim_count) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (m_currentVB && prim_count > 0 && m_currentVB->m_buffer.size() > 0) {
    uint32_t vertexCount;
    if (type == D3DPT_TRIANGLELIST) vertexCount = prim_count * 3;
    else if (type == D3DPT_POINTLIST) vertexCount = prim_count;
    else vertexCount = prim_count + 2;
    size_t vOffset = m_vbOffset + (size_t)start_vertex * m_vbStride;
    if (vOffset < m_currentVB->m_buffer.size()) {
      size_t avail = (m_currentVB->m_buffer.size() - vOffset) / m_vbStride;
      if (vertexCount > avail) vertexCount = (uint32_t)avail;
      const unsigned char *vData = m_currentVB->m_buffer.data() + vOffset;
      if (type == D3DPT_TRIANGLESTRIP || type == D3DPT_TRIANGLEFAN || type == D3DPT_POINTLIST) {
        static std::vector<uint16_t> s_unindexedIndices;
        build_strip_or_fan_indices(type, prim_count, s_unindexedIndices);
        draw_canonical(this, vData, vertexCount, s_unindexedIndices.data(), (uint32_t)s_unindexedIndices.size());
      } else {
        draw_canonical(this, vData, vertexCount, nullptr, 0);
      }
    }
  }
#endif
  return 0;
}
inline HRESULT IDirect3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT prim_count, const void* pVertexStreamZeroData, UINT stride) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (pVertexStreamZeroData && prim_count > 0) {
    uint32_t vertexCount;
    if (type == D3DPT_TRIANGLELIST) vertexCount = prim_count * 3;
    else if (type == D3DPT_POINTLIST) vertexCount = prim_count;
    else vertexCount = prim_count + 2;
    VKVertexLayout layout;
    if (VKLayout_From_FVF(m_fvf, layout, m_stageTexCoordIndex[0] & 0xFu) && stride && stride != layout.stride) return 0;
    if (type == D3DPT_TRIANGLESTRIP || type == D3DPT_TRIANGLEFAN || type == D3DPT_POINTLIST) {
      static std::vector<uint16_t> s_upIndices;
      build_strip_or_fan_indices(type, prim_count, s_upIndices);
      draw_canonical(this, (const unsigned char *)pVertexStreamZeroData, vertexCount, s_upIndices.data(), (uint32_t)s_upIndices.size());
    } else {
      draw_canonical(this, (const unsigned char *)pVertexStreamZeroData, vertexCount, nullptr, 0);
    }
  }
#endif
  return 0;
}

#ifndef _D3DMATRIX
#define _D3DMATRIX D3DMATRIX
#endif

enum : DWORD {
  D3DPTFILTERCAPS_MINFPOINT = 0x00000100,
  D3DPTFILTERCAPS_MINFLINEAR = 0x00000200,
  D3DPTFILTERCAPS_MINFANISOTROPIC = 0x00000400,
  D3DPTFILTERCAPS_MIPFPOINT = 0x00010000,
  D3DPTFILTERCAPS_MIPFLINEAR = 0x00020000,
  D3DPTFILTERCAPS_MAGFPOINT = 0x01000000,
  D3DPTFILTERCAPS_MAGFLINEAR = 0x02000000,
  D3DPTFILTERCAPS_MAGFANISOTROPIC = 0x04000000,
  D3DDEVCAPS_HWTRANSFORMANDLIGHT = 0x00020000,
};

#define D3DCAPS2_FULLSCREENGAMMA 0x00020000L
#define D3DPRASTERCAPS_DEPTHBIAS 0x04000000L
#define D3DDEVCAPS_NPATCHES 0x01000000L
#define D3DPMISCCAPS_COLORWRITEENABLE 0x8

#define D3DTEXOPCAPS_DISABLE 0x00000001L
#define D3DTEXOPCAPS_SELECTARG1 0x00000002L
#define D3DTEXOPCAPS_SELECTARG2 0x00000004L
#define D3DTEXOPCAPS_MODULATE 0x00000008L
#define D3DTEXOPCAPS_MODULATE2X 0x00000010L
#define D3DTEXOPCAPS_MODULATE4X 0x00000020L
#define D3DTEXOPCAPS_ADD 0x00000040L
#define D3DTEXOPCAPS_ADDSIGNED 0x00000080L
#define D3DTEXOPCAPS_ADDSIGNED2X 0x00000100L
#define D3DTEXOPCAPS_SUBTRACT 0x00000200L
#define D3DTEXOPCAPS_ADDSMOOTH 0x00000400L
#define D3DTEXOPCAPS_BLENDTEXTUREALPHA 0x00000800L
#define D3DTEXOPCAPS_BLENDFACTORALPHA 0x00001000L
#define D3DTEXOPCAPS_BLENDTEXTUREALPHAPM 0x00002000L
#define D3DTEXOPCAPS_BLENDCURRENTALPHA 0x00004000L
#define D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR 0x00008000L
#define D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA 0x00010000L
#define D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR 0x00020000L
#define D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA 0x00040000L
#define D3DTEXOPCAPS_DOTPRODUCT3 0x00040000L
#define D3DTEXOPCAPS_MULTIPLYADD 0x00100000L
#define D3DTEXOPCAPS_LERP 0x00200000L
#define D3DTEXOPCAPS_BUMPENVMAP 0x00200000L
#define D3DTEXOPCAPS_BUMPENVMAPLUMINANCE 0x00400000L
#define D3DPTEXTURECAPS_CUBEMAP 0x00000800L

// D3DCAPS9 full definition for Linux compile gate
struct D3DCAPS9 {
  D3DDEVTYPE DeviceType;
  UINT AdapterOrdinal;
  DWORD Caps;
  DWORD Caps2;
  DWORD Caps3;
  DWORD PresentationIntervals;
  DWORD CursorCaps;
  DWORD DevCaps;
  DWORD PrimitiveMiscCaps;
  DWORD RasterCaps;
  DWORD ZCmpCaps;
  DWORD SrcBlendCaps;
  DWORD DestBlendCaps;
  DWORD AlphaCmpCaps;
  DWORD ShadeCaps;
  DWORD TextureCaps;
  DWORD TextureFilterCaps;
  DWORD CubeTextureFilterCaps;
  DWORD VolumeTextureFilterCaps;
  DWORD TextureAddressCaps;
  DWORD VolumeTextureAddressCaps;
  DWORD LineCaps;
  DWORD MaxTextureWidth;
  DWORD MaxTextureHeight;
  DWORD MaxVolumeExtent;
  DWORD MaxTextureRepeat;
  DWORD MaxTextureAspectRatio;
  DWORD MaxAnisotropy;
  float MaxVertexW;
  float GuardBandLeft;
  float GuardBandTop;
  float GuardBandRight;
  float GuardBandBottom;
  float ExtentsAdjust;
  DWORD StencilCaps;
  DWORD FVFCaps;
  DWORD TextureOpCaps;
  DWORD MaxTextureBlendStages;
  DWORD MaxSimultaneousTextures;
  DWORD VertexProcessingCaps;
  DWORD MaxActiveLights;
  DWORD MaxUserClipPlanes;
  DWORD MaxVertexBlendMatrices;
  DWORD MaxVertexBlendMatrixIndex;
  float MaxPointSize;
  DWORD MaxPrimitiveCount;
  DWORD MaxVertexIndex;
  DWORD MaxStreams;
  DWORD MaxStreamStride;
  DWORD VertexShaderVersion;
  DWORD MaxVertexShaderConst;
  DWORD PixelShaderVersion;
  float PixelShader1xMaxValue;
  DWORD DevCaps2;
  float MaxNpatchTessellationLevel;
  DWORD Reserved5;
  UINT MasterAdapterOrdinal;
  UINT AdapterOrdinalInGroup;
  UINT NumberOfAdaptersInGroup;
  DWORD DeclTypes;
  DWORD NumSimultaneousRTs;
  DWORD StretchRectFilterCaps;
  DWORD VS20Caps;
  DWORD PS20Caps;
  DWORD VertexTextureFilterCaps;
  DWORD MaxVShaderInstructionsExecuted;
  DWORD MaxPShaderInstructionsExecuted;
  DWORD MaxVertexShader30InstructionSlots;
  DWORD MaxPixelShader30InstructionSlots;
  DWORD StereoCaps;
  DWORD D3D9MISCCAPS;
  DWORD PMISCCAPS;
  DWORD TextureBlendCaps;
  DWORD TextureStageCaps;
  DWORD MaxTextureStage;
};

#ifndef D3DXMATRIX_DEFINED
#define D3DXMATRIX_DEFINED
// D3DXMATRIX stub for d3dx9math.h functions
struct D3DXMATRIX
{
  union
  {
    struct
    {
      float _11, _12, _13, _14;
      float _21, _22, _23, _24;
      float _31, _32, _33, _34;
      float _41, _42, _43, _44;
    };
    float m[4][4];
  };
  D3DXMATRIX() : _11(0), _12(0), _13(0), _14(0),
                   _21(0), _22(0), _23(0), _24(0),
                   _31(0), _32(0), _33(0), _34(0),
                   _41(0), _42(0), _43(0), _44(0) {}
};
inline D3DXMATRIX operator*(const D3DXMATRIX& a, const D3DXMATRIX& b) {
  D3DXMATRIX r{};
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      for (int k = 0; k < 4; k++)
        r.m[i][j] += a.m[i][k] * b.m[k][j];
  return r;
}
inline D3DXMATRIX& operator*=(D3DXMATRIX& a, const D3DXMATRIX& b) { return a = a * b; }
#endif

#define E_FAIL 0x80004005L
#ifndef S_OK
#define S_OK 0L
#endif
#ifndef D3DXVECTOR4_DEFINED
#define D3DXVECTOR4_DEFINED
struct D3DXVECTOR4 { float x, y, z, w; D3DXVECTOR4() : x(0),y(0),z(0),w(0) {} D3DXVECTOR4(float a, float b, float c, float d) : x(a),y(b),z(c),w(d) {} operator const float*() { return &x; } };
#endif
using FLOAT = float;
#define D3DCOLOR_ARGB(A,R,G,B) ((DWORD)(A)<<24 | (DWORD)(R)<<16 | (DWORD)(G)<<8 | (DWORD)(B))

struct D3DDISPLAYMODE { UINT Width, Height, RefreshRate; D3DFORMAT Format; };
enum D3DSWAPEFFECT : unsigned {
  D3DSWAPEFFECT_DISCARD = 1,
  D3DSWAPEFFECT_FLIP = 2,
  D3DSWAPEFFECT_COPY = 3,
  D3DSWAPEFFECT_OVERLAY = 4,
  D3DSWAPEFFECT_FLIPEX = 5
};

#ifndef D3DPRESENT_INTERVAL_DEFAULT
#define D3DPRESENT_INTERVAL_DEFAULT 0x00000000
#define D3DPRESENT_INTERVAL_ONE     0x00000001
#define D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000
#define D3DPRESENT_RATE_DEFAULT     0x00000000
#endif

struct D3DPRESENT_PARAMETERS {
  UINT BackBufferWidth;
  UINT BackBufferHeight;
  D3DFORMAT BackBufferFormat;
  UINT BackBufferCount;
  D3DMULTISAMPLE_TYPE MultiSampleType;
  DWORD MultiSampleQuality;
  D3DSWAPEFFECT SwapEffect;
  HWND hDeviceWindow;
  BOOL Windowed;
  BOOL EnableAutoDepthStencil;
  D3DFORMAT AutoDepthStencilFormat;
  DWORD Flags;
  UINT FullScreen_RefreshRateInHz;
  UINT PresentationInterval;
};
struct D3DVERTEXELEMENT9 { BYTE Stream; BYTE Offset; BYTE Type; BYTE Method; BYTE Usage; BYTE UsageIndex; };
struct D3DADAPTER_IDENTIFIER9 {
  char Driver[512];
  char Description[512];
  char DeviceName[32];
  struct { DWORD LowPart; LONG HighPart; } DriverVersion;
  DWORD VendorId;
  DWORD DeviceId;
  DWORD SubSysId;
  DWORD Revision;
  GUID DeviceIdentifier;
  DWORD WHQLLevel;
};
inline HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9* caps) {
  if (caps) {
    memset(caps, 0, sizeof(*caps));
    caps->DeviceType = D3DDEVTYPE_HAL;
    caps->Caps2 = 0x00020000L;
    caps->DevCaps = 0x00000020L | 0x00000010L;
    caps->PrimitiveMiscCaps = 0x00000080L;
    caps->PresentationIntervals = 0x80000000L | 0x00000001L;
    caps->MaxTextureWidth = 4096;
    caps->MaxTextureHeight = 4096;
    caps->MaxVolumeExtent = 4096;
    caps->MaxTextureRepeat = 32768;
    caps->MaxTextureAspectRatio = 32768;
    caps->MaxAnisotropy = 16;
    caps->MaxVertexW = 1.0f;
    caps->StencilCaps = 0xFF;
    caps->FVFCaps = 0xFF;
    caps->TextureOpCaps = 0xFFFFFFFF;
    caps->TextureCaps = 0xFFFFFFFF;
    caps->TextureFilterCaps = 0xFFFFFFFF;
    caps->MaxTextureBlendStages = 8;
    caps->MaxSimultaneousTextures = 8;
    caps->VertexShaderVersion = 0xFFFE0300;
    caps->MaxVertexShaderConst = 256;
    caps->PixelShaderVersion = 0xFFFF0300;
    caps->PixelShader1xMaxValue = 8.0f;
  }
  return 0;
}
struct IDirect3D9 {
  HRESULT Create(UINT, UINT, D3DFORMAT, D3DFORMAT, UINT, D3DDISPLAYMODE*, IDirect3D9**) { return 0; }
  HRESULT RegisterSoftwareDevice(void*) { return 0; }
  UINT GetAdapterCount() { return 1; }
  HRESULT GetAdapterIdentifier(UINT, DWORD, D3DADAPTER_IDENTIFIER9* id) {
    if (id) {
      memset(id, 0, sizeof(*id));
      strncpy(id->Driver, "Vulkan PAL", sizeof(id->Driver)-1);
      strncpy(id->Description, "Vulkan Render Device", sizeof(id->Description)-1);
      strncpy(id->DeviceName, "Vulkan0", sizeof(id->DeviceName)-1);
      id->VendorId = 0x10DE;
      id->DeviceId = 0x2204;
    }
    return 0;
  }
  HRESULT GetAdapterModeCount(UINT, D3DFORMAT) { return 4; }
  HRESULT EnumAdapterModes(UINT, D3DFORMAT fmt, UINT mode, D3DDISPLAYMODE* d3dmode) {
    if (d3dmode) {
      static const struct { UINT w, h; } modes[4] = {
        { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1920, 1080 }
      };
      UINT idx = (mode < 4) ? mode : 0;
      d3dmode->Width = modes[idx].w;
      d3dmode->Height = modes[idx].h;
      d3dmode->RefreshRate = 60;
      d3dmode->Format = fmt;
    }
    return 0;
  }
  HRESULT GetAdapterDisplayMode(UINT, D3DDISPLAYMODE* d3dmode) {
    if (d3dmode) {
      d3dmode->Width = 1280;
      d3dmode->Height = 720;
      d3dmode->RefreshRate = 60;
      d3dmode->Format = D3DFMT_X8R8G8B8;
    }
    return 0;
  }
  HRESULT CheckDeviceType(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) { return 0; }
  HRESULT CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD usage, D3DRESOURCETYPE, D3DFORMAT) {
    if (usage & D3DUSAGE_RENDERTARGET) {
      return D3DERR_NOTAVAILABLE;
    }
    return 0;
  }
  HRESULT CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE, DWORD*) { return 0; }
  HRESULT CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) { return 0; }
  HRESULT GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS9* caps) {
    if (caps) {
      memset(caps, 0, sizeof(*caps));
      caps->DeviceType = D3DDEVTYPE_HAL;
      caps->Caps2 = 0x00020000L;
      caps->DevCaps = 0x00000020L | 0x00000010L;
      caps->PrimitiveMiscCaps = 0x00000080L;
      caps->PresentationIntervals = 0x80000000L | 0x00000001L;
      caps->MaxTextureWidth = 4096;
      caps->MaxTextureHeight = 4096;
      caps->MaxVolumeExtent = 4096;
      caps->MaxTextureRepeat = 32768;
      caps->MaxTextureAspectRatio = 32768;
      caps->MaxAnisotropy = 16;
      caps->MaxVertexW = 1.0f;
      caps->StencilCaps = 0xFF;
      caps->FVFCaps = 0xFF;
      caps->TextureOpCaps = 0xFFFFFFFF;
      caps->TextureCaps = 0xFFFFFFFF;
      caps->TextureFilterCaps = 0xFFFFFFFF;
      caps->MaxTextureBlendStages = 8;
      caps->MaxSimultaneousTextures = 8;
      caps->VertexShaderVersion = 0xFFFE0300;
      caps->MaxVertexShaderConst = 256;
      caps->PixelShaderVersion = 0xFFFF0300;
      caps->PixelShader1xMaxValue = 8.0f;
    }
    return 0;
  }
  HRESULT CreateDevice(UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9** ppDevice) {
    static IDirect3DDevice9 s_linuxDevice;
    if (ppDevice) *ppDevice = &s_linuxDevice;
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    PlatformWindowInfo *winInfo = Platform_GetWindowInfo();
    if (winInfo && !Platform_GetVulkanContext()->isInitialized) {
      Platform_InitVulkan(winInfo);
    }
#endif
    return 0;
  }
  ULONG Release() { return 0; }
};
#endif

struct ID3DXBuffer {
  void* pBuffer;
  DWORD Size;
  void* GetBufferPointer() { return pBuffer; }
  DWORD GetBufferSize() { return Size; }
  ULONG Release() { return 0; }
};

extern "C" {
}
// D3DX9 runtime function pointers (resolved from d3dx9_43.dll at startup).
// Signatures from d3dx9math.h; declarations here so ww3d_d3d9.h consumers
// can reference them before d3dx9math.h is included.
typedef D3DXMATRIX * (WINAPI * D3DXMatrixBinaryFunction)(D3DXMATRIX * out,
	const D3DXMATRIX * left, const D3DXMATRIX * right);
typedef D3DXMATRIX * (WINAPI * D3DXMatrixUnaryFunction)(D3DXMATRIX * out,
	const D3DXMATRIX * matrix);
typedef HRESULT (WINAPI * D3DXMatrixInverseFunction)(D3DXMATRIX * out, float * determinant,
	const D3DXMATRIX * matrix);
typedef D3DXMATRIX * (WINAPI * D3DXMatrixTripleFunction)(D3DXMATRIX * out,
	float x, float y, float z);

extern D3DXMatrixBinaryFunction	D3DXMatrixMultiply;
extern D3DXMatrixUnaryFunction		D3DXMatrixTranspose;
extern D3DXMatrixInverseFunction	D3DXMatrixInverse;
extern D3DXMatrixTripleFunction	D3DXMatrixScaling;
extern D3DXMatrixTripleFunction	D3DXMatrixTranslation;
static_assert(sizeof(DWORD)==4 && sizeof(D3DCOLOR)==4, "D3D words are 32-bit on LP64 too");
static_assert(sizeof(D3DMATRIX)==64 && sizeof(D3DMATERIAL9)==68, "D3D matrix/material layout");
static_assert(sizeof(D3DLIGHT9)==104 && sizeof(D3DVIEWPORT9)==24, "D3D light/viewport layout");
static_assert(D3DTS_WORLD==256 && D3DSAMP_DMAPOFFSET==13, "State cache indices");
static_assert(D3DFVF_XYZ==2 && D3DFVF_TEXCOORDSIZE3(3)==0x400000, "FVF bit encoding");
#endif
