# Test input data generator

This directory contains a C++ library and files for getting input data which represents RDMnet
protocol messages. The `messages` directory contains pairs of files with each pair representing a
complete valid RDMnet message, named according to the following convention:

* The file ending in `.data.txt` contains the raw bytes of the protocol message.
* The file ending in `.validation.c` is a C source file which defines a global data structure
  representing the correctly-parsed message. This is used for validating the parser in unit tests.

The format for the `.data.txt` file is as follows:

* Represent each byte of the message as a whitespace-delimited hexadecimal number, with no prefix.
* Double-slash (`//`) comments are allowed at the end of lines or on lines by themselves. Other
  types of comments or text are not allowed.
* Empty lines are allowed.

The files are scraped by CMake based on the above naming convention and added to a manifest which
is usable from a C++ program, which is defined in test_file_manifest.h. The manifest is a vector of
pairs containing the file name and a ref to the valid C structure.

`load_test_data.h` defines a function `rdmnet::testing::LoadTestData()` which can convert the
format of the `.data.txt` files to a byte array.

Consume this library from CMake using the target test_data.
