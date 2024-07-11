#include "gimi.h"
#include <time.h>

// Function Prototypes
int64_t update_size(AVIOContext *pb, int64_t pos);


// Branding


// Content ID
uint8_t* gimi_generate_uuid() {
    // Allocate memory for the UUID
    uint8_t *uuid = (uint8_t *)malloc(16 * sizeof(uint8_t));
    if (uuid == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    // Seed the random number generator
    srand((unsigned)time(NULL));

    // Generate 16 random bytes
    for (int i = 0; i < 16; i++) {
        uuid[i] = rand() % 256;
    }

    // Set the version to 4 -> 0100xxxx
    uuid[6] = (uuid[6] & 0x0F) | 0x40;

    // Set the variant to RFC 4122 -> 10xxxxxx
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    return uuid;
}

void gimi_free_uuid(uint8_t *uuid) {
    free(uuid);
}


// ISM


// Timestamp
int gimi_write_tai_timestamp_packet(AVIOContext *pb, MOVMuxContext *mov, int frame_number) {
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

int gimi_write_tai_timestamps(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {
    // Variables
    AVStream* stream = s->streams[0];
    int64_t frames = stream->nb_frames;

    // Allocate Timestamp Offsets
    mov->timestamp_offsets = (uint32_t*) malloc(frames * TAITimestampPacketSize);

    // Write Timestamps in mdat
    for (int i = 0; i < frames; i++) {
        gimi_write_tai_timestamp_packet(pb, mov, i);
    }

    return 0;
}

int gimi_write_taic_tag(AVIOContext *pb, MOVTrack *track) {
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

    return update_size(pb, pos);
}

int gimi_write_itai_tag(AVIOContext *pb, MOVTrack *track) {
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

    return update_size(pb, pos);
}


// Sample Auxiliary
int gimi_write_saiz_box(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {
    // Variables
    int64_t pos = avio_tell(pb);
    uint8_t version = 0;
    uint32_t flags = 1;
    uint32_t sample_count = (uint32_t) s->streams[0]->nb_frames;
    uint8_t default_sample_info_size = 5; // 4 byte timestamp + 1 byte flags

    avio_wb32(pb, 0); /* size update later */
    ffio_wfourcc(pb, "saiz");
    gimi_write_fullbox(pb, version, flags);

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

    return update_size(pb, pos);  
}

int gimi_write_saio_box(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {

  // Variables
  int64_t pos = avio_tell(pb);
  uint8_t version = 0;
  uint32_t flags = 1;
  uint32_t entry_count = (uint32_t) s->streams[0]->nb_frames;

  //Full Box
  avio_wb32(pb, 0); /* size update later */
  ffio_wfourcc(pb, "saio");
  gimi_write_fullbox(pb, version, flags);

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



  return update_size(pb, pos);  
}


// Boxes
int64_t update_size(AVIOContext *pb, int64_t pos) {
    int64_t curpos = avio_tell(pb);
    avio_seek(pb, pos, SEEK_SET);
    avio_wb32(pb, curpos - pos); /* rewrite size */
    avio_seek(pb, curpos, SEEK_SET);

    return curpos - pos;
}
void gimi_write_fullbox(AVIOContext * pb, uint8_t version, uint32_t flags) {
    // A full box has an 8-bit version and 24-bit flag. 
    uint32_t flags_and_version = (version << 24) | flags;
    avio_wb32(pb, flags_and_version); /* Version & flags */  //0x vv ff ff ff
}


// Meta Boxes
int gimi_write_meta_box_in_track(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {
    // Holds Content IDs

    // Variables
    int size = 0;
    int64_t pos = avio_tell(pb);
    #define ITEM_COUNT 1
    struct infe items[ITEM_COUNT];

    avio_wb32(pb, 0); /* size */
    ffio_wfourcc(pb, "meta");
    gimi_write_fullbox(pb, 0, 0);

    gimi_write_hdlr_box(pb, mov, s);

    items[0].id = 1;
    items[0].item_type = "uri ";
    items[0].name = "Content ID for Parent Track";
    items[0].content_type = NULL;
    items[0].uri_type = URI_TYPE_CONTENT_ID;
    items[0].value = gimi_generate_uuid();
    items[0].size = UUID_SIZE;
    // items[0].size = strlen(items[0].value) + 1;
    items[0].construction_method = 1; //Store in the value of the content id in the idat as opposed to mdat

    gimi_write_idat(pb, items, ITEM_COUNT);

    gimi_write_iinf_box(pb, items, ITEM_COUNT);

    gimi_write_iloc_box(pb, items, ITEM_COUNT);

    size = update_size(pb, pos);
    return size;
}

int gimi_write_meta_box_in_moov(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {
  // Holds ISM Security Data
  return 0;
}

int gimi_write_hdlr_box(AVIOContext *pb, MOVMuxContext *mov, AVFormatContext *s) {
    avio_wb32(pb, 33); /* size */
    ffio_wfourcc(pb, "hdlr");
    avio_wb32(pb, 0);
    avio_wb32(pb, 0);
    ffio_wfourcc(pb, "meta");
    avio_wb32(pb, 0);
    avio_wb32(pb, 0);
    avio_wb32(pb, 0);
    avio_w8(pb, 0);
    return 33;
}

int gimi_write_idat(AVIOContext *pb, infe* items, uint32_t item_count) {
    // Extends Box
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0); /* size update later */
    ffio_wfourcc(pb, "idat");

    for (uint32_t i = 0; i < item_count; i++) {
        items[i].offset = avio_tell(pb);               // Save location in idat
        avio_write(pb, items[i].value, items[i].size);    // Write item in idat box
    }

    return update_size(pb, pos);  
}

int gimi_write_iinf_box(AVIOContext *pb, struct infe* items, uint32_t item_count) {

    // Variables
    int64_t iinf_pos = avio_tell(pb);

    avio_wb32(pb, 0); /* size */
    ffio_wfourcc(pb, "iinf");
    gimi_write_fullbox(pb, 0, 0); /* Version & flags */

    avio_wb16(pb, item_count); /* entry_count */

    for (int i = 0; i < item_count; i++) {
        gimi_add_infe_item(pb, items[i]); // stored in mdat
    }

    return update_size(pb, iinf_pos);
}

int gimi_add_infe_item(AVIOContext *pb, struct infe item) {

    // Variables
    int64_t infe_pos = avio_tell(pb);
    uint8_t version = 2;
    int is_uri = !strcmp(item.item_type, "uri ");
    int is_mime = !strcmp(item.item_type, "mime");

    avio_wb32(pb, 0); /* size */
    ffio_wfourcc(pb, "infe");
    gimi_write_fullbox(pb, version, 0); //Version & Flags
    avio_wb16(pb, item.id); /* item_id */
    avio_wb16(pb, 0); /* item_protection_index */
    avio_write(pb, item.item_type, 4); /* item_type */
    avio_write(pb, item.name, strlen(item.name)+1); /* item_name */

    if (is_uri) {
        avio_write(pb, item.uri_type, strlen(item.uri_type)+1); /* item_name */
    } else if (is_mime) {
        avio_write(pb, item.content_type, strlen(item.content_type)+1); /* item_name */
    }

    return update_size(pb, infe_pos);
}

int gimi_write_iloc_box(AVIOContext *pb, infe* items, uint32_t item_count) {
    //Full Box - Has a version & flags

    // Variables
    uint8_t version = 1; // Versions 1 & 2 Provide a Construction Method
    int64_t pos = avio_tell(pb);
    uint8_t offset_size = 4;        // 4 bits {0, 4, 8}
    uint8_t length_size = 4;        // 4 bits {0, 4, 8}
    uint8_t base_offset_size = 0;   // 4 bits {0, 4, 8}
    uint8_t index_size = 0;         // 4 bits {0, 4, 8}

    avio_wb32(pb, 0); /* size update later */
    ffio_wfourcc(pb, "iloc");
    gimi_write_fullbox(pb, version, 0); /* Version & flags */   

    avio_w8(pb, (offset_size      << 4) | length_size);
    avio_w8(pb, (base_offset_size << 4) | index_size); 

    if (version < 2)
        avio_wb16(pb, item_count);
    else
        avio_wb32(pb, item_count); 

    for (int i = 0; i < item_count; i++) {
        if (version < 2)
            avio_wb16(pb, items[i].id);
        else
            avio_wb32(pb, items[i].id); /* item_id */

        if (version == 1 || version == 2)
            avio_wb16(pb, items[i].construction_method); //12 bits reserved, 4 bits construction method

        avio_wb16(pb, 0); // data_reference_index

        if (base_offset_size == 0) {
            ; //Do Nothing
        } else if (base_offset_size == 4) {
            ; //avio_wb32() //TODO
        } else if (base_offset_size == 8) {
            ; //avio_wb64() //TODO
        }
        avio_wb16(pb, 1); // extent_count
        if ((version == 1 || version == 2)  &&  (index_size > 0)) {
            // unsigned int (index_size*8) item_reference_index
        }
        avio_wb32(pb, items[i].offset); // extent_offset
        avio_wb32(pb, items[i].size); // extent_length
    }

    return update_size(pb, pos);  
}

