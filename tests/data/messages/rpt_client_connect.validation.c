#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage rpt_client_connect = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = {0xfa, 0x5e, 0x3d, 0x7b, 0x21, 0xc4, 0x4b, 0x68, 0x8b, 0x9f, 0xe5, 0xfe, 0x43, 0x67, 0xd6, 0x7e}
  },
  .data.broker = {
    .vector = VECTOR_BROKER_CONNECT,
    .data.client_connect = {
      .scope = "¿½¢ Iluminación?",
      .e133_version = 1,
      .search_domain = "example.etclink.net.",
      .connect_flags = 0x01,
      .client_entry = {
        .client_protocol = kClientProtocolRPT,
        .data.rpt = {
          .cid = {
            .data = {0xfa, 0x5e, 0x3d, 0x7b, 0x21, 0xc4, 0x4b, 0x68, 0x8b, 0x9f, 0xe5, 0xfe, 0x43, 0x67, 0xd6, 0x7e}
          },
          .uid = { 0x6574, 0xea45b652 },
          .type = kRPTClientTypeController,
          .binding_cid = {{0}}
        }
      }
    }
  }
};
