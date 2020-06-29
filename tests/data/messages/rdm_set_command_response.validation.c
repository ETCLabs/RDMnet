#include "rdmnet/core/message.h"

// clang-format off

static RdmBuffer rdm_bufs[] = {
  {
    .data = { 0xcc, 0x01, 0x1a, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0xf0, 0x02, 0x00, 0x10, 0x07, 0x47 },
    .data_len = 28,
  },
  {
    .data = { 0xcc, 0x01, 0x18, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0xf0, 0x00, 0x07, 0x34 },
    .data_len = 26
  }
};

const RdmnetMessage rdm_set_command_response = {
  .vector = ACN_VECTOR_ROOT_RPT,
  .sender_cid = {
    .data = {0xde, 0xad, 0xbe, 0xef, 0xba, 0xad, 0xf0, 0x0d, 0xfa, 0xce, 0xb0, 0x0c, 0xd1, 0x5e, 0xea, 0x5e}
  },
  .data.rpt = {
    .vector = VECTOR_RPT_NOTIFICATION,
    .header = {
      .source_uid = { 0x1234, 0x5678aaaa },
      .source_endpoint_id = 0x0004,
      .dest_uid = { 0xfffc, 0xffffffff },
      .dest_endpoint_id = 0x0000,
      .seqnum = 0x12345678
    },
    .data.rdm = {
      .rdm_buffers = rdm_bufs,
      .num_rdm_buffers = (sizeof(rdm_bufs) / sizeof(RdmBuffer)),
      .more_coming = false
    }
  }
};