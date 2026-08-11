# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-src")
  file(MAKE_DIRECTORY "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-src")
endif()
file(MAKE_DIRECTORY
  "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-build"
  "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-subbuild/miniaudio-populate-prefix"
  "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-subbuild/miniaudio-populate-prefix/tmp"
  "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-subbuild/miniaudio-populate-prefix/src/miniaudio-populate-stamp"
  "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-subbuild/miniaudio-populate-prefix/src"
  "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-subbuild/miniaudio-populate-prefix/src/miniaudio-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-subbuild/miniaudio-populate-prefix/src/miniaudio-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/dev.daksh/code/pulsar/build/_deps/miniaudio-subbuild/miniaudio-populate-prefix/src/miniaudio-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
