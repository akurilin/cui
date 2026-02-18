# Require at least CMake version 3.16 so newer commands/features used here are available.
cmake_minimum_required(VERSION 3.16)
# Define the project name; this is the root CMake project in this repository.
project(cui)
include(CTest)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
# Put built executable files in the build directory (or config subfolder for multi-config generators).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
# Put built shared/static libraries in the build directory (or config subfolder for multi-config generators).
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# This assumes the SDL source is available in vendored/SDL
# Build the SDL project that lives in vendored/SDL and make its targets available to this project.
# EXCLUDE_FROM_ALL means SDL won't be built unless something depends on it (which cui does below).
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)

file(GLOB PAGE_SOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/pages/*_page.c")
list(SORT PAGE_SOURCE_FILES)
if(NOT PAGE_SOURCE_FILES)
    message(FATAL_ERROR "No page sources found in src/pages/*.c matching *_page.c.")
endif()

set(PAGE_INDEX_FILE "${CMAKE_BINARY_DIR}/generated/page_index.c")
set(PAGE_INDEX_CONTENT "#include \"pages/app_page.h\"\n\n")
set(PAGE_DESCRIPTOR_REFERENCES)

foreach(PAGE_SOURCE_FILE IN LISTS PAGE_SOURCE_FILES)
    get_filename_component(PAGE_SOURCE_NAME "${PAGE_SOURCE_FILE}" NAME_WE)
    string(REGEX MATCH "^(.+)_page$" PAGE_ID_MATCH "${PAGE_SOURCE_NAME}")
    if(NOT PAGE_ID_MATCH)
        message(FATAL_ERROR "Page source does not follow <id>_page.c naming: ${PAGE_SOURCE_FILE}")
    endif()

    string(REGEX REPLACE "_page$" "" PAGE_ID "${PAGE_SOURCE_NAME}")
    set(PAGE_DESCRIPTOR_SYMBOL "${PAGE_ID}_page_ops")
    string(APPEND PAGE_INDEX_CONTENT
           "extern const app_page_ops ${PAGE_DESCRIPTOR_SYMBOL};\n")
    list(APPEND PAGE_DESCRIPTOR_REFERENCES
         "    { \"${PAGE_ID}\", &${PAGE_DESCRIPTOR_SYMBOL} }")
endforeach()

string(APPEND PAGE_INDEX_CONTENT "\nconst app_page_entry app_pages[] = {\n")
foreach(PAGE_DESCRIPTOR_REFERENCE IN LISTS PAGE_DESCRIPTOR_REFERENCES)
    string(APPEND PAGE_INDEX_CONTENT "${PAGE_DESCRIPTOR_REFERENCE},\n")
endforeach()
string(
    APPEND
    PAGE_INDEX_CONTENT
    "};\n\nconst size_t app_page_count = sizeof(app_pages) / sizeof(app_pages[0]);\n")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(WRITE "${PAGE_INDEX_FILE}" "${PAGE_INDEX_CONTENT}")

set(APP_SOURCES
    main.c
    ${PAGE_SOURCE_FILES}
    ${PAGE_INDEX_FILE}
    src/ui/ui_button.c
    src/ui/ui_checkbox.c
    src/system/ui_runtime.c
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
    src/ui/ui_window.c
    src/util/fail_fast.c
    src/util/string_util.c)

# Create your game executable target as usual
# Define an executable target named "cui"; WIN32 changes subsystem behavior on Windows only.
add_executable(
    cui
    WIN32
    ${APP_SOURCES})

target_include_directories(cui PRIVATE include)

# Link to the actual SDL3 library.
# Link the "cui" target against SDL3, privately (dependency is used for building cui, not exposed to dependents).
target_link_libraries(cui PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)

if(BUILD_TESTING)
    add_executable(
        ui_hierarchy_tests
        tests/ui_hierarchy_tests.c
        src/ui/ui_element.c
        src/ui/ui_layout_container.c
        src/ui/ui_pane.c
        src/ui/ui_scroll_view.c
    )

    target_include_directories(ui_hierarchy_tests PRIVATE include)
    target_link_libraries(ui_hierarchy_tests PRIVATE SDL3::SDL3)
    add_test(NAME ui_hierarchy_tests COMMAND ui_hierarchy_tests)
endif()
