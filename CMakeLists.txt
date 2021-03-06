cmake_minimum_required(VERSION 3.3)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_ROOT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(CMAKE_SKIP_RPATH TRUE)

option(USE_COTIRE ON)

include(cotire)

include(GetGitRevisionDescription)
git_describe(VERSION --always --tags --dirty=-modified --long --abbrev=40)

add_library(build_version version/src/version.c)
target_include_directories(build_version PUBLIC version/include)
target_compile_definitions(build_version PRIVATE BUILD_VERSION=\"${VERSION}\")

set(CMAKE_CXX_FLAGS_RELEASE "-Os -s -fvisibility=hidden")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -Wno-odr -g -rdynamic")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d-%H%M%S" UTC)

add_subdirectory(utils EXCLUDE_FROM_ALL)
add_subdirectory(logger EXCLUDE_FROM_ALL)
add_subdirectory(interface)
add_subdirectory(api)

file(GLOB DEPS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/deps/*)
file(GLOB EXTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/exts/*)

foreach(dep ${DEPS})
  message("Found DEP: " ${dep})
  add_subdirectory(${dep} EXCLUDE_FROM_ALL)
endforeach()
foreach(ext ${EXTS})
  message("Found EXT: " ${ext})
  add_subdirectory(${ext} EXCLUDE_FROM_ALL)
endforeach()

add_subdirectory(stone)