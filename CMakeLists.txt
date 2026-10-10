cmake_minimum_required(VERSION 3.16)
project(CityBloxx VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(CBX_BUILD_GAME "Build the SDL3 game executable" ON)

# ---------------------------------------------------------------------------------------------
# Engine + game core (no SDL dependency)
#
# Bit-exact behaviour relies on Java semantics: wrapping signed arithmetic and IEEE float math
# without fused multiply-add, so these flags are part of the contract, not an optimisation choice.
# ---------------------------------------------------------------------------------------------
file(GLOB CBX_CORE_SOURCES CONFIGURE_DEPENDS
  src/jcompat/*.cpp src/gfx/*.cpp src/platform/*.cpp src/audio/*.cpp
  src/game/*.cpp src/game/gen/*.cpp src/app/bootstrap.cpp src/app/name_dialog_view.cpp)

add_library(cbx_core STATIC ${CBX_CORE_SOURCES})
target_include_directories(cbx_core PUBLIC src src/game)
if(MSVC)
  target_compile_options(cbx_core PUBLIC /utf-8 /fp:strict /EHsc /wd4244 /wd4267 /wd4018)
else()
  target_compile_options(cbx_core PUBLIC -fwrapv -ffp-contract=off -Wall -Wno-unused-variable -Wno-parentheses)
endif()

# ---------------------------------------------------------------------------------------------
# The game
# ---------------------------------------------------------------------------------------------
if(CBX_BUILD_GAME)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/lib/cmake/SDL3")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl3")
  endif()
  find_package(SDL3 CONFIG QUIET)
  if(NOT SDL3_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
      add_library(SDL3::SDL3 ALIAS PkgConfig::SDL3)
    else()
      message(FATAL_ERROR "SDL3 development files not found (install SDL3 or set CBX_BUILD_GAME=OFF)")
    endif()
  endif()
  add_executable(citybloxx src/app/main.cpp)
  target_link_libraries(citybloxx PRIVATE cbx_core SDL3::SDL3)
  if(WIN32)
    set_target_properties(citybloxx PROPERTIES WIN32_EXECUTABLE ON)
  endif()
  # run from the build tree: assets are found next to the executable or in ./assets
  add_custom_command(TARGET citybloxx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:citybloxx>/assets)
  if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/bin/SDL3.dll")
    add_custom_command(TARGET citybloxx POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/sdl3/bin/SDL3.dll $<TARGET_FILE_DIR:citybloxx>)
  endif()
  install(TARGETS citybloxx RUNTIME DESTINATION bin)
  install(DIRECTORY assets/ DESTINATION share/citybloxx/assets)
endif()


