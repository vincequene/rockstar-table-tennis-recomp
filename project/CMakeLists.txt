# tabletennis - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(tabletennis LANGUAGES C CXX)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(TABLETENNIS_SOURCES
    src/main.cpp
    src/discord_presence.cpp
)

if(WIN32)
    add_executable(tabletennis WIN32 ${TABLETENNIS_SOURCES})
else()
    add_executable(tabletennis ${TABLETENNIS_SOURCES})
endif()

rexglue_setup_target(tabletennis)


if(WIN32)
    target_link_libraries(tabletennis PRIVATE xinput)
endif()

# Nom et icone du programme
if(WIN32)
    # The icon is not shipped in the repository (game artwork): it is used only
    # if res/tabletennis.ico exists locally.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/tabletennis.ico")
        target_sources(tabletennis PRIVATE res/tabletennis.rc)
    else()
        target_sources(tabletennis PRIVATE res/version.rc)
    endif()
    set_target_properties(tabletennis PROPERTIES OUTPUT_NAME "Rockstar Table Tennis")
endif()
