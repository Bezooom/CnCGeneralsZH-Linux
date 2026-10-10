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
#include <vector>
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
#include "PAL_Window.h"
#include "PAL_Vulkan.h"
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
 D3DFVF_TEXCOUNT_MASK=0xf00, D3DFVF_TEXCOUNT_SHIFT=8, D3DFVF_POSITION_MASK=0x400e;
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

  HRESULT CreateIndexBuffer(UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE);
  HRESULT CreateVertexBuffer(UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE);
  HRESULT CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
  HRESULT CreatePixelShader(const DWORD*, IDirect3DPixelShader9**);
  HRESULT CreateVertexShader(const DWORD*, IDirect3DVertexShader9**);
  HRESULT CreateVertexDeclaration(const D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9**);
  HRESULT SetIndices(IDirect3DIndexBuffer9* ib) { m_currentIB = ib; return 0; }
  HRESULT SetTransform(D3DTRANSFORMSTATETYPE, const D3DMATRIX*) { return 0; }
  HRESULT GetTransform(D3DTRANSFORMSTATETYPE, D3DMATRIX*) { return 0; }
  HRESULT SetStreamSource(UINT stream, IDirect3DVertexBuffer9* vb, UINT offset, UINT stride) {
    m_currentVB = vb;
    m_vbOffset = offset;
    m_vbStride = stride ? stride : 32;
    return 0;
  }
  HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE, int, int, UINT, UINT, UINT);
  HRESULT SetMaterial(const D3DMATERIAL9*) { return 0; }
  HRESULT SetLight(DWORD, const D3DLIGHT9*) { return 0; }
  HRESULT LightEnable(DWORD, BOOL) { return 0; }
  HRESULT SetClipPlane(DWORD, const float*) { return 0; }
  HRESULT SetRenderState(D3DRENDERSTATETYPE, DWORD) { return 0; }
  HRESULT SetTextureStageState(UINT, D3DTEXTURESTAGESTATETYPE, DWORD) { return 0; }
  HRESULT SetSamplerState(DWORD, D3DSAMPLERSTATETYPE, DWORD) { return 0; }
  HRESULT TestCooperativeLevel() { return 0; }
  HRESULT SetFVF(DWORD) { return 0; }
  HRESULT GetRenderState(D3DRENDERSTATETYPE, DWORD*) { return 0; }
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
  HRESULT UpdateSurface(IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const POINT*) { return 0; }
  HRESULT StretchRect(IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE) { return 0; }
  HRESULT UpdateTexture(IDirect3DBaseTexture9*, IDirect3DBaseTexture9*) { return 0; }
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

struct IDirect3DBaseTexture9 {
  DWORD SetLOD(DWORD) { return 0; }
  DWORD GetLevelCount() { return 1; }
  DWORD SetPriority(DWORD) { return 0; }
  DWORD GetPriority() { return 0; }
  D3DRESOURCETYPE GetType() { return D3DRTYPE_TEXTURE; }
  ULONG AddRef() { return 1; }
  ULONG Release() { return 0; }
};
struct IDirect3DTexture9 : IDirect3DBaseTexture9 {
  void *m_vulkanTex = nullptr;
  uint32_t m_width = 256;
  uint32_t m_height = 256;
  std::vector<uint8_t> m_buffer;

  IDirect3DTexture9() : m_vulkanTex(nullptr), m_width(256), m_height(256) {}
  ~IDirect3DTexture9() {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (m_vulkanTex) {
      Platform_DestroyVulkanTexture(m_vulkanTex);
      m_vulkanTex = nullptr;
    }
#endif
  }

