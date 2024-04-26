# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/serh/_work/esp/esp-idf-v4.4.7/esp-idf/components/bootloader/subproject"
  "/home/serh/_work/workspace/Middleware/build/bootloader"
  "/home/serh/_work/workspace/Middleware/build/bootloader-prefix"
  "/home/serh/_work/workspace/Middleware/build/bootloader-prefix/tmp"
  "/home/serh/_work/workspace/Middleware/build/bootloader-prefix/src/bootloader-stamp"
  "/home/serh/_work/workspace/Middleware/build/bootloader-prefix/src"
  "/home/serh/_work/workspace/Middleware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/serh/_work/workspace/Middleware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
