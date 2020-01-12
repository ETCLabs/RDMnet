#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage broker_null = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0x7a, 0xb5, 0x96, 0x7a, 0x17, 0x37, 0x48, 0x9b, 0x9b, 0xc8, 0x62, 0xa8, 0xea, 0x47, 0x9b, 0x6b }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_NULL
  }
};

