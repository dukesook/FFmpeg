#include "gimi.h"
#include "gimi_ism_xml.h"
#include <time.h>

// Constants
const uint8_t EXTENDED_TYPE_CONTENT_ID[16] = {0x4a, 0x66, 0xef, 0xa7, 0xe5, 0x41, 0x52, 0x6c, 0x94, 0x27, 0x9e, 0x77, 0x61, 0x7f, 0xeb, 0x7d};
const size_t CONTENT_ID_SIZE = 16;
const size_t TAITimestampPacketSize = 9; // 8 byte timestamp + 1 byte flags
#define UUID_SIZE (16)
#define URI_TYPE_CONTENT_ID "urn:uuid:25d7f5a6-7a80-5c0f-b9fb-30f64edf2712";
#define DEBUG_FILENAME "out/output.txt"

// Function Prototypes
int64_t update_size(AVIOContext* pb, int64_t pos);

// Branding
int gimi_write_brands(AVIOContext* pb) {
  ffio_wfourcc(pb, "geo1");
  ffio_wfourcc(pb, "unif");
  return 8;
}

// Content ID
int gimi_write_track_content_ids_in_mdat(AVIOContext* pb, MOVMuxContext* mov, int64_t nb_frames) {
  // Variables
  uint64_t pos;
  size_t size; // size of all content ids
  uint8_t* content_id;

  // Allocate Content ID Offsets
  mov->content_id_offsets = (uint32_t*)malloc(nb_frames * sizeof(uint32_t));

  for (int i = 0; i < nb_frames; i++) {
    pos = avio_tell(pb);
    mov->content_id_offsets[i] = pos; // Save Content ID Position (for saio box)
    content_id = gimi_generate_uuid();
    avio_write(pb, content_id, CONTENT_ID_SIZE);
    mov->mdat_size += CONTENT_ID_SIZE;
    gimi_free_uuid(content_id);
  }

  size = nb_frames * CONTENT_ID_SIZE;
  return size;
}

int gimi_write_per_sample_content_ids(AVIOContext* pb, uint32_t* offsets, uint64_t content_id_count) {
  // Variables
  Box_saiz saiz;
  Box_saio saio;

  {
    saiz.aux_info_type = "suid";
    saiz.aux_info_type_parameter = 0x0;
    saiz.default_sample_info_size = CONTENT_ID_SIZE;
    saiz.sample_count = content_id_count;
    saiz.sample_info_sizes = NULL;
  }
  gimi_write_saiz_box(pb, &saiz);

  {
    saio.aux_info_type = "suid";
    saio.aux_info_type_parameter = 0x0;
    saio.entry_count = content_id_count;
    // saio.offsets = (uint64_t*)malloc(timestamp_count * sizeof(uint64_t));
    saio.offsets = offsets;
  }
  gimi_write_saio_box(pb, &saio);

  return 0;
}

uint8_t* gimi_generate_uuid() {
  // Variables
  uint8_t* uuid = (uint8_t*)malloc(16 * sizeof(uint8_t));
  if (uuid == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }

  // Seed the random number generator
  srand((unsigned)time(NULL));

  // Generate 16 random bytes
  for (int i = 0; i < 16; i++) {
    uuid[i] = rand() % 256;
    // uuid[i] = 0xFF;
  }

  // Set the version to 4 -> 0100xxxx
  uuid[6] = (uuid[6] & 0x0F) | 0x40;

  // Set the variant to RFC 4122 -> 10xxxxxx
  uuid[8] = (uuid[8] & 0x3F) | 0x80;

  // Temporarily Hard-Code for debugging
  for (int i = 0; i < 16; i++) {
    uuid[i] = 0xCC;
  }
  // Temporarily Hard-Code for debugging

  return uuid;
}

void gimi_free_uuid(uint8_t* uuid) {
  // free(uuid);
}

