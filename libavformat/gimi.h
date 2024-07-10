#ifndef GIMI_H
#define GIMI_H

#include "av1.h"
#include "avc.h"
#include "avformat.h"
#include "avio.h"
#include "avio_internal.h"
#include "config_components.h"
#include "dovi_isom.h"
#include "evc.h"
#include "hevc.h"
#include "iamf_writer.h"
#include "internal.h"
#include "isom.h"
#include "libavcodec/ac3_parser_internal.h"
#include "libavcodec/dnxhddata.h"
#include "libavcodec/flac.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/internal.h"
#include "libavcodec/put_bits.h"
#include "libavcodec/raw.h"
#include "libavcodec/vc1_common.h"
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/csp.h"
#include "libavutil/dict.h"
#include "libavutil/dovi_meta.h"
#include "libavutil/intfloat.h"
#include "libavutil/libm.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/stereo3d.h"
#include "libavutil/timecode.h"
#include "libavutil/uuid.h"
#include "mov_chan.h"
#include "movenc.h"
#include "movenc_ttml.h"
#include "mux.h"
#include "nal.h"
#include "rawutils.h"
#include "riff.h"
#include "rtpenc.h"
#include "ttmlenc.h"
#include "version.h"
#include "vpcc.h"
#include "vvc.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

// Branding

// Content ID

// ISM

// Timestamp
int mov_write_tai_timestamp_packet(AVIOContext* pb, MOVMuxContext* mov, int frame_number);
int mov_write_tai_timestamps(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s);
int mov_write_taic_tag(AVIOContext* pb, MOVTrack* track);
int mov_write_itai_tag(AVIOContext* pb, MOVTrack* track);
#define TAITimestampPacketSize  (5)


// Sample Auxiliary
int mov_write_saiz_box(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s);
int mov_write_saio_box(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s);

// Utils
void mov_write_fullbox(AVIOContext* pb, uint8_t version, uint32_t flags);
int64_t gimi_update_size(AVIOContext *pb, int64_t pos);


#endif /* AVFORMAT_AVIO_H */
