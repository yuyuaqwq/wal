#The MIT License(MIT)
#Copyright © 2025 https://github.com/yuyuaqwq
#
#Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
#
#The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


# src/CMakeLists.txt

set(SOURCES
    crc32.cpp
    log_writer.cpp
    log_reader.cpp
)

add_library(wal ${SOURCES})

set(WAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include CACHE STRING "wal header dir")
set(WAL_LIB wal CACHE STRING "wal lib")

target_include_directories(wal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(wal PRIVATE ${WAL_HEADER_DIR})
target_include_directories(wal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)