// TAI Timestamps
int gimi_write_timestamps_in_mdat(AVIOContext* pb, MOVMuxContext* mov, int64_t nb_frames) {
  // Variables
  TAITimestampPacket* timestamps;
  uint64_t pos;
  int timestamp_packet_size;

  // Allocate Timestamp Offsets
  mov->timestamp_offsets = (uint32_t*)malloc(nb_frames * TAITimestampPacketSize);

  // Write Timestamps in mdat
  timestamps = gimi_fabricate_tai_timestamps(nb_frames);
  for (int i = 0; i < nb_frames; i++) {
    pos = avio_tell(pb);
    mov->timestamp_offsets[i] = pos; // Save Timestamp Position (for saio box)
    timestamp_packet_size = gimi_write_tai_timestamp_packet(pb, &timestamps[i]);
    mov->mdat_size += timestamp_packet_size;
  }
  gimi_free_tai_timestamps(timestamps);

  return 0;
}

int gimi_write_per_sample_timestamps(AVIOContext* pb, uint32_t* offsets, uint64_t timestamp_count) {
  // Variables
  Box_saiz saiz;
  Box_saio saio;

  {
    saiz.aux_info_type = "atai";
    saiz.aux_info_type_parameter = 0x0;
    saiz.default_sample_info_size = 9; // 8 byte timestamp + 1 byte flags
    saiz.sample_count = timestamp_count;
    saiz.sample_info_sizes = NULL;
  }
  gimi_write_saiz_box(pb, &saiz);

  {
    saio.aux_info_type = "atai";
    saio.aux_info_type_parameter = 0x0;
    saio.entry_count = timestamp_count;
    // saio.offsets = (uint64_t*)malloc(timestamp_count * sizeof(uint64_t));
    saio.offsets = offsets;
  }
  gimi_write_saio_box(pb, &saio);

  gimi_write_to_file();

  return 0;
}

int gimi_write_tai_timestamp_packet(AVIOContext* pb, TAITimestampPacket* timestampPacket) {
  // Variables
  uint64_t timestamp = timestampPacket->tai_seconds;
  uint8_t synchronization_state = timestampPacket->synchronization_state;
  uint8_t timestamp_generation_failure = timestampPacket->timestamp_generation_failure;
  uint8_t timestamp_is_modified = timestampPacket->timestamp_is_modified;
  uint8_t status_bits = 0;

  avio_wb64(pb, timestamp);

  status_bits |= (synchronization_state & 0x01) << 7;
  status_bits |= (timestamp_generation_failure & 0x01) << 6;
  status_bits |= (timestamp_is_modified & 0x01) << 5;
  avio_w8(pb, status_bits);

  return TAITimestampPacketSize;
}

int gimi_write_taic_tag(AVIOContext* pb, MOVTrack* track) {
  // Variables
  int64_t pos = avio_tell(pb);
  uint8_t version = 0;
  uint32_t flags = 0;
  uint64_t time_uncertainty = 0x1122334455667788;
  uint32_t clock_resolution = 0x100;
  int32_t clock_drift_rate = 0x200;
  uint8_t clock_type = 0x1;
  uint8_t reserved = 0;
  uint8_t output = (clock_type << 6) | reserved;

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, "taic");

  gimi_write_fullbox(pb, version, flags);

  avio_wb64(pb, time_uncertainty);

  avio_wb32(pb, clock_resolution);

  avio_wb32(pb, clock_drift_rate);

  // unsigned int(2) clock_type;
  // unsigned int(6) reserved = 0;
  avio_w8(pb, output);

  return update_size(pb, pos);
}

TAITimestampPacket* gimi_fabricate_tai_timestamps(uint32_t timestamp_count) {
  // Variables
  TAITimestampPacket* timestamps = (TAITimestampPacket*)malloc(timestamp_count * sizeof(TAITimestampPacket));
  uint64_t base_timestamp = 0x7777777777777777;

  for (uint32_t i = 0; i < timestamp_count; i++) {
    timestamps[i].tai_seconds = base_timestamp + (i * 0);
    timestamps[i].synchronization_state = 0;
    timestamps[i].timestamp_generation_failure = 1;
    timestamps[i].timestamp_is_modified = 1;
  }

  return timestamps;
}

void gimi_free_tai_timestamps(TAITimestampPacket* timestamps) {
  free(timestamps);
}

