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

typedef struct infe {
    uint32_t id;                 // Starts at 1
    const char* item_type;       // mime, uri , etc, 
    const char* name;            // Optional, Human readable description
    const char* content_type;    // Used when item_type='mime', identifies the type of data
    const char* uri_type;        // Used when item_type='uri ', identifies which type of uri item
    void* value;                 // The actual data to be stored into the idat
    uint32_t size;               // The size in bytes of void* value
    uint32_t offset;             // The number of bytes from the beginning of the idat to the first byte of data
    uint8_t construction_method; // 0=mdat  1=idat  2=item
} infe;

// Branding

// Content ID
uint8_t* gimi_generate_uuid(void); // 16 bytes (128 bits)
void gimi_free_uuid(uint8_t *uuid);
#define UUID_SIZE (16)
#define URI_TYPE_CONTENT_ID "urn:uuid:25d7f5a6-7a80-5c0f-b9fb-30f64edf2712";

// ISM

// Tai Timestamps
int gimi_write_tai_timestamp_packet(AVIOContext* pb, MOVMuxContext* mov, int frame_number);
int gimi_write_tai_timestamps(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s);
int gimi_write_taic_tag(AVIOContext* pb, MOVTrack* track);
int gimi_write_itai_tag(AVIOContext* pb, MOVTrack* track);
#define TAITimestampPacketSize  (5)

// Sample Auxiliary
int gimi_write_saiz_box(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s);
int gimi_write_saio_box(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s);

// Boxes
// int64_t update_size(AVIOContext *pb, int64_t pos);
void gimi_write_fullbox(AVIOContext* pb, uint8_t version, uint32_t flags);

// Meta Boxes
int gimi_write_meta_box_in_track(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s);
int gimi_write_meta_box_in_moov(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s);
int gimi_write_hdlr_box(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s);
int gimi_write_idat(AVIOContext *pb, infe* items, uint32_t item_count);
int gimi_write_iinf_box(AVIOContext *pb, struct infe* items, uint32_t item_count);
int gimi_add_infe_item(AVIOContext *pb, struct infe item);
int gimi_write_iloc_box(AVIOContext *pb, infe* items, uint32_t item_count);

#endif /* AVFORMAT_AVIO_H */
