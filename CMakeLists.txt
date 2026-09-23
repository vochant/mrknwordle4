cmake_minimum_required(VERSION 3.16)
project(wordle VERSION 4.1 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

set(WORDLE_LOG_LEVEL "info" CACHE STRING "Minimum log level: debug, info, warn or error")
set_property(CACHE WORDLE_LOG_LEVEL PROPERTY STRINGS debug info warn error)
if(WORDLE_LOG_LEVEL STREQUAL "debug")
    set(WORDLE_LOG_LEVEL_VALUE 0)
elseif(WORDLE_LOG_LEVEL STREQUAL "info")
    set(WORDLE_LOG_LEVEL_VALUE 1)
elseif(WORDLE_LOG_LEVEL STREQUAL "warn")
    set(WORDLE_LOG_LEVEL_VALUE 2)
elseif(WORDLE_LOG_LEVEL STREQUAL "error")
    set(WORDLE_LOG_LEVEL_VALUE 3)
else()
    message(FATAL_ERROR "WORDLE_LOG_LEVEL must be debug, info, warn or error")
endif()

add_library(wordle_build INTERFACE)
target_include_directories(wordle_build INTERFACE include include/nbt)
target_compile_options(wordle_build INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:/EHsc;/utf-8>")

include(cmake/Dependencies.cmake)

add_library(wordle_dictionary STATIC
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
    src/dictReader.cpp
)

target_link_libraries(wordle_dictionary PUBLIC wordle_build wordle_json)

add_library(wordle_i18n STATIC
    src/i18n.cpp
    src/i18n_latin.cpp
    src/i18n_mf.cpp
    src/locale.cpp
)
target_link_libraries(wordle_i18n PUBLIC wordle_build wordle_icu wordle_json)

add_library(libwordle STATIC
    src/logger.cpp
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
    src/widgets/link.cpp
    src/widgets/datetime.cpp
    src/widgets/switch.cpp
    src/widgets/input.cpp
    src/widgets/menu.cpp
    src/widgets/longmenu.cpp
    src/eventbus.cpp
    src/platform.cpp
    src/terminal/terminal.cpp
    src/render.cpp
    src/util/dialog.cpp
    src/options.cpp
    src/bcrypt.cpp
    src/blowfish.cpp
    src/userdb.cpp
    src/game_context.cpp
    src/dict_context.cpp
    src/terminal/session.cpp
    src/ui/application.cpp
    src/ui/dict.cpp
    src/ui/game.cpp
    src/ui/history.cpp
    src/ui/menu.cpp
    src/ui/setup.cpp
    src/ui/plugins.cpp
    src/ui/user.cpp
    src/registry.cpp
    src/dictionary.cpp
    src/plugins/manager.cpp
    src/color.cpp
    src/plugins/runtime.cpp
    src/terminal/unicode.cpp
    src/plugins/files.cpp
)

target_link_libraries(libwordle PUBLIC wordle_dictionary wordle_i18n wordle_sqlite Threads::Threads)
target_compile_definitions(libwordle PRIVATE WORDLE_LOG_LEVEL=${WORDLE_LOG_LEVEL_VALUE})
include(cmake/Terminal.cmake)
include(cmake/Plugins.cmake)

set(WORDLE_MERGE_EXECUTABLE "" CACHE FILEPATH "Native dictionary compiler for cross builds")
if(WORDLE_MERGE_EXECUTABLE)
    add_executable(merge IMPORTED GLOBAL)
    set_target_properties(merge PROPERTIES IMPORTED_LOCATION "${WORDLE_MERGE_EXECUTABLE}" CROSSCOMPILING_EMULATOR "")
elseif(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    message(FATAL_ERROR "Cross builds require WORDLE_MERGE_EXECUTABLE or CMAKE_CROSSCOMPILING_EMULATOR")
else()
    add_executable(merge res/merge/main.cpp)
    target_link_libraries(merge PRIVATE wordle_dictionary)
endif()

add_executable(wordle4 src/main.cpp)
target_link_libraries(wordle4 PRIVATE libwordle)
target_compile_definitions(wordle4 PRIVATE WORDLE_VERSION="${PROJECT_VERSION}")
if(WIN32)
    enable_language(RC)
    target_sources(wordle4 PRIVATE resources.rc)
endif()

add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/accept.dict"
    COMMAND merge "${PROJECT_BINARY_DIR}/accept.dict"
        plain "${PROJECT_SOURCE_DIR}/res/allowed_words.txt"
        json "${PROJECT_SOURCE_DIR}/res/dictionary.json"
    DEPENDS merge res/allowed_words.txt res/dictionary.json
    COMMENT "Generating accept.dict"
    VERBATIM)

add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/answer.dict"
    COMMAND merge "${PROJECT_BINARY_DIR}/answer.dict"
        plain "${PROJECT_SOURCE_DIR}/res/possible_words.txt"
        json "${PROJECT_SOURCE_DIR}/res/targets.json"
    DEPENDS merge res/possible_words.txt res/targets.json
    COMMENT "Generating answer.dict"
    VERBATIM)

add_custom_target(generate_dicts ALL
    DEPENDS
        "${PROJECT_BINARY_DIR}/accept.dict"
        "${PROJECT_BINARY_DIR}/answer.dict")
add_dependencies(wordle4 generate_dicts)

set(WORDLE_CHECK_I18N_EXECUTABLE "" CACHE FILEPATH "Native i18n checker for cross builds")
if(WORDLE_CHECK_I18N_EXECUTABLE)
    add_executable(i18n_check IMPORTED GLOBAL)
    set_target_properties(i18n_check PROPERTIES IMPORTED_LOCATION "${WORDLE_CHECK_I18N_EXECUTABLE}" CROSSCOMPILING_EMULATOR "")
elseif(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    message(FATAL_ERROR "Cross builds require WORDLE_CHECK_I18N_EXECUTABLE or CMAKE_CROSSCOMPILING_EMULATOR")
else()
    add_executable(i18n_check res/check_i18n/main.cpp)
    target_link_libraries(i18n_check PRIVATE wordle_build wordle_json)
endif()

file(GLOB WORDLE_I18N_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/res/i18n/*")
add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/i18n/locales.json"
    COMMAND i18n_check "${PROJECT_SOURCE_DIR}/res/i18n" "${PROJECT_BINARY_DIR}/i18n"
    DEPENDS i18n_check ${WORDLE_I18N_FILES}
    COMMENT "Selecting i18n catalogs by message keys"
    VERBATIM)
add_custom_target(generate_i18n ALL DEPENDS "${PROJECT_BINARY_DIR}/i18n/locales.json")
add_custom_target(check_i18n DEPENDS "${PROJECT_BINARY_DIR}/i18n/locales.json")

# Copy generated dict files to output directory
add_custom_target(copy_dicts ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:wordle4>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_BINARY_DIR}/accept.dict"
        "${PROJECT_BINARY_DIR}/answer.dict"
        $<TARGET_FILE_DIR:wordle4>
    DEPENDS generate_dicts wordle4
    COMMENT "Copying dict files to wordle4 output dir")

# Copy common resource files to output directory
add_custom_target(copy_common_resources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:wordle4>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/res/common"
        $<TARGET_FILE_DIR:wordle4>
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        $<TARGET_FILE_DIR:wordle4>/i18n
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/i18n"
        $<TARGET_FILE_DIR:wordle4>/i18n
    DEPENDS generate_i18n wordle4
    COMMENT "Copying common resource files to output directory")

install(FILES "${PROJECT_SOURCE_DIR}/doc/wordle4.6"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")
