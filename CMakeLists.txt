cmake_minimum_required(VERSION 3.5)
project(wordle LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY lib)

add_subdirectory(res/merge)

set(MERGE_EXEC $<TARGET_FILE:merge>)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/accept.dict
    COMMAND ${MERGE_EXEC} accept.dict plain ${CMAKE_CURRENT_SOURCE_DIR}/res/allowed_words.txt json ${CMAKE_CURRENT_SOURCE_DIR}/res/dictionary.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS merge ${CMAKE_CURRENT_SOURCE_DIR}/res/allowed_words.txt ${CMAKE_CURRENT_SOURCE_DIR}/res/dictionary.json
    COMMENT "Generating accept.dict"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/answer.dict
    COMMAND ${MERGE_EXEC} answer.dict plain ${CMAKE_CURRENT_SOURCE_DIR}/res/possible_words.txt json ${CMAKE_CURRENT_SOURCE_DIR}/res/targets.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS merge ${CMAKE_CURRENT_SOURCE_DIR}/res/possible_words.txt ${CMAKE_CURRENT_SOURCE_DIR}/res/targets.json
    COMMENT "Generating answer.dict"
)

add_custom_target(generate_dicts ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/accept.dict
            ${CMAKE_CURRENT_BINARY_DIR}/answer.dict
)

set(LIBRARY_SOURCES
    src/ast/ast.cpp
    src/env/environment.cpp
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/util.cpp
    src/object/object.cpp
    src/program/program.cpp
    src/vm/vm.cpp
    src/vm/gct.cpp
    src/plugins/plugin.cpp
    src/plugins/base.cpp
    src/plugins/io.cpp
    src/plugins/math.cpp
    src/plugins/fileio.cpp
    src/plugins/wordle.cpp
    src/plugins/dynamic_load.cpp
    src/nbt/endian_str.cpp
    src/nbt/tag.cpp
    src/nbt/tag_compound.cpp
    src/nbt/tag_list.cpp
    src/nbt/tag_string.cpp
    src/nbt/value.cpp
    src/nbt/value_initializer.cpp
    src/nbt/io/stream_writer.cpp
    src/nbt/io/stream_reader.cpp
    src/nbt/text/json_formatter.cpp
    src/logger.cpp
    src/cursor.cpp
    src/char.cpp
    src/paintbrush/paintbrush.cpp
    src/services/mouse.service.cpp
    src/services/keybd.service.cpp
    src/widgets/widget.cpp
    src/widgets/index.cpp
    src/widgets/router.cpp
    src/widgets/text.cpp
    src/widgets/layout.cpp
    src/widgets/button.cpp
    src/widgets/icon.cpp
    src/widgets/link.cpp
    src/widgets/datetime.cpp
    src/widgets/switch.cpp
    src/widgets/input.cpp
    src/widgets/menu.cpp
    src/widgets/longmenu.cpp
    src/ipc.cpp
    src/render.cpp
    src/tick.cpp
    src/env.cpp
    src/util/dialog.cpp
    src/i18n.cpp
    src/options.cpp
    src/dictReader.cpp
    src/bcrypt.cpp
    src/blowfish.cpp
    src/userdb.cpp
    src/game_context.cpp
    src/dict_context.cpp
)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc i18n data)

set(PROGRAM_SOURCES src/main.cpp)
add_library(libwordle STATIC ${LIBRARY_SOURCES})
target_include_directories(libwordle PUBLIC
    include
    include/nbt
    ${PROJECT_BINARY_DIR}
)

target_link_libraries(libwordle PUBLIC 
    nlohmann_json::nlohmann_json 
    unofficial::sqlite3::sqlite3 
    ICU::uc 
    ICU::i18n 
    ICU::data
)
add_executable(wordle4 ${PROGRAM_SOURCES})

if(WIN32)
    enable_language(RC)
    target_sources(wordle4 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources.rc)
endif()

target_link_libraries(wordle4 PRIVATE libwordle)

add_dependencies(wordle4 generate_dicts)

# Copy generated dict files to output directory
add_custom_target(copy_dicts ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:wordle4>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/accept.dict
        ${CMAKE_CURRENT_BINARY_DIR}/answer.dict
        $<TARGET_FILE_DIR:wordle4>
    DEPENDS generate_dicts wordle4
    COMMENT "Copying dict files to wordle4 output dir"
)

# Copy common resource files to output directory
add_custom_target(copy_common_resources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:wordle4>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res/common
        $<TARGET_FILE_DIR:wordle4>
    DEPENDS wordle4
    COMMENT "Copying common resource files to output directory"
)