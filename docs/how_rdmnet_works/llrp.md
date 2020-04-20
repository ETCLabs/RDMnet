# Low-Level Recovery Protocol (LLRP)                                                        {#llrp}

RDMnet includes a protocol called the Low-Level Recovery Protocol (LLRP) which operates somewhat
independently of the rest of its protocols. LLRP provides a standard way to discover and
communicate with RDMnet-capable devices even if they don't currently have valid IP settings. The
main purpose of LLRP is to correct misconfigured devices such that the other protocols of RDMnet
can work properly.

All messaging in LLRP is sent via UDP multicast (this is what enables it to work even in the
presence of misconfigured IP settings). There are two roles in LLRP: an _LLRP Manager_ initiates
discovery and sends RDM commands to _LLRP Targets_, which respond to those commands.

To help increase the number of situations in which LLRP is useful, the RDMnet standard requires all
RDMnet devices and brokers to also operate as LLRP Targets. This RDMnet library handles that
functionality automatically when using the Device or Broker APIs.

**IMPORTANT NOTE:** LLRP is not inherently scalable and is meant solely as a "bootstrapping"
protocol which contains just enough functionality to get RDMnet communication working again.
Because of this, the RDMnet standard limits the set of RDM commands allowed for sending via LLRP.
See "RDM PIDs Supported" below for more information.

## Discovery

Discovery in LLRP is initiated by a manager sending a _Probe Request_ to a well-known multicast
address. Any targets which receive the probe request and meet certain criteria respond with a
_Probe Reply_.

The discovery process also contains some response-suppression mechanisms intended to assist
discovery where more than a few hundred LLRP targets are present. This is an implementation detail
of the protocol and is not discussed here.

```
LLRP Manager                                 LLRP Target
     ||                                          ||
     ||  Probe Request                           ||
     ||-----------------> 239.255.250.133        ||
     ||                                          ||
     ||                             Probe Reply  ||
     ||          239.255.250.134 <---------------||
     ||                                          ||
```

## Target Addressing

Like in other RDMnet protocols, LLRP managers and targets have both a Component Identifier (CID)
and an RDM UID which can be either static or dynamic. See \ref roles_and_addressing for more
information on these identifiers.

Because dynamic RDM UIDs are not guaranteed to be globally unique, LLRP managers must always use a
combination of CID and UID to track LLRP targets. The CID uniquely and globally identifies a
target, and the UID provides a destination address for the RDM command structures that are used to
configure targets. 

## RDM Configuration

After performing discovery, an LLRP Manager caches its list of LLRP Targets and may send RDM
commands to them to retrieve or change their current configuration. Common RDM PIDs sent over LLRP
include the IP addressing PIDs defined in ANSI E1.37-2, and the `COMPONENT_SCOPE` PID defined in
the RDMnet standard.

```
LLRP Manager                                 LLRP Target
     ||                                          ||
     ||  RDM Command                             ||
     ||---------------> 239.255.250.133          ||
     ||                                          ||
     ||                            RDM Response  ||
     ||         239.255.250.134 <----------------||
     ||                                          ||
``` 

Since LLRP RDM commands are sent via multicast UDP, delivery is done on a best-effort basis and the
LLRP Manager considers the response to an RDM command to be lost if it is not received within 2
seconds.

### RDM PIDs supported

The whitelist of RDM PIDs allowed over LLRP in the current revision of the standard is below. The
standards specified can all be downloaded free of charge from the
[ESTA TSP website](https://tsp.esta.org/tsp/documents/published_docs.php).

* ANSI E1.33:
  + `COMPONENT_SCOPE`
  + `SEARCH_DOMAIN` (RDMnet Devices only)
  + `TCP_COMMS_STATUS` (RDMnet Devices only)
  + `BROKER_STATUS` (RDMnet Brokers only)
* ANSI E1.20:
  + `SUPPORTED_PARAMETERS`
  + `DEVICE_INFO`
  + `RESET_DEVICE`
  + `FACTORY_DEFAULTS`
  + `DEVICE_LABEL`
  + `MANUFACTURER_LABEL`
  + `DEVICE_MODEL_DESCRIPTION`
  + `IDENTIFY_DEVICE`
* ANSI E1.37-1:
  + `LOCK_STATE_DESCRIPTION`
  + `LOCK_STATE`
* ANSI E1.37-2:
  + `LIST_INTERFACES`
  + `INTERFACE_LABEL`
  + `INTERFACE_HARDWARE_ADDRESS_TYPE1`
  + `IPV4_DHCP_MODE`
  + `IPV4_ZEROCONF_MODE`
  + `IPV4_CURRENT_ADDRESS`
  + `IPV4_STATIC_ADDRESS`
  + `INTERFACE_RENEW_DHCP`
  + `INTERFACE_RELEASE_DHCP`
  + `INTERFACE_APPLY_CONFIGURATION`
  + `IPV4_DEFAULT_ROUTE`
  + `DNS_IPV4_NAME_SERVER`
  + `DNS_HOSTNAME`
  + `DNS_DOMAIN_NAME`
 