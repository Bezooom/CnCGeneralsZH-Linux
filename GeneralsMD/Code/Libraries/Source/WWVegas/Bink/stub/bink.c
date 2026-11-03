#include "bink.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct BinkInternal {
    BINK pub;
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
    int video_stream_idx;
    AVPacket *pkt;
    AVFrame *frame;
    struct SwsContext *sws_ctx;
    enum AVPixelFormat last_dst_fmt;
    int last_dst_w;
    int last_dst_h;
    int is_frame_ready;
} BinkInternal;

static void decode_next_frame(BinkInternal *b) {
    if (!b || !b->fmt_ctx || !b->codec_ctx) return;
    int got_frame = 0;

    while (!got_frame && av_read_frame(b->fmt_ctx, b->pkt) >= 0) {
        if (b->pkt->stream_index == b->video_stream_idx) {
            if (avcodec_send_packet(b->codec_ctx, b->pkt) == 0) {
                if (avcodec_receive_frame(b->codec_ctx, b->frame) == 0) {
                    got_frame = 1;
                }
            }
        }
        av_packet_unref(b->pkt);
    }

    if (!got_frame) {
        // EOF: loop back to beginning for looping video
        av_seek_frame(b->fmt_ctx, b->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(b->codec_ctx);
        while (!got_frame && av_read_frame(b->fmt_ctx, b->pkt) >= 0) {
            if (b->pkt->stream_index == b->video_stream_idx) {
                if (avcodec_send_packet(b->codec_ctx, b->pkt) == 0) {
                    if (avcodec_receive_frame(b->codec_ctx, b->frame) == 0) {
                        got_frame = 1;
                    }
                }
            }
            av_packet_unref(b->pkt);
        }
    }
    b->is_frame_ready = got_frame;
}

HBINK BINKCALL BinkOpen(const char *name, unsigned int flags) {
    (void)flags;
    if (!name || name[0] == '\0') return NULL;

    char path[1024];
    strncpy(path, name, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    for (char *p = path; *p; ++p) {
        if (*p == '\\') *p = '/';
    }

    const char *open_path = path;
    if (access(open_path, R_OK) != 0) {
        // Try relative to Run/ or GeneralsMD/Run/
        static char alt_path[1024];
        snprintf(alt_path, sizeof(alt_path), "GeneralsMD/Run/%s", path);
        if (access(alt_path, R_OK) == 0) {
            open_path = alt_path;
        } else {
            return NULL;
        }
    }

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, open_path, NULL, NULL) < 0) {
        return NULL;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    int video_stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_idx < 0) {
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    AVStream *stream = fmt_ctx->streams[video_stream_idx];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    if (avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0 ||
        avcodec_open2(codec_ctx, codec, NULL) < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    BinkInternal *b = (BinkInternal *)calloc(1, sizeof(BinkInternal));
    if (!b) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    b->fmt_ctx = fmt_ctx;
    b->codec_ctx = codec_ctx;
    b->video_stream_idx = video_stream_idx;
    b->pkt = av_packet_alloc();
    b->frame = av_frame_alloc();
    b->last_dst_fmt = AV_PIX_FMT_NONE;

    b->pub.Width = (unsigned int)codec_ctx->width;
    b->pub.Height = (unsigned int)codec_ctx->height;
    b->pub.Frames = stream->nb_frames > 0 ? (unsigned int)stream->nb_frames : 1000;
    b->pub.FrameNum = 1;
    b->pub.LastFrameNum = 0;
    b->pub.FrameRate = stream->avg_frame_rate.num > 0 ? (unsigned int)stream->avg_frame_rate.num : 30;
    b->pub.FrameRateDiv = stream->avg_frame_rate.den > 0 ? (unsigned int)stream->avg_frame_rate.den : 1;

    decode_next_frame(b);

    return &b->pub;
}

void BINKCALL BinkClose(HBINK bnk) {
    if (!bnk) return;
    BinkInternal *b = (BinkInternal *)bnk;
    if (b->sws_ctx) {
        sws_freeContext(b->sws_ctx);
        b->sws_ctx = NULL;
    }
    if (b->frame) {
        av_frame_free(&b->frame);
    }
    if (b->pkt) {
        av_packet_free(&b->pkt);
    }
    if (b->codec_ctx) {
        avcodec_free_context(&b->codec_ctx);
    }
    if (b->fmt_ctx) {
        avformat_close_input(&b->fmt_ctx);
    }
    free(b);
}

int BINKCALL BinkWait(HBINK bnk) {
    (void)bnk;
    return 0; // 0 = frame is ready
}

int BINKCALL BinkDoFrame(HBINK bnk) {
    if (!bnk) return 0;
    BinkInternal *b = (BinkInternal *)bnk;
    decode_next_frame(b);
    return 0;
}

void BINKCALL BinkNextFrame(HBINK bnk) {
    if (!bnk) return;
    bnk->LastFrameNum = bnk->FrameNum;
    bnk->FrameNum++;
    if (bnk->Frames > 0 && bnk->FrameNum > bnk->Frames) {
        bnk->FrameNum = 1;
    }
}

int BINKCALL BinkGoto(HBINK bnk, unsigned int frame, int flags) {
    (void)flags;
    if (!bnk) return 0;
    BinkInternal *b = (BinkInternal *)bnk;
    av_seek_frame(b->fmt_ctx, b->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(b->codec_ctx);
    bnk->FrameNum = frame;
    decode_next_frame(b);
    return 0;
}

int BINKCALL BinkCopyToBuffer(HBINK bnk, void *dest, int destpitch,
                              unsigned int destheight, unsigned int destx,
                              unsigned int desty, unsigned int flags)
{
    (void)destheight;
    if (!bnk || !dest) return 0;
    BinkInternal *b = (BinkInternal *)bnk;
    if (!b->frame || b->frame->width <= 0 || b->frame->height <= 0) return 0;

    enum AVPixelFormat dst_pix_fmt = AV_PIX_FMT_BGRA;
    int bpp = 4;
    unsigned int surfaceType = flags & 0x0F;

    switch (surfaceType) {
        case BINKSURFACE32:
            dst_pix_fmt = AV_PIX_FMT_BGRA;
            bpp = 4;
            break;
        case BINKSURFACE32R:
            dst_pix_fmt = AV_PIX_FMT_RGBA;
            bpp = 4;
            break;
        case BINKSURFACE24:
            dst_pix_fmt = AV_PIX_FMT_BGR24;
            bpp = 3;
            break;
        case BINKSURFACE24R:
            dst_pix_fmt = AV_PIX_FMT_RGB24;
            bpp = 3;
            break;
        case BINKSURFACE565:
            dst_pix_fmt = AV_PIX_FMT_RGB565LE;
            bpp = 2;
            break;
        case BINKSURFACE555:
            dst_pix_fmt = AV_PIX_FMT_RGB555LE;
            bpp = 2;
            break;
        default:
            dst_pix_fmt = AV_PIX_FMT_BGRA;
            bpp = 4;
            break;
    }

    int src_w = b->frame->width;
    int src_h = b->frame->height;

    if (!b->sws_ctx || b->last_dst_fmt != dst_pix_fmt ||
        b->last_dst_w != src_w || b->last_dst_h != src_h) {
        if (b->sws_ctx) sws_freeContext(b->sws_ctx);
        b->sws_ctx = sws_getContext(
            src_w, src_h, (enum AVPixelFormat)b->frame->format,
            src_w, src_h, dst_pix_fmt,
            SWS_FAST_BILINEAR, NULL, NULL, NULL);
        b->last_dst_fmt = dst_pix_fmt;
        b->last_dst_w = src_w;
        b->last_dst_h = src_h;
    }

    if (b->sws_ctx) {
        uint8_t *dst_data[4] = {
            (uint8_t *)dest + (size_t)desty * destpitch + (size_t)destx * bpp,
            NULL, NULL, NULL
        };
        int dst_linesize[4] = { destpitch, 0, 0, 0 };
        sws_scale(b->sws_ctx, (const uint8_t * const*)b->frame->data, b->frame->linesize,
                  0, src_h, dst_data, dst_linesize);
    }
    return 0;
}

int   BINKCALL BinkSetVolume(HBINK bnk, unsigned int trackid, int volume) {
    (void)bnk; (void)trackid; (void)volume;
    return 0;
}

void  BINKCALL BinkSetSoundTrack(unsigned int total_tracks, unsigned int *tracks) {
    (void)total_tracks; (void)tracks;
}

int   BINKCALL BinkSetSoundSystem(BINKSNDSYSOPEN open, BINKSNDSYSPARAM param) {
    (void)open; (void)param;
    return 0;
}

int   BINKCALL BinkOpenDirectSound(BINKSNDSYSPARAM param) {
    (void)param;
    return 0;
}