// Sample Auxiliary
int gimi_write_saiz_box(AVIOContext* pb, Box_saiz* saiz) {
  // Variables
  int64_t pos = avio_tell(pb);
  uint8_t version = 0;
  uint32_t flags = 1;

  avio_wb32(pb, 0); /* size update later */
  ffio_wfourcc(pb, "saiz");
  gimi_write_fullbox(pb, version, flags);

  if (flags == 1) {
    ffio_wfourcc(pb, saiz->aux_info_type);
    avio_wb32(pb, 0x0); // unsigned int(32) aux_info_type_parameter - 8-bit integer identifying a specific stream of sample auxiliary information.
  }

  avio_w8(pb, saiz->default_sample_info_size);

  avio_wb32(pb, saiz->sample_count);

  if (saiz->default_sample_info_size == 0) {
    // TODO
    // unsigned int (8) sample_info_size[ sample_count ];
  }

  return update_size(pb, pos);
}

int gimi_write_saio_box(AVIOContext* pb, Box_saio* saio) {
  // Variables
  int64_t pos = avio_tell(pb);
  uint8_t version = 0;
  uint32_t flags = 1;

  // Full Box
  avio_wb32(pb, 0); /* size update later */
  ffio_wfourcc(pb, "saio");
  gimi_write_fullbox(pb, version, flags);

  if (flags == 1) {
    ffio_wfourcc(pb, saio->aux_info_type); // unsigned int(32) aux_info_type
    avio_wb32(pb, 0x0);                    // unsigned int(32) aux_info_type_parameter
  }

  avio_wb32(pb, saio->entry_count);

  if (version == 0) {
    for (int i = 0; i < saio->entry_count; i++) {
      // uint32_t offset = mov->timestamp_offsets[i];
      uint32_t offset = saio->offsets[i];
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
int64_t update_size(AVIOContext* pb, int64_t pos) {
  // Variables
  int64_t curpos = avio_tell(pb);
  avio_seek(pb, pos, SEEK_SET);
  avio_wb32(pb, curpos - pos); /* rewrite size */
  avio_seek(pb, curpos, SEEK_SET);

  return curpos - pos;
}

void gimi_write_fullbox(AVIOContext* pb, uint8_t version, uint32_t flags) {
  // A full box has an 8-bit version and 24-bit flag.
  uint32_t flags_and_version = (version << 24) | flags;
  avio_wb32(pb, flags_and_version); /* Version & flags */ // 0x vv ff ff ff
}

// Meta Boxes
int gimi_write_meta_box_top_level(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s) {
// Variables
#define ITEM_COUNT_TOP_LEVEL 1
#define PROPERTY_COUNT_TOP_LEVEL 1
#define ASSOCIATION_COUNT_TOP_LEVEL 1
  int size = 0;
  int64_t pos = avio_tell(pb);
  struct infe items[ITEM_COUNT_TOP_LEVEL];
  struct Box properties[PROPERTY_COUNT_TOP_LEVEL];
  struct Association associations[ASSOCIATION_COUNT_TOP_LEVEL];
  uint8_t* content_id = gimi_generate_uuid();

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, "meta");
  gimi_write_fullbox(pb, 0, 0);

  gimi_write_hdlr_box(pb, mov, s);

  // Create Items
  {
    items[0].id = 1;
    items[0].item_type = "mime";
    items[0].name = "ODNI ISM XML Security Marking";
    items[0].content_type = "application/dni-arh+xml";
    items[0].uri_type = NULL;
    items[0].value = ism_xml;
    items[0].size = strlen(items[0].value) + 1;
    items[0].construction_method = 1; // store in idat
  }

  // Create Properties
  // memcpy(payload, &content_id, CONTENT_ID_SIZE);
  // gimi_free_uuid(content_id);
  {
    properties[0].fourcc = "uuid";
    memcpy(properties[0].extended_type, EXTENDED_TYPE_CONTENT_ID, 16);
    properties[0].payload = content_id;
    properties[0].payload_size = CONTENT_ID_SIZE;
  }

  // Create Item<->Property Associations
  {
    associations[0].item_id = 1;
    associations[0].property_count = 1;
    associations[0].property_ids = (uint16_t*)malloc(sizeof(uint16_t));
    associations[0].property_ids[0] = 1;
  }

  gimi_write_idat(pb, items, ITEM_COUNT_TOP_LEVEL);

  gimi_write_iinf_box(pb, items, ITEM_COUNT_TOP_LEVEL);

  gimi_write_iprp_box(pb, properties, PROPERTY_COUNT_TOP_LEVEL, associations, ASSOCIATION_COUNT_TOP_LEVEL);

  // gimi_write_iref_box(pb, mov, s);

  gimi_write_iloc_box(pb, items, ITEM_COUNT_TOP_LEVEL);

  size = update_size(pb, pos);
  return size;
}

int gimi_write_meta_box_in_track(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s) {
  // Holds Content IDs

  // Variables
  int size = 0;
  int64_t pos = avio_tell(pb);
#define ITEM_COUNT 1
#define PROPERTY_COUNT 0
#define ASSOCIATION_COUNT 0
  struct infe items[ITEM_COUNT];
  uint64_t content_id[] = {0xCCCCCCCCCCCCCCCC, 0xCCCCCCCCCCCCCCCC};

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, "meta");
  gimi_write_fullbox(pb, 0, 0);

  gimi_write_hdlr_box(pb, mov, s);

  // Create Items
  {
    items[0].id = 1;
    items[0].item_type = "uri ";
    items[0].name = "Content ID for Parent Track";
    items[0].content_type = NULL;
    items[0].uri_type = URI_TYPE_CONTENT_ID;
    // items[0].value = gimi_generate_uuid();
    items[0].value = content_id;
    items[0].size = UUID_SIZE;
    items[0].construction_method = 1; // Store in the value of the content id in the idat as opposed to mdat
  }

  gimi_write_idat(pb, items, ITEM_COUNT);

  gimi_write_iinf_box(pb, items, ITEM_COUNT);

  gimi_write_iloc_box(pb, items, ITEM_COUNT);

  size = update_size(pb, pos);
  return size;
}

int gimi_write_hdlr_box(AVIOContext* pb, MOVMuxContext* mov, AVFormatContext* s) {
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

int gimi_write_idat(AVIOContext* pb, infe* items, uint32_t item_count) {
  // Variables
  int64_t pos = avio_tell(pb);
  avio_wb32(pb, 0); /* size update later */
  ffio_wfourcc(pb, "idat");

  for (uint32_t i = 0; i < item_count; i++) {
    items[i].offset = avio_tell(pb);               // Save location in idat
    avio_write(pb, items[i].value, items[i].size); // Write item in idat box
  }

  return update_size(pb, pos);
}

int gimi_write_iinf_box(AVIOContext* pb, struct infe* items, uint32_t item_count) {
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

int gimi_add_infe_item(AVIOContext* pb, struct infe item) {
  // Variables
  int64_t infe_pos = avio_tell(pb);
  uint8_t version = 2;
  int is_uri = !strcmp(item.item_type, "uri ");
  int is_mime = !strcmp(item.item_type, "mime");

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, "infe");
  gimi_write_fullbox(pb, version, 0);               // Version & Flags
  avio_wb16(pb, item.id);                           /* item_id */
  avio_wb16(pb, 0);                                 /* item_protection_index */
  avio_write(pb, item.item_type, 4);                /* item_type */
  avio_write(pb, item.name, strlen(item.name) + 1); /* item_name */

  if (is_uri) {
    avio_write(pb, item.uri_type, strlen(item.uri_type) + 1); /* item_name */
  }
  else if (is_mime) {
    avio_write(pb, item.content_type, strlen(item.content_type) + 1); /* item_name */
  }

  return update_size(pb, infe_pos);
}

int gimi_write_iloc_box(AVIOContext* pb, infe* items, uint32_t item_count) {
  // Full Box - Has a version & flags

  // Variables
  uint8_t version = 1; // Versions 1 & 2 Provide a Construction Method
  int64_t pos = avio_tell(pb);
  uint8_t offset_size = 4;      // 4 bits {0, 4, 8}
  uint8_t length_size = 4;      // 4 bits {0, 4, 8}
  uint8_t base_offset_size = 0; // 4 bits {0, 4, 8}
  uint8_t index_size = 0;       // 4 bits {0, 4, 8}

  avio_wb32(pb, 0); /* size update later */
  ffio_wfourcc(pb, "iloc");
  gimi_write_fullbox(pb, version, 0); /* Version & flags */

  avio_w8(pb, (offset_size << 4) | length_size);
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
      avio_wb16(pb, items[i].construction_method); // 12 bits reserved, 4 bits construction method

    avio_wb16(pb, 0); // data_reference_index

    if (base_offset_size == 0) {
      ; // Do Nothing
    }
    else if (base_offset_size == 4) {
      ; // avio_wb32() //TODO
    }
    else if (base_offset_size == 8) {
      ; // avio_wb64() //TODO
    }
    avio_wb16(pb, 1); // extent_count
    if ((version == 1 || version == 2) && (index_size > 0)) {
      // unsigned int (index_size*8) item_reference_index
    }
    avio_wb32(pb, items[i].offset); // extent_offset
    avio_wb32(pb, items[i].size);   // extent_length
  }

  return update_size(pb, pos);
}

int gimi_write_iprp_box(AVIOContext* pb, struct Box* properties, size_t property_count, Association* associations, size_t association_count) {
  // Variables
  int size = 0;
  int64_t pos = avio_tell(pb);

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, "iprp");

  gimi_write_ipco_box(pb, properties, property_count);

  gimi_write_ipma_box(pb, associations, association_count);

  size = update_size(pb, pos);
  return size;
}

