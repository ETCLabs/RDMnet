# Discovery                                                                            {#discovery}

RDMnet includes an automatic discovery mechanism which makes use of
[Multicast DNS](https://en.wikipedia.org/wiki/Multicast_DNS) (mDNS) combined with
[DNS Service Discovery](http://www.dns-sd.org/) (DNS-SD). This mechanism is used exclusively by
RDMnet controllers and devices to discover brokers. Brokers advertise themselves via DNS-SD, and
controllers and devices query for brokers that match their configured scope(s).

mDNS combined with DNS-SD is sometimes also known as
["zeroconf"](https://en.wikipedia.org/wiki/Zero-configuration_networking) or 
["Bonjour"](https://en.wikipedia.org/wiki/Bonjour_(software)).

## DNS-SD

Like traditional [DNS](https://en.wikipedia.org/wiki/Domain_Name_System), which translates a domain
name to a network address, DNS-SD also translates names to network addresses; however, there is a
subtle difference. Where DNS queries typically translate a fully-qualified domain name to a single
IP address, DNS-SD queries translate a _service name_ to one or more _service instances_ which
provide the given service. 

### A lighthearted analogy

Let's say you've got a craving for pizza. If you have a specific pizza restaurant in mind, you can
search for its name to find its address. For example, you might want to search Google Maps for the
excellent [Glass Nickel Pizza Company](https://www.google.com/maps/search/Glass+Nickel+Pizza/@43.055168,-89.4258971,13z)
in Madison, WI, which would give you the address of that specific restaurant. This is analogous to
traditional DNS.

If you weren't as picky and wanted to find out what other pizza restaurants were out there, you
might just search for ["pizza"](https://www.google.com/maps/search/pizza/@43.0679178,-89.4164854,13z).
This would turn up a bunch of different restaurants, any of which you could then get the address
from after deciding which one you wanted. This is analogous to DNS-SD; pizza is the _service name_,
and the pizza restaurants are all _service instances_ which provide the "pizza" service.

### How it works in RDMnet

For RDMnet's purposes, clients searching for brokers query on the `rdmnet` service, and the service
instances returned are brokers:

```
_rdmnet._tcp.local. -> Broker1._rdmnet._tcp.local.
                       Broker2._rdmnet._tcp.local.
```

The second step of DNS-SD resolves one or more of those service instances to an IP address and port
at which the broker can be reached:

```
Broker1._rdmnet._tcp.local. -> 10.101.50.60:12345
```

In practice, DNS-SD queries are often filtered to only return brokers for a given scope, which is
implemented as a _service sub-type_. Assuming only `Broker2` above is operating on the `default`
RDMnet scope, a filtered query would look like this:

```
_default._sub._rdmnet._tcp.local. -> Broker2._rdmnet._tcp.local.
```

## mDNS

You may have noticed that all of the domain names used in the queries above ended in ".local". This
is a special top-level domain which indicates that DNS queries for that name are done using
multicast DNS (or _mDNS_). mDNS is a serverless alternative to the traditional Domain Name Service
which allows queries to be resolved directly between a client and server without the need for an
intermediary DNS server. In mDNS, all queries and responses are sent to a single, well-known
multicast address and port.

All RDMnet brokers are required to respond to queries via mDNS, making it the _de-facto_ default
way to discover brokers in RDMnet. It is possible (and supported by the library) to also register
and discover brokers via traditional unicast DNS by setting up records in a DNS server, but this
obviously requires more user configuration.
