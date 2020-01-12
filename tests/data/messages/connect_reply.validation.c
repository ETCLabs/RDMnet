#include "rdmnet/core/message.h"

// clang-format off

const RdmnetMessage connect_reply = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0x90, 0x17, 0x9f, 0xcb, 0x9e, 0xd2, 0x46, 0x1f, 0x9b, 0x0b, 0x65, 0x9b, 0xdc, 0x55, 0xe4, 0xf0 }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_CONNECT_REPLY,
    .data.connect_reply = {
      .connect_status = kRdmnetConnectOk,
      .e133_version = 1,
      .broker_uid = { 0x7a82, 0x1816a8c0 },
      .client_uid = { 0x594a, 0x6629ab59 }
    }
  }
};
