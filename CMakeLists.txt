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
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2 nlohmann_json)

add_executable(DOOMDarkAges src/game.cpp)
target_link_libraries(DOOMDarkAges PRIVATE SDL2::SDL2 nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(DOOMDarkAges PRIVATE SDL2::SDL2main)
endif()

if(MSVC)
    target_compile_options(DOOMDarkAges PRIVATE /W4 /permissive-)
else()
    target_compile_options(DOOMDarkAges PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET DOOMDarkAges POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/levels" "$<TARGET_FILE_DIR:DOOMDarkAges>/levels"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/extracted" "$<TARGET_FILE_DIR:DOOMDarkAges>/assets/extracted"
)