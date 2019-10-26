# Changelog
All notable changes to the RDMnet library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Platform port and binary package: macOS

### Changed
- Update to comply with final published ANSI E1.33-2019
- Minor changes to API functions for style

## [0.2.0] - 2019-06-20
### Added
- This CHANGELOG file.
- Windows installers for example applications.

### Changed
- RDMnet is now built with CMake. Documentation updated accordingly.
- RDMnet example applications are now portable.
- RDMnet compile-time config options no longer affect the contents of public headers
  (correspondingly, rdmnet/opts.h moved from under include/ to src/)
- rdmnet/broker_prot.h: Renamed is_disconnect() to is_disconnect_msg() for consistency

### Removed
- rdm* source files, which are now in the [RDM](https://github.com/ETCLabs/RDM) repository.
- Visual Studio project files, as we now build with CMake.

## [0.1.0] - 2018-10-18
### Added
- Initial library modules and partial documentation.
- Implementation of draft standard BSR E1.33 rev 63.
- Initial Broker, Controller, Device and LLRP Manager example applications.

[Unreleased]: https://github.com/ETCLabs/RDMnet/compare/master...develop
[0.2.0]: https://github.com/ETCLabs/RDMnet/compare/v0.1.0.4...v0.2.0
[0.1.0]: https://github.com/ETCLabs/RDMnet/releases/tag/v0.1.0.4
