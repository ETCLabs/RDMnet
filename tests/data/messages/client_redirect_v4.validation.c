#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage client_redirect_v4 = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0xed, 0x8d, 0xee, 0x0c, 0xdf, 0xca, 0x4d, 0x29, 0xa5, 0x0a, 0xe0, 0x08, 0x1d, 0xd5, 0x67, 0xdf }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_REDIRECT_V4,
    .data.client_redirect = {
      .new_addr = {
        .ip = {
          .type = kEtcPalIpTypeV4,
          .addr.v4 = 0xc0a81337
        },
        .port = 0x8888
      }
    }
  }
};
