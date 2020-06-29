#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage fetch_client_list = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0x15, 0xdb, 0xe4, 0xa5, 0x6b, 0x70, 0x41, 0x55, 0x85, 0x99, 0xbc, 0x5b, 0xd3, 0x78, 0x79, 0x3f }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_FETCH_CLIENT_LIST
  }
};

