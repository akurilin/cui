# Require at least CMake version 3.16 so newer commands/features used here are available.
cmake_minimum_required(VERSION 3.16)
# Define the project name; this is the root CMake project in this repository.
project(hello)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
# Put built executable files in the build directory (or config subfolder for multi-config generators).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
# Put built shared/static libraries in the build directory (or config subfolder for multi-config generators).
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# This assumes the SDL source is available in vendored/SDL
# Build the SDL project that lives in vendored/SDL and make its targets available to this project.
# EXCLUDE_FROM_ALL means SDL won't be built unless something depends on it (which hello does below).
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)

# Create your game executable target as usual
# Define an executable target named "hello"; WIN32 changes subsystem behavior on Windows only.
add_executable(
    hello
    WIN32
    main.c
    src/ui/ui_button.c
    src/ui/ui_checkbox.c
    src/ui/ui_context.c
    src/ui/ui_element.c
    src/ui/ui_fps_counter.c
    src/ui/ui_hrule.c
    src/ui/ui_pane.c
    src/ui/ui_image.c
    src/ui/ui_layout_container.c
    src/ui/ui_scroll_view.c
    src/ui/ui_segment_group.c
    src/ui/ui_slider.c
    src/ui/ui_text.c
    src/ui/ui_text_input.c
)

target_include_directories(hello PRIVATE include)

# Link to the actual SDL3 library.
# Link the "hello" target against SDL3, privately (dependency is used for building hello, not exposed to dependents).
target_link_libraries(hello PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)
