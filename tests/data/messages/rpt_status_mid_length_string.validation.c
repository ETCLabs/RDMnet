#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage rpt_status_mid_length_string = {
  .vector = ACN_VECTOR_ROOT_RPT,
  .sender_cid = {
    .data = { 0x69, 0xbc, 0x7b, 0x44, 0xcb, 0x21, 0x42, 0xf8, 0xa3, 0x7d, 0xaa, 0x1a, 0x43, 0x65, 0x35, 0x72 }
  },
  .data.rpt = {
    .vector = VECTOR_RPT_STATUS,
    .header = {
      .source_uid = { 0x1234, 0x5678aaaa },
      .source_endpoint_id = 0x0000,
      .dest_uid = { 0xcba9, 0x87654321 },
      .dest_endpoint_id = 0x0000,
      .seqnum = 0x12345678
    },
    .data.status = {
      .status_code = kRptStatusUnknownRdmUid,
      .status_string = "Something went wrong!"
    }
  }
};
