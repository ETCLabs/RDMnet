#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage rpt_status_string_absent = {
  .vector = ACN_VECTOR_ROOT_RPT,
  .sender_cid = {
    .data = { 0x31, 0x09, 0x92, 0x97, 0x4a, 0xd7, 0x47, 0xb2, 0x8d, 0x1c, 0x85, 0xf4, 0x4b, 0x0e, 0x65, 0x34 }
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
      .status_code = kRptStatusRdmTimeout,
      .status_string = NULL
    }
  }
};
