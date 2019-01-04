echo off
cmake -DNEW_VERSION_NUMBER=%1 -P create_build.cmake