#include "rdmnet/core/message.h"

// clang-format off

static RdmUid uids[] = {
  {
    .manu = 0xa592,
    .id = 0x00000037
  },
  {
    .manu = 0xa592,
    .id = 0x00000038
  },
  {
    .manu = 0xa592,
    .id = 0x0000ffaa
  }
};

const RdmnetMessage fetch_dynamic_uid_assignment = {
  .vector = ACN_VECTOR_ROOT_BROKER,
  .sender_cid = {
    .data = { 0x17, 0x83, 0x1f, 0x54, 0x5a, 0x14, 0x47, 0x98, 0xa6, 0x72, 0x06, 0x7f, 0x42, 0x1f, 0xfb, 0x33 }
  },
  .data.broker = {
    .vector = VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST,
    .data.fetch_uid_assignment_list = {
      .uids = uids,
      .num_uids = 3,
      .more_coming = false
    }
  }
};
