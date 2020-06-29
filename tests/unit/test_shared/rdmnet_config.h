#ifdef __cplusplus
extern "C" {
#endif
void RdmnetTestingAssertHandler(const char* expression, const char* file, unsigned int line)
#ifdef __clang__
    __attribute__((analyzer_noreturn))
#endif
    ;
#ifdef __cplusplus
}
#endif

#define RDMNET_ASSERT(expr) \
  if (!(expr))              \
  RdmnetTestingAssertHandler(#expr, __FILE__, __LINE__)
