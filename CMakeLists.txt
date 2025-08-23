cmake_minimum_required(VERSION 3.20)

project(RaylibLiveWallpaper
  VERSION 0.1.0
  DESCRIPTION "Live wallpapers with raylib"
  LANGUAGES C CXX)

set(RLP_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

  # Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only turn on ObjC/ObjCXX on Apple, so Windows stays happy
if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

# Compat for PROJECT_IS_TOP_LEVEL on older CMake
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
  if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(PROJECT_IS_TOP_LEVEL ON)
  else()
    set(PROJECT_IS_TOP_LEVEL OFF)
  endif()
endif()

include(GNUInstallDirs)
option(RLP_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})

# Library target.
add_library(RaylibLiveWallpaper STATIC
  $<$<PLATFORM_ID:Windows>:src/platform/windows/desktop_integration_windows.cpp>
  $<$<PLATFORM_ID:Darwin>:src/platform/macos/desktop_integration_macos.mm>
  $<$<PLATFORM_ID:Linux>:src/platform/linux/desktop_integration_linux.cpp>
  include/desktop_integration.h
)

add_library(RLP::RaylibLiveWallpaper ALIAS RaylibLiveWallpaper)

target_compile_features(RaylibLiveWallpaper PUBLIC cxx_std_20)

target_include_directories(RaylibLiveWallpaper
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Platform links
if(WIN32)
  target_compile_definitions(RaylibLiveWallpaper PUBLIC RLP_PLATFORM_WINDOWS)
  target_link_libraries(RaylibLiveWallpaper PRIVATE user32 gdi32 shcore shlwapi dwmapi)
elseif(APPLE)
  target_compile_definitions(RaylibLiveWallpaper PUBLIC RLP_PLATFORM_MACOS)

  find_library(COCOA_LIBRARY Cocoa)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(COREVIDEO_LIBRARY CoreVideo)
  find_library(OPENGL_LIBRARY OpenGL)
  find_library(CARBON_LIBRARY Carbon)
  
  target_link_libraries(RaylibLiveWallpaper PRIVATE
      ${COCOA_LIBRARY}
      ${IOKIT_LIBRARY}
      ${COREVIDEO_LIBRARY}
      ${OPENGL_LIBRARY}
      ${CARBON_LIBRARY}
  )
elseif(UNIX)
    find_package(X11 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XRANDR REQUIRED xrandr)

    target_link_libraries(RaylibLiveWallpaper PRIVATE
        ${X11_LIBRARIES}
        ${XRANDR_LIBRARIES}
    )

    target_include_directories(RaylibLiveWallpaper PRIVATE
        ${XRANDR_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
    )
endif()

# Warnings for the lib itself
if(MSVC)
  target_compile_options(RaylibLiveWallpaper PRIVATE /W4 /permissive-)
else()
  target_compile_options(RaylibLiveWallpaper PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -------- Packaging: install + Config files --------
include(CMakePackageConfigHelpers)
set(RLP_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/RaylibLiveWallpaper)

install(TARGETS RaylibLiveWallpaper
  EXPORT RaylibLiveWallpaperTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT RaylibLiveWallpaperTargets
  NAMESPACE RLP::
  DESTINATION ${RLP_INSTALL_CONFIGDIR}
  FILE RaylibLiveWallpaperTargets.cmake
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/RaylibLiveWallpaperConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/RaylibLiveWallpaperConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/RaylibLiveWallpaperConfig.cmake
  INSTALL_DESTINATION ${RLP_INSTALL_CONFIGDIR}
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/RaylibLiveWallpaperConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/RaylibLiveWallpaperConfigVersion.cmake
  DESTINATION ${RLP_INSTALL_CONFIGDIR}
)

# -------- Examples --------
if(RLP_BUILD_EXAMPLES)
    include("${RLP_ROOT_PATH}/examples/CMakeLists.txt")
endif()