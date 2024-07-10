#include "gimi.h"


// Branding


// Content ID


// ISM


// Timestamp
int mov_write_tai_timestamp_packet(AVIOContext *pb, MOVMuxContext *mov, int frame_number) {
    // Variables
    uint64_t pos;
    uint64_t timestamp = frame_number; // TODO: get the timestamp, don't just use the frame number
    uint8_t flags = 0xAA;  // TODO: use the appropriate flags

    pos = avio_tell(pb);

    // Save Timestamp Position (for saio box)
    mov->timestamp_offsets[frame_number] = pos;
    avio_wb64(pb, timestamp);
    mov->mdat_size += sizeof(timestamp);

    avio_w8(pb, flags);
    mov->mdat_size += sizeof(flags);

    return TAITimestampPacketSize;

}

int mov_write_tai_timestamps(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {
    // Variables
    AVStream* stream = s->streams[0];
    int64_t frames = stream->nb_frames;

    // Allocate Timestamp Offsets
    mov->timestamp_offsets = (uint32_t*) malloc(frames * TAITimestampPacketSize);

    // Write Timestamps in mdat
    for (int i = 0; i < frames; i++) {
        mov_write_tai_timestamp_packet(pb, mov, i);
    }

    return 0;
}

int mov_write_taic_tag(AVIOContext *pb, MOVTrack *track) {
    // Variables
    int64_t pos = avio_tell(pb);
    uint64_t time_uncertainty = 0x1122334455667788;
    uint32_t clock_resolution = 0x100;
    int32_t clock_drift_rate = 0x200;
    uint8_t clock_type = 0x1;
    uint8_t reserved = 0;
    uint8_t output = (clock_type << 6) | reserved;

    avio_wb32(pb, 0); /* size */
    ffio_wfourcc(pb, "taic");

    // version(8) & flags(24)
    avio_wb32(pb, 0);

    avio_wb64(pb, time_uncertainty);

    avio_wb32(pb, clock_resolution);

    avio_wb32(pb, clock_drift_rate);


    avio_w8(pb, output);
    // unsigned int(2) clock_type; 
    // unsigned int(6) reserved = 0;


    // avio_wb32(pb, bit_rates.buffer_size);
    // avio_wb32(pb, bit_rates.max_bit_rate);
    // avio_wb32(pb, bit_rates.avg_bit_rate);

    return gimi_update_size(pb, pos);
}

int mov_write_itai_tag(AVIOContext *pb, MOVTrack *track) {
    // Variables
    int64_t pos = avio_tell(pb);
    uint64_t tai_timestamp = 0x1122334455667788;
    uint8_t synchronization_state = 0;
    uint8_t timestamp_generation_failure = 1;
    uint8_t timestamp_is_modified = 1;
    uint8_t status_bits = 0;

    avio_wb32(pb, 0); /* size */
    ffio_wfourcc(pb, "itai");

    // version(8) & flags(24)
    avio_wb32(pb, 0);

    avio_wb64(pb, tai_timestamp);



    status_bits |= (synchronization_state & 0x01) << 7;
    status_bits |= (timestamp_generation_failure & 0x01) << 6; 
    status_bits |= (timestamp_is_modified & 0x01) << 5; 
    avio_w8(pb, status_bits);

    return gimi_update_size(pb, pos);
}


// Sample Auxiliary
int mov_write_saiz_box(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {
    // Variables
    int64_t pos = avio_tell(pb);
    uint8_t version = 0;
    uint32_t flags = 1;
    uint32_t sample_count = (uint32_t) s->streams[0]->nb_frames;
    uint8_t default_sample_info_size = 5; // 4 byte timestamp + 1 byte flags

    avio_wb32(pb, 0); /* size update later */
    ffio_wfourcc(pb, "saiz");
    mov_write_fullbox(pb, version, flags);

    if (flags == 1) {
        ffio_wfourcc(pb, "stai"); // unsigned int(32) aux_info_type
        avio_wb32(pb, 0x0); // unsigned int(32) aux_info_type_parameter - 8-bit integer identifying a specific stream of sample auxiliary information.
    }

    // uint8_t default_sample_info_size = mov->timestamp_size;
    avio_w8(pb, default_sample_info_size);

    avio_wb32(pb, sample_count);

    if (default_sample_info_size == 0) {
        // unsigned int (8) sample_info_size[ sample_count ];
    }

    return gimi_update_size(pb, pos);  
}

int mov_write_saio_box(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {

  // Variables
  int64_t pos = avio_tell(pb);
  uint8_t version = 0;
  uint32_t flags = 1;
  uint32_t entry_count = (uint32_t) s->streams[0]->nb_frames;

  //Full Box
  avio_wb32(pb, 0); /* size update later */
  ffio_wfourcc(pb, "saio");
  mov_write_fullbox(pb, version, flags);

  if (flags == 1) {
      ffio_wfourcc(pb, "stai"); // unsigned int(32) aux_info_type
      avio_wb32(pb, 0x0);// unsigned int(32) aux_info_type_parameter
  }

  avio_wb32(pb, entry_count);

  if (version == 0) {
      for (int i = 0; i < entry_count; i++) {
          uint32_t offset = mov->timestamp_offsets[i];
          // uint32_t offset = i;
          avio_wb32(pb, offset);
      }
  }
  else {
      // unsigned int (64) offset[ entry_count ];
      printf("saio version %d not yet implemented\n", version);
      exit(1);
  }



  return gimi_update_size(pb, pos);  
}


// Utils
void mov_write_fullbox(AVIOContext * pb, uint8_t version, uint32_t flags) {
    // A full box has an 8-bit version and 24-bit flag. 
    uint32_t flags_and_version = (version << 24) | flags;
    avio_wb32(pb, flags_and_version); /* Version & flags */  //0x vv ff ff ff
}

int64_t gimi_update_size(AVIOContext *pb, int64_t pos) {
    int64_t curpos = avio_tell(pb);
    avio_seek(pb, pos, SEEK_SET);
    avio_wb32(pb, curpos - pos); /* rewrite size */
    avio_seek(pb, curpos, SEEK_SET);

    return curpos - pos;
}


