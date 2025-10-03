message(STATUS "Adding libGadget for Fakeway example...")

set(LIBGADGET_FETCHCONTENT_VERSION 3.0.0.1)

include(FetchContent)
FetchContent_Declare(
  libGadget
  GIT_REPOSITORY https://github.com/ETCLabs/libGadget
  GIT_TAG v${LIBGADGET_FETCHCONTENT_VERSION}
)
FetchContent_MakeAvailable(libGadget)
