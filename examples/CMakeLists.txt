set(INCLUDE_PATH "${RLP_ROOT_PATH}/include")

# Find or fetch raylib
find_package(raylib 5.0 QUIET)
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
  target_link_libraries(${example_name} PRIVATE raylib RLP::RaylibLiveWallpaper)
  target_include_directories(${example_name} PRIVATE ${RAYLIB_PATH} ${INCLUDE_PATH})

  # Compiler-specific options
  if(MSVC)
    target_compile_definitions(${example_name} PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )
    target_compile_options(${example_name} PRIVATE /W3)
  else()
    target_compile_options(${example_name} PRIVATE -Wall -Wextra)

    # Add debug flags for better debugging experience
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${example_name} PRIVATE -g -O0)
    endif()

    # Special flags for Objective-C++ on macOS
    if(APPLE)
      target_compile_options(${example_name} PRIVATE 
        $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
        $<$<COMPILE_LANGUAGE:OBJCXX>:-fno-objc-exceptions>
      )
      
      # Ensure debug symbols for .mm files
      if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${example_name} PRIVATE 
          $<$<COMPILE_LANGUAGE:OBJCXX>:-gdwarf-2>
        )
      endif()
    endif()
  endif()

  # Copy assets to the correct location for each platform if you have any
  # add_custom_command(TARGET ${example_name} POST_BUILD
  #   COMMAND ${CMAKE_COMMAND} -E copy_directory
  #   ${CMAKE_CURRENT_SOURCE_DIR}/assets
  #   ${CMAKE_BINARY_DIR}/bin/assets
  # )

  if(APPLE)
    # For macOS app bundles, also copy assets into the app bundle Resources directory
    # add_custom_command(TARGET ${example_name} POST_BUILD
    #   COMMAND ${CMAKE_COMMAND} -E make_directory
    #   ${CMAKE_BINARY_DIR}/bin/${example_name}.app/Contents/Resources
    #   COMMAND ${CMAKE_COMMAND} -E copy_directory
    #   ${CMAKE_CURRENT_SOURCE_DIR}/assets
    #   ${CMAKE_BINARY_DIR}/bin/${example_name}.app/Contents/Resources
    # )

    set_target_properties(${example_name} PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.rayliblivewallpaper"
      MACOSX_BUNDLE_BUNDLE_NAME "Raylib Live Wallpaper"
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
  endif() 
endfunction()

add_example(rlp_demo "examples/demo/main.cpp")