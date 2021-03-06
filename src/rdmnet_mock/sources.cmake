# RDMnet mock source manifest

set(RDMNET_MOCK_DISCOVERY_HEADERS
  ${RDMNET_INCLUDE}/rdmnet_mock/discovery.h
  ${RDMNET_SRC}/rdmnet_mock/disc/common.h
)
set(RDMNET_MOCK_DISCOVERY_SOURCES
  ${RDMNET_SRC}/rdmnet_mock/discovery.c
  ${RDMNET_SRC}/rdmnet_mock/disc/common.c
)
set(RDMNET_MOCK_CORE_HEADERS
  ${RDMNET_SRC}/rdmnet_mock/core/broker_prot.h
  ${RDMNET_SRC}/rdmnet_mock/core/client.h
  ${RDMNET_SRC}/rdmnet_mock/core/common.h
  ${RDMNET_SRC}/rdmnet_mock/core/connection.h
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.h
  ${RDMNET_SRC}/rdmnet_mock/core/llrp.h
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_manager.h
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_target.h
  ${RDMNET_SRC}/rdmnet_mock/core/message.h
  ${RDMNET_SRC}/rdmnet_mock/core/msg_buf.h
  ${RDMNET_SRC}/rdmnet_mock/core/rpt_prot.h
)
set(RDMNET_MOCK_CORE_SOURCES
  ${RDMNET_SRC}/rdmnet_mock/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet_mock/core/client.c
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_SRC}/rdmnet_mock/core/connection.c
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_manager.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet_mock/core/message.c
  ${RDMNET_SRC}/rdmnet_mock/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet_mock/core/rpt_prot.c
  # Real sources which don't need to be mocked
  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet/core/client_entry.c
)
set(RDMNET_MOCK_API_HEADERS
  ${RDMNET_INCLUDE}/rdmnet_mock/controller.h
  ${RDMNET_INCLUDE}/rdmnet_mock/common.h
  ${RDMNET_INCLUDE}/rdmnet_mock/device.h
  ${RDMNET_INCLUDE}/rdmnet_mock/llrp_manager.h
)
set(RDMNET_MOCK_API_SOURCES
  ${RDMNET_SRC}/rdmnet_mock/controller.c
  ${RDMNET_SRC}/rdmnet_mock/common.c
  ${RDMNET_SRC}/rdmnet_mock/device.c
  ${RDMNET_SRC}/rdmnet_mock/llrp_manager.c
)

set(RDMNET_MOCK_ALL_HEADERS
  ${RDMNET_MOCK_DISCOVERY_HEADERS}
  ${RDMNET_MOCK_CORE_HEADERS}
  ${RDMNET_MOCK_API_HEADERS}
)
set(RDMNET_MOCK_ALL_SOURCES
  ${RDMNET_MOCK_DISCOVERY_SOURCES}
  ${RDMNET_MOCK_CORE_SOURCES}
  ${RDMNET_MOCK_API_SOURCES}
)
