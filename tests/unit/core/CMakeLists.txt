# RDMnet core library unit tests

# Because the RDMnet core library is written in C, several test executables are
# necessary in order to selectively mock/stub dependencies. This allows
# isolated unit tests to be written.

add_subdirectory(support_modules)
add_subdirectory(connection)
add_subdirectory(llrp)
