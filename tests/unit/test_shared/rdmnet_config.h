void RdmnetTestingAssertHandler(const char* expression, const char* file, unsigned int line);

#define RDMNET_ASSERT(expr) (void)((!!(expr)) || (RdmnetTestingAssertHandler(#expr, __FILE__, __LINE__), 0))
