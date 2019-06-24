################################ Mock Library #################################

# RDMnet mock libraries, for unit testing

# Mock discovery library
add_library(RDMnetDiscovery_mock INTERFACE)
target_sources(RDMnetDiscovery_mock INTERFACE
  ${RDMNET_INCLUDE}/rdmnet_mock/core/discovery.h
  ${RDMNET_SRC}/rdmnet_mock/discovery/mock_discovery.c
)
target_include_directories(RDMnetDiscovery_mock INTERFACE ${RDMNET_INCLUDE})
target_link_libraries(RDMnetDiscovery_mock INTERFACE meekrosoft::fff)
source_group(src\\discovery FILES ${RDMNET_SRC}/rdmnet/discovery/mock.c)

add_library(RDMnetCore_mock
  # Mock sources
  ${RDMNET_INCLUDE}/rdmnet_mock/core.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/broker_prot.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/connection.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/llrp_target.h
  ${RDMNET_INCLUDE}/rdmnet_mock/core/rpt_prot.h

  ${RDMNET_SRC}/rdmnet_mock/core.c
  ${RDMNET_SRC}/rdmnet_mock/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet_mock/core/connection.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet_mock/core/rpt_prot.c

  # Real sources
  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet/core/client_entry.c
)
target_compile_definitions(RDMnetCore_mock PUBLIC RDMNET_BUILDING_FULL_MOCK_CORE_LIB)
target_include_directories(RDMnetCore_mock PUBLIC ${RDMNET_INCLUDE} PRIVATE ${RDMNET_SRC})
target_link_libraries(RDMnetCore_mock PUBLIC meekrosoft::fff RDMnetDiscovery_mock PRIVATE lwpa RDM)
