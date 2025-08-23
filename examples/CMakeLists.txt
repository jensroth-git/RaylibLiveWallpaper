set(INCLUDE_PATH "${RLP_ROOT_PATH}/include")

# Try to find raylib first, then fetch a specific tag for determinism
find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
  )
  FetchContent_MakeAvailable(raylib)
endif()

function(add_example example_name source_file)
  add_executable(${example_name} ${source_file})

  # Link: rely on transitive include paths from raylib and your library
  target_link_libraries(${example_name} PRIVATE raylib RLP::RaylibLiveWallpaper)
  target_include_directories(${example_name} PRIVATE ${INCLUDE_PATH})

  if(MSVC)
    target_compile_definitions(${example_name} PRIVATE
      _CRT_SECURE_NO_WARNINGS
      WIN32_LEAN_AND_MEAN
    )
    target_compile_options(${example_name} PRIVATE /W3)

    # Optional: make it a GUI app without console window
    target_link_options(${example_name} PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  else()
    target_compile_options(${example_name} PRIVATE -Wall -Wextra)
    # Config scoped debug flags
    target_compile_options(${example_name} PRIVATE $<$<CONFIG:Debug>:-g -O0>)

    # Objective-C++ flags only if that language is enabled (Apple builds)
    if(APPLE)
      get_property(has_objcxx GLOBAL PROPERTY ENABLED_LANGUAGES)
      if("OBJCXX" IN_LIST has_objcxx)
        target_compile_options(${example_name} PRIVATE
          $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
          $<$<COMPILE_LANGUAGE:OBJCXX>:-fno-objc-exceptions>
          $<$<AND:$<COMPILE_LANGUAGE:OBJCXX>,$<CONFIG:Debug>>:-gdwarf-2>
        )
      endif()
    endif()
  endif()

  # macOS app bundle settings (optional)
  if(APPLE)
    set_target_properties(${example_name} PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.rlp.${example_name}"
      MACOSX_BUNDLE_BUNDLE_NAME "Raylib Live Wallpaper Demo"
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
  endif()

  # Optional: install the demo for packagers and CI artifacts
  install(TARGETS ${example_name}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE  DESTINATION "."
  )
endfunction()

# Source path is relative to this CMakeLists file
add_example(rlp_demo "examples/demo/main.cpp")