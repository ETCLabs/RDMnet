# Using the Device API                                          {#using_device}

The RDMnet Device API exposes both a C and C++ language interface. The C++
interface is a header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The device init and deinit functions should be called once each at application startup and shutdown
time. These functions interface with the EtcPal \ref etcpal_log API to configure what happens when
the library logs messages. Optionally pass an EtcPalLogParams structure to use this functionality.
This structure can be shared across different ETC library modules.

The init function has the side-effect of initializing the core RDMnet library, which by default
starts a background thread for handling periodic RDMnet functionality and receiving data (this
behavior can be overridden at compile-time if an app wants more control over its threading, see
#RDMNET_USE_TICK_THREAD and rdmnet_core_tick()). The deinit function joins this thread.

<!-- CODE_BLOCK_START -->
```c
#include "rdmnet/device.h"

// In some function called at startup...
EtcPalLogParams log_params;
// Initialize log_params...

etcpal_error_t init_result = rdmnet_device_init(&log_params);
// Or, to init without worrying about logs from the RDMnet library...
etcpal_error_t init_result = rdmnet_device_init(NULL);

// In some function called at shutdown...
rdmnet_device_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "rdmnet/cpp/device.h"

// In some function called at startup...
etcpal::Logger logger;
// Initialize and start logger...

etcpal::Result init_result = rdmnet::Device::Init(logger);

// Or, to init without worrying about logs from the RDMnet library...
etcpal::Result init_result = rdmnet::Device::Init();

// In some function called at shutdown...
rdmnet::Device::Deinit();
```
<!-- CODE_BLOCK_END -->