  HRESULT GetSurfaceLevel(UINT, IDirect3DSurface9**);
  HRESULT GetLevelDesc(UINT, D3DSURFACE_DESC* desc) {
    if (desc) {
      desc->Width = m_width;
      desc->Height = m_height;
      desc->Format = D3DFMT_A8R8G8B8;
    }
    return 0;
  }
  HRESULT LockRect(UINT, D3DLOCKED_RECT* rect, const RECT*, DWORD) {
    size_t sz = m_width * m_height * 4;
    if (m_buffer.size() < sz) m_buffer.resize(sz);
    if (rect) {
      rect->Pitch = m_width * 4;
      rect->pBits = m_buffer.data();
    }
    return 0;
  }
  HRESULT UnlockRect(UINT) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
    if (m_buffer.size() > 0) {
      if (m_vulkanTex) {
        Platform_DestroyVulkanTexture(m_vulkanTex);
      }
      m_vulkanTex = Platform_CreateVulkanTexture(m_width, m_height, m_buffer.data(), D3DFMT_A8R8G8B8);
    }
#endif
    return 0;
  }
  HRESULT AddDirtyRect(const RECT*) { return 0; }
  ULONG AddRef() { return 1; }
  ULONG Release() { return 0; }
};
struct IDirect3DCubeTexture9 : IDirect3DBaseTexture9 {
  HRESULT GetCubeMapSurface(D3DCUBEMAP_FACES, UINT, IDirect3DSurface9**) { return 0; }
  HRESULT GetLevelDesc(UINT, D3DSURFACE_DESC*) { return 0; }
  HRESULT LockRect(D3DCUBEMAP_FACES, UINT, D3DLOCKED_RECT*, const RECT*, DWORD) { return 0; }
  HRESULT UnlockRect(D3DCUBEMAP_FACES, UINT) { return 0; }
  ULONG AddRef() { return 1; }
  ULONG Release() { return 0; }
};
struct IDirect3DVolumeTexture9 : IDirect3DBaseTexture9 {
  HRESULT GetLevelDesc(UINT, D3DVOLUME_DESC*) { return 0; }
  HRESULT LockBox(UINT, D3DLOCKED_BOX*, const D3DBOX*, DWORD) { return 0; }
  HRESULT UnlockBox(UINT) { return 0; }
  ULONG AddRef() { return 1; }
  ULONG Release() { return 0; }
};
struct IDirect3DSurface9 {
  HRESULT GetDesc(D3DSURFACE_DESC* desc) {
    if (desc) {
      desc->Width = 1280;
      desc->Height = 720;
      desc->Format = D3DFMT_X8R8G8B8;
    }
    return 0;
  }
  HRESULT LockRect(D3DLOCKED_RECT* rect, const RECT*, DWORD) {
    if (rect) {
      rect->Pitch = 1280 * 4;
      rect->pBits = Get_Dummy_D3D_Scratch();
    }
    return 0;
  }
  HRESULT UnlockRect() { return 0; }
  HRESULT GetContainer(const GUID&, void**) { return 0; }
  ULONG AddRef() { return 1; }
  ULONG Release() { return 0; }
};
struct IDirect3DVertexBuffer9 {
  std::vector<uint8_t> m_buffer;
  HRESULT Lock(UINT offset, UINT size, void** ppbData, DWORD) {
    size_t needed = offset + (size ? size : 65536);
    if (m_buffer.size() < needed) m_buffer.resize(needed);
    if (ppbData) *ppbData = m_buffer.data() + offset;
    return 0;
  }
  HRESULT Unlock() { return 0; }
  ULONG Release() { return 0; }
};
struct IDirect3DIndexBuffer9 {
  std::vector<uint8_t> m_buffer;
  HRESULT Lock(UINT offset, UINT size, void** ppbData, DWORD) {
    size_t needed = offset + (size ? size : 65536);
    if (m_buffer.size() < needed) m_buffer.resize(needed);
    if (ppbData) *ppbData = m_buffer.data() + offset;
    return 0;
  }
  HRESULT Unlock() { return 0; }
  ULONG Release() { return 0; }
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
inline HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT, IDirect3DSurface9** ppSurface) {
  static IDirect3DSurface9 s_surface;
  if (ppSurface) *ppSurface = &s_surface;
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9** pp, HANDLE) {
  static IDirect3DIndexBuffer9 s_ib;
  if (pp) *pp = &s_ib;
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9** pp, HANDLE) {
  static IDirect3DVertexBuffer9 s_vb;
  if (pp) *pp = &s_vb;
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9** pp, HANDLE*) {
  static IDirect3DTexture9 s_tex;
  if (pp) *pp = &s_tex;
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
inline HRESULT IDirect3DDevice9::CreateOffscreenPlainSurface(UINT, UINT, D3DFORMAT, D3DPOOL, IDirect3DSurface9** pp, HANDLE*) {
  static IDirect3DSurface9 s_surf;
  if (pp) *pp = &s_surf;
  return 0;
}
inline HRESULT IDirect3DDevice9::CreateDepthStencilSurface(UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9** pp, HANDLE*) {
  static IDirect3DSurface9 s_surf;
  if (pp) *pp = &s_surf;
  return 0;
}
inline HRESULT IDirect3DDevice9::GetRenderTarget(UINT, IDirect3DSurface9** pp) {
  static IDirect3DSurface9 s_surf;
  if (pp) *pp = &s_surf;
  return 0;
}
inline HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** pp) {
  static IDirect3DSurface9 s_surf;
  if (pp) *pp = &s_surf;
  return 0;
}
inline HRESULT IDirect3DDevice9::GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9** pp) {
  static IDirect3DSurface9 s_surf;
  if (pp) *pp = &s_surf;
  return 0;
}
inline HRESULT IDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9*, IDirect3DSurface9*) { return 0; }
inline HRESULT IDirect3DDevice9::SetRenderTarget(DWORD, IDirect3DSurface9*) { return 0; }
inline HRESULT IDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9*) { return 0; }
inline HRESULT IDirect3DDevice9::SetTexture(DWORD, IDirect3DBaseTexture9* tex) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (tex) {
    IDirect3DTexture9 *t = (IDirect3DTexture9*)tex;
    if (t->m_vulkanTex) {
      Platform_BindVulkanTexture(t->m_vulkanTex);
    }
  }
