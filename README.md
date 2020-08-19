# RDMnet

RDMnet is a library that implements ANSI E1.33, a standard by ESTA for
entertainment technology. E1.33 extends the functionality of RDM (Remote Device
Management, ANSI E1.20) onto IP networks. The RDMnet library is maintained by
[ETC](http://www.etcconnect.com).

|        | Binary Package | Docs |
|------- |----------------|------|
| Stable | [ ![Download](https://api.bintray.com/packages/etclabs/rdmnet_bin/stable/images/download.svg) ](https://bintray.com/etclabs/rdmnet_bin/stable/_latestVersion) | <a href="docs/index.html">Link</a> |
| Latest | [ ![Download](https://api.bintray.com/packages/etclabs/rdmnet_bin/latest/images/download.svg) ](https://bintray.com/etclabs/rdmnet_bin/latest/_latestVersion) | <a href="docs/head/">Link</a> |
 
## Binary Package

The RDMnet binary package is currently ported for Microsoft Windows and macOS.
It includes the following components:
* *rdmnet_controller_example*: A Qt-based GUI application which does basic
  discovery, display and configuration of RDMnet Components.
* *rdmnet_broker_example*: A console application which implements an RDMnet
  Broker.
* *rdmnet_device_example*: A console application which implements an RDMnet
  Device.
* *rdmnet_gateway_example*: (from version 0.3.0, Windows only) A console
  application which uses connected ETC
  [Gadget II](https://www.etcconnect.com/Products/Networking/Gadget-II/Features.aspx)
  devices to act as an RDMnet Gateway.
* *llrp_manager_example*: A console application which implements a basic LLRP
  Manager.

### Run the binaries

The console applications can be invoked from the command line with no arguments
to run with default settings. Use `--help` to check what usage options are
available. For example:
```
rdmnet_broker_example --help
```

## Note on Qt

The RDMnet Controller example application is distributed with binaries for Qt
5.9.7. This bundling is permitted under the terms of the GNU Lesser General
Public License (LGPL) v.3. For more information on this, see the
[disclaimer](https://github.com/ETCLabs/RDMnet/blob/master/ThirdPartySoftware.txt)
and the <a href="docs/index.html">documentation</a>.
