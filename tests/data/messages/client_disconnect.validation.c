#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage client_disconnect = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0x9a, 0xad, 0x1b, 0x1e, 0x32, 0xfa, 0x43, 0xd2, 0xae, 0x31, 0x39, 0x2a, 0xe8, 0x8b, 0x19, 0xa0 }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_DISCONNECT,
    .data.disconnect = {
      .disconnect_reason = kRdmnetDisconnectCapacityExhausted
    }
  }
};