#endif
  return 0;
}
inline HRESULT IDirect3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE type, int, int, UINT num_vertices, UINT start_index, UINT prim_count) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (m_currentVB && m_currentIB && prim_count > 0 && m_currentVB->m_buffer.size() > 0 && m_currentIB->m_buffer.size() > 0) {
    uint32_t indexCount = (type == D3DPT_TRIANGLESTRIP) ? (prim_count + 2) : (prim_count * 3);
    uint32_t stride = m_vbStride ? m_vbStride : 32;
    const void *vData = m_currentVB->m_buffer.data() + m_vbOffset;
    const uint16_t *iData = (const uint16_t*)(m_currentIB->m_buffer.data() + start_index * sizeof(uint16_t));
    Platform_DrawVulkanIndexed(vData, num_vertices, stride, iData, indexCount);
  }
#endif
  return 0;
}
inline HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE type, UINT start_vertex, UINT prim_count) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (m_currentVB && prim_count > 0 && m_currentVB->m_buffer.size() > 0) {
    uint32_t vertexCount = (type == D3DPT_TRIANGLESTRIP) ? (prim_count + 2) : (prim_count * 3);
    uint32_t stride = m_vbStride ? m_vbStride : 32;
    const void *vData = m_currentVB->m_buffer.data() + m_vbOffset + start_vertex * stride;
    Platform_DrawVulkanVertices(vData, vertexCount, stride);
  }
#endif
  return 0;
}
inline HRESULT IDirect3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT prim_count, const void* pVertexStreamZeroData, UINT stride) {
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
  if (pVertexStreamZeroData && prim_count > 0) {
    uint32_t vertexCount = (type == D3DPT_TRIANGLESTRIP) ? (prim_count + 2) : (prim_count * 3);
    Platform_DrawVulkanVertices(pVertexStreamZeroData, vertexCount, stride ? stride : 32);
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
  HRESULT CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) { return 0; }
  HRESULT CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE, DWORD*) { return 0; }
  HRESULT CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) { return 0; }
  HRESULT GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS9* caps) {
    if (caps) {
      memset(caps, 0, sizeof(*caps));
      caps->DeviceType = D3DDEVTYPE_HAL;
      caps->Caps2 = 0x00020000L;
      caps->DevCaps = 0x00000020L | 0x00000010L;
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
