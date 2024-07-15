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
  const void* value;           // The actual data to be stored into the idat
  uint32_t size;               // The size in bytes of void* value
  uint32_t offset;             // The number of bytes from the beginning of the idat to the first byte of data
  uint8_t construction_method; // 0=mdat  1=idat  2=item
} infe;

typedef struct Box {
  // uint32_t box_size; // size (4 bytes) + fourcc (4 bytes) + payload (n bytes)
  const char* fourcc;
  uint8_t extended_type[16]; // when foucc='uuid', the extended type is used to identify the payload.
  size_t payload_size;
  uint8_t* payload;
} Box;

typedef struct Association {
  uint32_t item_id;       // The item id that has properties associated with it.
  uint8_t property_count; // The number of property ids associated with the item id.
  uint16_t* property_ids; // The msb is the 'essential' bit
  // uint8_t* essential;  // TODO: the msb of the property_id is the 'essential' bit
} Association;

typedef struct TAITimestampPacket {
  uint64_t tai_seconds;
  uint8_t synchronization_state;
  uint8_t timestamp_generation_failure;
  uint8_t timestamp_is_modified;

} TAITimestampPacket;

typedef struct Box_saiz {
  const char* aux_info_type;
  uint32_t aux_info_type_parameter;
  uint8_t default_sample_info_size;
  uint32_t sample_count;
  uint8_t* sample_info_sizes;
} Box_saiz;

typedef struct Box_saio {
  const char* aux_info_type;
  uint32_t aux_info_type_parameter;
  uint32_t entry_count;
  uint32_t* offsets;
} Box_saio;

// Branding
int gimi_write_brands(AVIOContext* pb);

// Content ID
int gimi_write_track_content_ids_in_mdat(AVIOContext* pb, MOVMuxContext* mov, int64_t nb_frames);
uint8_t* gimi_generate_uuid(void); // 16 bytes (128 bits)
void gimi_free_uuid(uint8_t *uuid);

// TAI Timestamps
int gimi_write_timestamps_in_mdat(AVIOContext* pb, MOVMuxContext* mov, int64_t nb_frames);
int gimi_write_per_sample_timestamps(AVIOContext* pb, uint32_t* offsets, uint64_t timestamp_count);
int gimi_write_tai_timestamp_packet(AVIOContext* pb, TAITimestampPacket* timestamp);
int gimi_write_taic_tag(AVIOContext* pb, MOVTrack* track);
TAITimestampPacket* gimi_fabricate_tai_timestamps(uint32_t timestamp_count);
void gimi_free_tai_timestamps(TAITimestampPacket* timestamps);

// Sample Auxiliary
int gimi_write_per_sample_content_ids(AVIOContext* pb, uint32_t* offsets, uint64_t content_id_count);
int gimi_write_saiz_box(AVIOContext* pb, Box_saiz* saiz);
int gimi_write_saio_box(AVIOContext* pb, Box_saio* saio);

// Boxes
// int64_t update_size(AVIOContext *pb, int64_t pos);
void gimi_write_fullbox(AVIOContext* pb, uint8_t version, uint32_t flags);

// Meta Boxes
int gimi_write_meta_box_top_level(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s);
int gimi_write_meta_box_in_track(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s);
int gimi_write_hdlr_box(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s);
int gimi_write_idat(AVIOContext *pb, infe* items, uint32_t item_count);
int gimi_write_iinf_box(AVIOContext *pb, struct infe* items, uint32_t item_count);
int gimi_add_infe_item(AVIOContext *pb, struct infe item);
int gimi_write_iloc_box(AVIOContext *pb, infe* items, uint32_t item_count);
int gimi_write_iprp_box(AVIOContext *pb, struct Box* properties, size_t property_count, Association* associations, size_t association_count);
int gimi_write_ipco_box(AVIOContext *pb, struct Box* properties, size_t property_count);
int gimi_write_ipma_box(AVIOContext *pb,  Association* associations, size_t association_count);
int gimi_write_box(AVIOContext *pb, struct Box box);

// Debug
void gimi_write_to_file(void);
void gimi_log(const char* message);
void gimi_clear_log(void);

#endif /* GIMI_H */
