option(WORDLE_ENABLE_CURSES "Build the curses terminal backend" ON)
set(WORDLE_CURSES_PROVIDER "AUTO" CACHE STRING "Curses provider: AUTO, NONE, NCURSES, PDCURSES")
set_property(CACHE WORDLE_CURSES_PROVIDER PROPERTY STRINGS AUTO NONE NCURSES PDCURSES)
set(WORDLE_PDCURSES_FORCE_UTF8 "AUTO" CACHE STRING "PDCursesMod ABI: AUTO, ON, OFF")
set_property(CACHE WORDLE_PDCURSES_FORCE_UTF8 PROPERTY STRINGS AUTO ON OFF)

include(CheckCXXSourceCompiles)

check_cxx_source_compiles([[
    #include <poll.h>
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <unistd.h>
    int main() {
        termios state {};
        pollfd descriptor {};
        winsize size {};
        char byte = 0;
        return isatty(0) + tcgetattr(0, &state) + tcsetattr(0, TCSANOW, &state) +
               ioctl(1, TIOCGWINSZ, &size) + poll(&descriptor, 1, 0) + read(0, &byte, 1) + write(1, &byte, 1);
    }
]] WORDLE_HAS_ANSI_API)

if(WORDLE_HAS_ANSI_API)
    target_sources(libwordle PRIVATE src/terminal/ansi.cpp)
    target_compile_definitions(libwordle PRIVATE WORDLE_HAS_ANSI=1)
endif()

if(WIN32)
    target_sources(libwordle PRIVATE src/terminal/win32.cpp)
    target_link_libraries(libwordle PUBLIC shell32 user32)
    target_compile_definitions(libwordle PRIVATE WORDLE_HAS_WIN32=1)
endif()

if(NOT WORDLE_HAS_ANSI_API AND NOT WIN32 AND NOT WORDLE_ENABLE_CURSES)
    message(FATAL_ERROR "No terminal backend is available")
endif()

if(NOT WORDLE_ENABLE_CURSES)
    return()
endif()

string(TOUPPER "${WORDLE_CURSES_PROVIDER}" provider)
if(provider STREQUAL "AUTO")
    if(WIN32)
        set(provider PDCURSES)
    else()
        set(provider NCURSES)
    endif()
    set(curses_optional TRUE)
elseif(provider STREQUAL "NONE")
    return()
elseif(NOT provider STREQUAL "NCURSES" AND NOT provider STREQUAL "PDCURSES")
    message(FATAL_ERROR "Unknown WORDLE_CURSES_PROVIDER: ${WORDLE_CURSES_PROVIDER}")
endif()

add_library(wordle_curses INTERFACE)
set(curses_found FALSE)

function(check_pdcursesmod result)
    cmake_parse_arguments(ARG "FORCE_UTF8" "" "INCLUDES;LIBRARIES" ${ARGN})
    set(CMAKE_REQUIRED_INCLUDES ${ARG_INCLUDES})
    set(CMAKE_REQUIRED_LIBRARIES ${ARG_LIBRARIES})
    set(CMAKE_REQUIRED_DEFINITIONS -DPDC_WIDE=1 -DPDC_NCMOUSE=1)
    if(ARG_FORCE_UTF8)
        list(APPEND CMAKE_REQUIRED_DEFINITIONS -DPDC_FORCE_UTF8=1)
    endif()
    unset(${result} CACHE)
    check_cxx_source_compiles([[
        #include <curses.h>
        #ifndef __PDCURSESMOD__
        #error "PDCursesMod is required"
        #endif
        int main() {
            return endwin() + init_extended_color(0, 0, 0, 0) + init_extended_pair(0, 0, 0);
        }
    ]] ${result})
    set(${result} ${${result}} PARENT_SCOPE)
endfunction()

