# Global Initialization and Destruction                                  {#global_init_and_destroy}

The core RDMnet init and deinit functions should be called once each at application startup and
shutdown time. These functions interface with the EtcPal \ref etcpal_log API to configure what
happens when the library logs messages. Optionally pass an EtcPalLogParams structure to use this 
functionality. This structure can be shared across different ETC library modules.

The init function by default starts a background thread for handling periodic RDMnet functionality
and receiving data. The deinit function joins this thread.

<!-- CODE_BLOCK_START -->
```c
// In some function called at startup...
EtcPalLogParams log_params;
// Initialize log_params...

etcpal_error_t init_result = rdmnet_init(&log_params, NULL);
// Or, to init without worrying about logs from the RDMnet library...
etcpal_error_t init_result = rdmnet_init(NULL, NULL);

// In some function called at shutdown...
rdmnet_core_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
// In some function called at startup...
etcpal::Logger logger;
// Initialize and start logger...

etcpal::Error init_result = rdmnet::Init(logger);

// Or, to init without worrying about logs from the RDMnet library...
etcpal::Error init_result = rdmnet::Init();

// In some function called at shutdown...
rdmnet::Deinit();
```
<!-- CODE_BLOCK_END -->
