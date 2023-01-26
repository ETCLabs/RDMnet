// Testing assert handler

#ifdef __cplusplus
extern "C" {
#endif

bool RdmnetTestingAssertHandler(const char* expression, const char* file, const char* func, unsigned int line);

#ifdef __cplusplus
}
#endif

#define RDMNET_ASSERT_VERIFY(expr) ((expr) ? true : RdmnetTestingAssertHandler(#expr, __FILE__, __func__, __LINE__))

// Static mem
#define RDMNET_DYNAMIC_MEM 0

// Some carefully considered, well-thought-out maximums
#define RDMNET_MAX_CONTROLLERS 5
#define RDMNET_MAX_DEVICES 5
#define RDMNET_MAX_EPT_CLIENTS 5
#define RDMNET_MAX_SCOPES_PER_CONTROLLER 5
#define RDMNET_MAX_ENDPOINTS_PER_DEVICE 5
#define RDMNET_MAX_RESPONDERS_PER_DEVICE 25
#define RDMNET_MAX_PROTOCOLS_PER_EPT_CLIENT 5
#define RDMNET_MAX_SENT_OVERFLOW_RESPONSES 5
#define RDMNET_PARSER_MAX_CLIENT_ENTRIES 5
#define RDMNET_PARSER_MAX_EPT_SUBPROTS 5
#define RDMNET_PARSER_MAX_DYNAMIC_UID_ENTRIES 5
#define RDMNET_PARSER_MAX_ACK_OVERFLOW_RESPONSES 5
#define RDMNET_MAX_MCAST_NETINTS 5
#define RDMNET_MAX_MONITORED_SCOPES 5
#define RDMNET_MAX_DISCOVERED_BROKERS_PER_SCOPE 5
#define RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER 5
#define RDMNET_MAX_ADDITIONAL_TXT_ITEMS_PER_DISCOVERED_BROKER 5
