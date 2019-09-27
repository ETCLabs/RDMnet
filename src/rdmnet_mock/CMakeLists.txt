################################ Mock Library #################################

# RDMnet mock libraries, for unit testing

# Mock discovery library
add_library(RDMnetDiscoveryMock INTERFACE)
target_sources(RDMnetDiscoveryMock INTERFACE
  ${RDMNET_INCLUDE}/rdmnet_mock/core/discovery.h
  ${RDMNET_SRC}/rdmnet_mock/discovery/mock_discovery.c
)
target_include_directories(RDMnetDiscoveryMock INTERFACE ${RDMNET_INCLUDE})
target_link_libraries(RDMnetDiscoveryMock INTERFACE meekrosoft::fff)

set(RDMNET_CORE_MOCK_HEADERS
  ${RDMNET_INCLUDE}/rdmnet_mock/core.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/broker_prot.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/connection.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/llrp_target.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/rpt_prot.h
)
set(RDMNET_CORE_MOCK_SOURCES
  ${RDMNET_SRC}/rdmnet_mock/core.c
  ${RDMNET_SRC}/rdmnet_mock/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet_mock/core/connection.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet_mock/core/rpt_prot.c
  # Real sources which don't need to be mocked
  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet/core/client_entry.c
)
add_library(RDMnetCoreMock INTERFACE)
target_sources(RDMnetCoreMock INTERFACE
  ${RDMNET_CORE_MOCK_HEADERS}
  ${RDMNET_CORE_MOCK_SOURCES}
)
target_compile_definitions(RDMnetCoreMock INTERFACE RDMNET_BUILDING_FULL_MOCK_CORE_LIB)
target_include_directories(RDMnetCoreMock INTERFACE ${RDMNET_INCLUDE} ${RDMNET_SRC})
target_link_libraries(RDMnetCoreMock INTERFACE meekrosoft::fff RDMnetDiscoveryMock EtcPal RDM)
