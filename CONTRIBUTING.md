# Contributing to RDMnet

Thank you for your interest in contributing to the RDMnet project!

## Pull requests

Thanks for your interest in contributing code to the RDMnet library!

### Building and Debugging the Library

Check out the relevant [docs page](https://etclabs.github.io/RDMnetDocs/head/building_and_integrating.html)
for how to build RDMnet, especially the section titled "Building RDMnet on its own".

When configuring with CMake, you will want to define `RDMNET_BUILD_TESTS=ON` so that you can check
that the unit tests pass as you modify the RDMnet library. This adds a lot of unit test executable
targets to the build; you can run them all at once using CTest by just typing `ctest` in the build
directory (or `ctest -C [configuration]` for a multi-config CMake generator).

The RDMnet example applications can help provide immediate debugging feedback for changes made to
the library. Enable them by configuring with `RDMNET_BUILD_CONSOLE_EXAMPLES=ON` and
`RDMNET_BUILD_GUI_EXAMPLES=ON` (the latter requires Qt; see the doc page linked above for more
information).

### Before Opening a Pull Request

* Make sure the unit tests pass
* Add unit tests if applicable for any regressions or new features you have added to the core library
  * (This is not necessary if just modifying the example apps as they do not have test coverage)
* Format the code you've touched using clang-format (CMake creates a convenient target `reformat_all`
  which runs clang-format on all of the RDMnet sources if it is available on your PATH)

## Reporting issues

### Check to make sure your issue isn't already known

If you have identified a reproducible bug or missing feature, please do the following before
opening an issue:

* Make sure the bug or feature is not covered as a known issue in the README or documentation.
* Make sure the bug or feature is not covered in an existing open issue.

### Write a comprehensive bug report

A good bug report includes the following:

* Which app(s) or library code you were using and their versions
* A set of steps to reproduce the issue, in order
* What you expected to see, and what happened instead
* If the bug has occured in code you wrote that uses the RDMnet library, please provide code
  snippets and try to reduce to a minimal reproducible example.
* Any logging output that was produced when the issue occurred