int gimi_write_ipco_box(AVIOContext* pb, struct Box* properties, size_t property_count) {
  // Variables
  int size = 0;
  int64_t pos = avio_tell(pb);

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, "ipco");

  for (uint32_t i = 0; i < property_count; i++) {
    gimi_write_box(pb, properties[i]);
  }

  size = update_size(pb, pos);
  return size;
}

int gimi_write_ipma_box(AVIOContext* pb, Association* associations, size_t association_count) {
  // Variables
  int size = 0;
  int64_t pos = avio_tell(pb);
  uint8_t version = 1;
  uint32_t flags = 0;

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, "ipma");

  gimi_write_fullbox(pb, version, flags);

  avio_wb32(pb, association_count); // entry_count

  for (uint32_t i = 0; i < association_count; i++) {
    Association ass = associations[i];
    if (version < 1) {
      avio_wb16(pb, ass.item_id);
    }
    else {
      avio_wb32(pb, ass.item_id);
    }
    avio_w8(pb, ass.property_count);
    for (int j = 0; j < ass.property_count; j++) {
      if (flags & 1) {
        uint16_t property_id = ass.property_ids[j];
        // TODO: the msb indicates 'essential'
        avio_wb16(pb, property_id);
      }
      else {
        // TODO: the msb indicates 'essential'
        uint8_t property_id = (uint8_t)ass.property_ids[j];
        avio_w8(pb, property_id);
      }
    }
  }

  size = update_size(pb, pos);
  return size;
}

int gimi_write_box(AVIOContext* pb, struct Box box) {
  // Variables
  int size = 0;
  int64_t pos = avio_tell(pb);

  // uint32_t box_size = 8 + box.payload_size;

  avio_wb32(pb, 0); /* size */
  ffio_wfourcc(pb, box.fourcc);

  if (!strcmp(box.fourcc, "uuid")) {
    avio_write(pb, box.extended_type, 16);
  }

  avio_write(pb, box.payload, box.payload_size);

  size = update_size(pb, pos);
  return size;
}

// Debug
void gimi_write_to_file() {

  const char* text = "Hello, World!";
  FILE* file = fopen("out/output.txt", "w");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  fprintf(file, "%s\n", text);

  fclose(file);

  gimi_log("1");
  gimi_log("2");
  gimi_log("3\n");
  gimi_log("4");
}

void gimi_log(const char* message) {
  FILE* file = fopen(DEBUG_FILENAME, "a");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }
  fprintf(file, "%s", message);
  fclose(file);
}

void gimi_clear_log() {
  int error = remove(DEBUG_FILENAME);
  if (error) {
    perror("Error deleting file");
    exit(1);
  }
}