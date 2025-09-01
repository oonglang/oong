cmake_minimum_required(VERSION 3.13)
project(oong LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
  core
  support
)

llvm_map_components_to_libnames(LLVM_EXTRA_LIBS
  target
  mc
  asmparser
  asmprinter
  object
  X86
  orcjit
  executionengine
)

# Combine and filter out any diaguids.lib entries that may be hardcoded in
# the LLVM lib lists (some LLVM packages include paths to diaguids from the
# machine where LLVM was built). We'll add the correct DIA lib explicitly later.
set(ALL_LLVM_LIBS ${LLVM_LIBS} ${LLVM_EXTRA_LIBS})
set(FILTERED_LLVM_LIBS)
foreach(LIBPATH IN LISTS ALL_LLVM_LIBS)
  string(FIND "${LIBPATH}" "diaguids.lib" DIASUB)
  if(DIASUB EQUAL -1)
    list(APPEND FILTERED_LLVM_LIBS "${LIBPATH}")
  else()
    message(STATUS "Removing embedded diaguids reference from LLVM list: ${LIBPATH}")
  endif()
endforeach()
message(STATUS "ALL_LLVM_LIBS = ${ALL_LLVM_LIBS}")
message(STATUS "FILTERED_LLVM_LIBS = ${FILTERED_LLVM_LIBS}")

add_executable(oong
  src/main.cpp
  src/compiler.cpp
  src/interpreter.cpp
  src/lexer.cpp
  src/token.cpp
  src/parser.cpp
  src/ast.cpp
)
set_target_properties(oong PROPERTIES OUTPUT_NAME "oong")
target_include_directories(oong PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(oong PRIVATE ${FILTERED_LLVM_LIBS})

