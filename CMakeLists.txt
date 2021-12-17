cmake_minimum_required(VERSION 3.10)

project(VMI VERSION 1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG "Enable debug check and output" OFF)

set (CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})
if (DEBUG)
  message("Debug check and output: ON")
else()
  message("Debug check and output: OFF")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  if (NOT DEBUG)
    message(
      WARNING
      "Debug build is selected but debug check and output is not enabled.\n"
      "Hint: enable debug check and output explicitly via:\n"
      "  > cmake -DDEBUG=ON <other options> .."
    )
  endif()
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O1")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

configure_file(config.h.in config.h @ONLY)

find_library(LIBVMI vmi)
# find_library(LIBCAPSTONE capstone)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${PROJECT_BINARY_DIR}"
)

# add_executable(mem-event mem-event.cc)
# target_link_libraries(mem-event PRIVATE ${LIBVMI})

add_executable(proc-list proc-list.cc)
target_link_libraries(proc-list PRIVATE ${LIBVMI})

# add_executable(breakpoint breakpoint.cc)
# target_link_libraries(
#   breakpoint
#   PRIVATE ${LIBVMI}
#   PRIVATE ${LIBCAPSTONE}
# )
