#ifdef __cplusplus
extern "C" {
#endif
void RdmnetTestingAssertHandler(const char* expression, const char* file, unsigned int line);
#ifdef __cplusplus
}
#endif

#define RDMNET_ASSERT(expr) (void)((!!(expr)) || (RdmnetTestingAssertHandler(#expr, __FILE__, __LINE__), 0))
