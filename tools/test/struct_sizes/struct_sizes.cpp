// struct_sizes, a tool which prints the byte size of various structs used in the RDMnet library
// This is mostly for optimizing around stack usage in embedded applications.

#include <iostream>

#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/client_entry.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/discovery.h"
#include "rdmnet/core/ept_prot.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/rpt_prot.h"

#include "rdmnet/core/client.h"
#include "rdmnet/private/client.h"

#define SIZE_COLUMN_TAB_OFFSET 4

#define PRINT_SIZE(type)                                                                                            \
  do                                                                                                                \
  {                                                                                                                 \
    std::cout << #type;                                                                                             \
    static_assert(SIZE_COLUMN_TAB_OFFSET > ((sizeof(#type) - 1) / 8), "SIZE_COLUMN_TAB_OFFSET must be increased."); \
    size_t num_tabs = SIZE_COLUMN_TAB_OFFSET - ((sizeof(#type) - 1) / 8);                                           \
    for (size_t i = 0; i < num_tabs; ++i)                                                                           \
      std::cout << "\t";                                                                                            \
    std::cout << sizeof(type) << std::endl;                                                                         \
  } while (0)

#define PRINT_HEADER_NAME(name) std::cout << std::endl << "=== " #name " ===" << std::endl

void PrintAllSizes()
{
  std::cout << "Typename";
  for (int i = 0; i < SIZE_COLUMN_TAB_OFFSET - 1; ++i)
    std::cout << "\t";
  std::cout << "Size" << std::endl;

  PRINT_HEADER_NAME("rdmnet/core/broker_prot.h");
  PRINT_SIZE(BrokerClientConnectMsg);
  PRINT_SIZE(BrokerConnectReplyMsg);
  PRINT_SIZE(BrokerClientEntryUpdateMsg);
  PRINT_SIZE(BrokerClientRedirectMsg);
  PRINT_SIZE(ClientList);
  PRINT_SIZE(BrokerDynamicUidRequestList);
  PRINT_SIZE(BrokerDynamicUidMapping);
  PRINT_SIZE(BrokerDynamicUidAssignmentList);
  PRINT_SIZE(BrokerFetchUidAssignmentList);
  PRINT_SIZE(BrokerDisconnectMsg);
  PRINT_SIZE(BrokerMessage);

  PRINT_HEADER_NAME("rdmnet/core/client_entry.h");
  PRINT_SIZE(EptSubProtocol);
  PRINT_SIZE(RptClientEntry);
  PRINT_SIZE(EptClientEntry);
  PRINT_SIZE(ClientEntry);

  PRINT_HEADER_NAME("rdmnet/core/connection.h");
  PRINT_SIZE(RdmnetConnCallbacks);
  PRINT_SIZE(RdmnetConnectionConfig);

  PRINT_HEADER_NAME("rdmnet/discovery.h");

  PRINT_HEADER_NAME("rdmnet/core/ept_prot.h");
  PRINT_SIZE(EptStatusMsg);
  PRINT_SIZE(EptDataMsg);
  PRINT_SIZE(EptMessage);

  PRINT_HEADER_NAME("rdmnet/core/message.h");
  PRINT_SIZE(RdmnetLocalRdmCommand);
  PRINT_SIZE(RdmnetRemoteRdmCommand);
  PRINT_SIZE(RdmnetLocalRdmResponse);
  PRINT_SIZE(RdmnetRemoteRdmResponse);
  PRINT_SIZE(RptClientMessage);
  PRINT_SIZE(EptClientMessage);
  PRINT_SIZE(RdmnetMessage);

  PRINT_HEADER_NAME("rdmnet/core/rpt_prot.h");
  PRINT_SIZE(RptHeader);
  PRINT_SIZE(RptStatusMsg);
  PRINT_SIZE(RptRdmBufList);
  PRINT_SIZE(RptMessage);

  PRINT_HEADER_NAME("rdmnet/core/client.h");
  PRINT_SIZE(RdmnetRptClientConfig);
  PRINT_SIZE(ClientCallbackDispatchInfo);
}

int main(int /*argc*/, char* /*argv*/[])
{
  PrintAllSizes();
  return 0;
}
