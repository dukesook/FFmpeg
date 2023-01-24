
#include "libavutil/attributes.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/thread.h"
#include "codec_internal.h"
#include "mpegutils.h"
#include "mpegvideo.h"
#include "h263.h"
#include "h263enc.h"
#include "mpeg4video.h"
#include "mpeg4videodata.h"
#include "mpeg4videodefs.h"
#include "mpeg4videoenc.h"
#include "mpegvideoenc.h"
#include "profiles.h"
#include "version.h"


int ff_ngiis_encode_picture(AVCodecContext *avctx, AVPacket *pkt,
                          const AVFrame *pic_arg, int *got_packet);