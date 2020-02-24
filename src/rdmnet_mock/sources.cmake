# RDMnet mock source manifest

set(RDMNET_MOCK_DISCOVERY_HEADERS
  ${RDMNET_INCLUDE}/rdmnet_mock/discovery.h
)
set(RDMNET_MOCK_DISCOVERY_SOURCES
  ${RDMNET_SRC}/rdmnet_mock/discovery.c
)
set(RDMNET_MOCK_CORE_HEADERS
  ${RDMNET_INCLUDE}/rdmnet_mock/core.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/broker_prot.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/connection.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/llrp_target.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/rpt_prot.h
)
set(RDMNET_MOCK_CORE_SOURCES
  ${RDMNET_SRC}/rdmnet_mock/core.c
  ${RDMNET_SRC}/rdmnet_mock/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet_mock/core/connection.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet_mock/core/rpt_prot.c
  # Real sources which don't need to be mocked
  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet/core/client_entry.c
)
set(RDMNET_MOCK_API_HEADERS
  ${RDMNET_INCLUDE}/rdmnet_mock/controller.h
  ${RDMNET_INCLUDE}/rdmnet_mock/common.h
)
set(RDMNET_MOCK_API_SOURCES
  ${RDMNET_SRC}/rdmnet_mock/controller.c
  ${RDMNET_SRC}/rdmnet_mock/common.c
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