if(provider STREQUAL "PDCURSES")
    option(WORDLE_PDCURSES_DLL "Link a Windows PDCurses DLL" OFF)
    set(PDCURSES_EXTRA_LIBRARIES "" CACHE STRING "Additional PDCursesMod port libraries")
    string(TOUPPER "${WORDLE_PDCURSES_FORCE_UTF8}" pdc_force_utf8)
    if(NOT pdc_force_utf8 STREQUAL "AUTO" AND NOT pdc_force_utf8 STREQUAL "ON" AND NOT pdc_force_utf8 STREQUAL "OFF")
        message(FATAL_ERROR "WORDLE_PDCURSES_FORCE_UTF8 must be AUTO, ON or OFF")
    endif()
    find_package(PDCurses CONFIG QUIET)
    find_package(unofficial-pdcurses CONFIG QUIET)
    if(TARGET PDCurses::PDCurses)
        set(pdc_target PDCurses::PDCurses)
    elseif(TARGET unofficial::pdcurses::pdcurses)
        set(pdc_target unofficial::pdcurses::pdcurses)
    endif()
    if(pdc_target)
        if(NOT pdc_force_utf8 STREQUAL "ON")
            check_pdcursesmod(WORDLE_HAS_PDCURSESMOD_TARGET LIBRARIES ${pdc_target} ${PDCURSES_EXTRA_LIBRARIES})
        endif()
        if(NOT WORDLE_HAS_PDCURSESMOD_TARGET AND NOT pdc_force_utf8 STREQUAL "OFF")
            check_pdcursesmod(WORDLE_HAS_PDCURSESMOD_TARGET_UTF8 FORCE_UTF8
                              LIBRARIES ${pdc_target} ${PDCURSES_EXTRA_LIBRARIES})
        endif()
    endif()
    if(WORDLE_HAS_PDCURSESMOD_TARGET OR WORDLE_HAS_PDCURSESMOD_TARGET_UTF8)
        target_link_libraries(wordle_curses INTERFACE ${pdc_target})
        set(curses_found TRUE)
        if(WORDLE_HAS_PDCURSESMOD_TARGET_UTF8)
            set(pdc_force_utf8 TRUE)
        endif()
    else()
        find_path(PDCURSES_INCLUDE_DIR curses.h PATH_SUFFIXES pdcurses)
        find_library(PDCURSES_LIBRARY NAMES pdcursesw pdcurses XCurses)
        if(PDCURSES_INCLUDE_DIR AND PDCURSES_LIBRARY)
            if(NOT pdc_force_utf8 STREQUAL "ON")
                check_pdcursesmod(WORDLE_HAS_PDCURSESMOD_MANUAL
                                  INCLUDES ${PDCURSES_INCLUDE_DIR}
                                  LIBRARIES ${PDCURSES_LIBRARY} ${PDCURSES_EXTRA_LIBRARIES})
            endif()
            if(NOT WORDLE_HAS_PDCURSESMOD_MANUAL AND NOT pdc_force_utf8 STREQUAL "OFF")
                check_pdcursesmod(WORDLE_HAS_PDCURSESMOD_MANUAL_UTF8 FORCE_UTF8
                                  INCLUDES ${PDCURSES_INCLUDE_DIR}
                                  LIBRARIES ${PDCURSES_LIBRARY} ${PDCURSES_EXTRA_LIBRARIES})
            endif()
        endif()
        if(WORDLE_HAS_PDCURSESMOD_MANUAL OR WORDLE_HAS_PDCURSESMOD_MANUAL_UTF8)
            target_include_directories(wordle_curses INTERFACE ${PDCURSES_INCLUDE_DIR})
            target_link_libraries(wordle_curses INTERFACE ${PDCURSES_LIBRARY})
            set(curses_found TRUE)
            if(WORDLE_HAS_PDCURSESMOD_MANUAL_UTF8)
                set(pdc_force_utf8 TRUE)
            endif()
        endif()
    endif()
    if(curses_found)
        target_compile_definitions(wordle_curses INTERFACE PDC_WIDE=1 PDC_NCMOUSE=1 WORDLE_CURSES_PDCURSES=1
                                                          WORDLE_PDCURSES_EXTENDED=1)
        if(pdc_force_utf8 STREQUAL "TRUE")
            target_compile_definitions(wordle_curses INTERFACE PDC_FORCE_UTF8=1)
        endif()
    endif()
    target_link_libraries(wordle_curses INTERFACE ${PDCURSES_EXTRA_LIBRARIES})
    if(WORDLE_PDCURSES_DLL)
        target_compile_definitions(wordle_curses INTERFACE PDC_DLL_BUILD=1)
    endif()
elseif(provider STREQUAL "NCURSES")
    if(PkgConfig_FOUND)
        pkg_check_modules(NCURSESW QUIET IMPORTED_TARGET ncursesw)
    endif()
    if(TARGET PkgConfig::NCURSESW)
        target_link_libraries(wordle_curses INTERFACE PkgConfig::NCURSESW)
        set(curses_found TRUE)
    else()
        set(CURSES_NEED_WIDE TRUE)
        set(CURSES_NEED_NCURSES TRUE)
        find_package(Curses QUIET)
        if(Curses_FOUND)
            target_include_directories(wordle_curses INTERFACE ${CURSES_INCLUDE_DIRS})
            target_link_libraries(wordle_curses INTERFACE ${CURSES_LIBRARIES})
            set(curses_found TRUE)
        endif()
    endif()
endif()

if(NOT curses_found)
    if(curses_optional)
        message(STATUS "System ${provider} was not found; curses backend disabled")
        return()
    endif()
    if(provider STREQUAL "PDCURSES")
        message(FATAL_ERROR "PDCurses provider requires PDCursesMod with wide characters and extended color APIs")
    endif()
    message(FATAL_ERROR "Requested ncurses provider was not found")
endif()

target_sources(libwordle PRIVATE src/terminal/curses.cpp)
target_compile_definitions(libwordle PRIVATE WORDLE_HAS_CURSES=1)
target_link_libraries(libwordle PUBLIC wordle_curses)
