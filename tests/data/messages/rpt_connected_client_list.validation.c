#include "rdmnet/core/message.h"

// clang-format off

static RptClientEntry client_entries[] = {
  {
    .cid = {
      .data = {0xfa, 0x5e, 0x3d, 0x7b, 0x21, 0xc4, 0x4b, 0x68, 0x8b, 0x9f, 0xe5, 0xfe, 0x43, 0x67, 0xd6, 0x7e}
    },
    .uid = { 0x6574, 0xea45b652 },
    .type = kRPTClientTypeController,
    .binding_cid = {{0}}
  }
};

const RdmnetMessage rpt_connected_client_list = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0xc9, 0x57, 0xa9, 0xe5, 0x72, 0xb3, 0x45, 0x5b, 0xba, 0x4f, 0x5b, 0x00, 0xcd, 0xc6, 0xfb, 0x57 }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_CONNECTED_CLIENT_LIST,
    .data.client_list = {
      .client_protocol = kClientProtocolRPT,
      .data.rpt = {
        .more_coming = false,
        .num_client_entries = 1,
        .client_entries = client_entries
      }
    }
  }
};

