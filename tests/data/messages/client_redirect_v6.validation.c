#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage client_redirect_v6 = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0x4b, 0xc3, 0x43, 0x0c, 0x5b, 0xcf, 0x47, 0x7c, 0xaa, 0xd0, 0x5f, 0x70, 0x18, 0xe6, 0x52, 0xd3 }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_REDIRECT_V6,
    .data.client_redirect = {
      .new_addr = {
        .ip = {
          .type = kEtcPalIpTypeV6,
          .addr.v6 = {
            .addr_buf = { 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xbb, 0xaa, 0xbb, 0x11, 0x22, 0x11, 0x22 },
            .scope_id = 0
          }
        },
        .port = 0x1578
      }
    }
  }
};
