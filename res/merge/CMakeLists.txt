cmake_minimum_required(VERSION 3.5)
project(wordle_merge LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY lib)

set(PROGRAM_SOURCES 
    ../../src/nbt/endian_str.cpp
    ../../src/nbt/tag.cpp
    ../../src/nbt/tag_compound.cpp
    ../../src/nbt/tag_list.cpp
    ../../src/nbt/tag_string.cpp
    ../../src/nbt/value.cpp
    ../../src/nbt/value_initializer.cpp
    ../../src/nbt/io/stream_writer.cpp
    ../../src/nbt/io/stream_reader.cpp
    ../../src/nbt/text/json_formatter.cpp
    ../../src/dictReader.cpp
    main.cpp
)

add_executable(merge ${PROGRAM_SOURCES})

target_include_directories(merge PUBLIC
    ../../include
    ../../include/nbt
    ${PROJECT_BINARY_DIR}
)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(merge PUBLIC nlohmann_json::nlohmann_json)