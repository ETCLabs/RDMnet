# Using the Broker API                                                              {#using_broker}

The RDMnet broker API exposes a C++11 interface for creating instances of RDMnet broker
functionality.

**NOTE:** Typically, RDMnet broker functionality should have its own dedicated service, and not be
included directly in applications. This API is useful for building an RDMnet broker service, or for
applications where background services are not available, such as mobile platforms.

## Initialization

The RDMnet library must be globally initialized before using the RDMnet broker API. See
\ref global_init_and_destroy.

To create a broker instance, instantiate an rdmnet::Broker and call its Startup() function. A
broker can operate on a single RDMnet scope at a time; the initial scope, and other configuration
parameters the broker uses at runtime, are provided via the rdmnet::BrokerSettings struct.

The RDMnet broker API is an asynchronous, callback-oriented API. Part of the initial configuration
for a broker instance is an abstract interface for the library to use as callbacks. Callbacks are
dispatched from a background thread.

```cpp
#include "rdmnet/broker.h"

// Each broker is a component that must have a Component ID (CID), which is simply a UUID.
// Software should generate and save a CID so that the same one is used on each run of the software.
auto my_cid = etcpal::Uuid::OsPreferred();

// Contains the configuration settings that the broker needs to operate. Some of these are set to
// default values and can be changed if necessary. Must pass your ESTA manufacturer ID. If you have
// not yet requested an ESTA manufacturer ID, the range 0x7ff0 to 0x7fff can be used for
// prototyping.
rdmnet::BrokerSettings settings(my_cid, MY_ESTA_MANUFACTURER_ID_VAL);

rdmnet::Broker broker;
auto result = broker.Startup(settings);
if (result)
{
  // Broker is now running
}
else
{
  std::cout << "Error starting broker: " << result.ToString() << '\n';
}
```

Once running, the broker will spawn a number of worker threads and operate independently with no
further action needed. The full breakdown of threads used is described in the rdmnet::Broker class
documentation.

## Deinitialization

The broker should be shut down gracefully before program termination using the Shutdown() function.
This function will send graceful disconnect messages to all connected clients, close all network
sockets and connections, deallocate resources and join all threads.

```cpp
broker.Shutdown();

// At this point, the broker instance is in a non-running state. It can be started again with the
// Startup() function.
```

## Changing Settings at Runtime

### Scope

The broker's scope can be changed at runtime using the ChangeScope() function.

```cpp
auto result = broker.ChangeScope("new scope string", kRdmnetDisconnectUserReconfigure);
if (result)
{
  // Broker is now using the new scope.
}
else
{
  std::cout << "Error changing broker's scope: " << result.ToString() << '\n';
}
```

When the scope changes, the broker will disconnect all currently connected clients using the
disconnect reason code given in the disconnect_reason parameter. It will then restart broker
services and re-register for discovery using the new scope.

## Logging

Brokers can log messages using the etcpal::Logger class. To capture logs from the broker, set up an
etcpal::Logger instance per its documentation, and pass it to the broker instance on startup:

```cpp
etcpal::Logger logger;
logger.SetLogMask(ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO)).Startup(my_log_message_handler);

broker.Startup(settings, &logger);
```

The broker logs messages at appropriate syslog-style severity levels; for example, if you set the
logger's log mask to ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG), the broker will log _lots_ of messages
(including a message logged for every message the broker routes). If you set it to
ETCPAL_LOG_UPTO(ETCPAL_LOG_ERR), the logging will be much more sparse and only represent error
conditions.

## Broker Notifications

The broker can pass notifications of certain events back to the application for handling.
Currently, these notifications are all informational; broker instances operate the same way
regardless of how they are handled.

```cpp
class MyBrokerNotifyHandler : public rdmnet::BrokerNotifyHandler
{
  // Implement the BrokerNotifyHandler functions...
}

MyBrokerNotifyHandler notify_handler;
broker.Startup(settings, &logger, &notify_handler);
```

### Scope Changed

If BrokerSettings::allow_rdm_scope_change was set to true at the initialization of a broker
instance, the broker will accept RDM commands which change its scope. When this happens, it will
also deliver a scope changed notification to the notification handler.

```cpp
void MyBrokerNotifyHandler::HandleScopeChanged(const std::string& new_scope)
{
  std::cout << "Broker's scope changed to '" << new_scope << "'\n";
}
```
