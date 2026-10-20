cmake_minimum_required(VERSION 3.20)
project(DOOMDarkAges LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.11
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

add_executable(DOOMDarkAges src/launcher.cpp)
target_link_libraries(DOOMDarkAges PRIVATE SDL2::SDL2)
if(WIN32)
    target_link_libraries(DOOMDarkAges PRIVATE SDL2::SDL2main)
endif()

if(MSVC)
    target_compile_options(DOOMDarkAges PRIVATE /W4 /permissive-)
else()
    target_compile_options(DOOMDarkAges PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_custom_command(TARGET DOOMDarkAges POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2>" "$<TARGET_FILE_DIR:DOOMDarkAges>"
    )
endif()