#ifdef __cplusplus
extern "C" {
#endif

bool RdmnetTestingAssertHandler(const char* expression, const char* file, const char* func, unsigned int line);

#ifdef __cplusplus
}
#endif

#define RDMNET_ASSERT_VERIFY(expr) ((expr) ? true : RdmnetTestingAssertHandler(#expr, __FILE__, __func__, __LINE__))

#define RDMNET_DYNAMIC_MEM 1
