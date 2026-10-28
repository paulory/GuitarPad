cmake_minimum_required (VERSION 3.22)

project (GuitarPad VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
if (APPLE)
    enable_language (OBJCXX)
endif()

option (GUITARPAD_BUILD_APP "Compila o app JUCE" ON)

# ---------- nucleo (sem JUCE) + testes ----------
add_library (guitarpad_core STATIC
    Source/Core/NoteDetector.cpp
    Source/Core/MappingEngine.cpp)
target_include_directories (guitarpad_core PUBLIC Source/Core)
find_package (Threads REQUIRED)
target_link_libraries (guitarpad_core PUBLIC Threads::Threads)

enable_testing()
add_executable (test_detector tests/test_detector.cpp)
target_link_libraries (test_detector PRIVATE guitarpad_core)
add_test (NAME detector COMMAND test_detector)
add_executable (test_mapping tests/test_mapping.cpp)
target_link_libraries (test_mapping PRIVATE guitarpad_core)
add_test (NAME mapping COMMAND test_mapping)

if (MSVC)
    add_compile_options (/utf-8)
    target_compile_options (guitarpad_core PRIVATE /utf-8)
    target_compile_options (test_detector PRIVATE /utf-8)
    target_compile_options (test_mapping PRIVATE /utf-8)
endif()

if (NOT GUITARPAD_BUILD_APP)
    return()
endif()

# ---------- app ----------
include (FetchContent)
FetchContent_Declare (JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable (JUCE)

juce_add_gui_app (GuitarPad
    PRODUCT_NAME "GuitarPad"
    COMPANY_NAME "Tofu"
    BUNDLE_ID "com.tofu.guitarpad"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "O GuitarPad escuta a entrada da guitarra para detectar as notas.")

juce_generate_juce_header (GuitarPad)

target_sources (GuitarPad PRIVATE
    Source/App/Main.cpp
    Source/App/MainComponent.cpp)

if (WIN32)
    target_sources (GuitarPad PRIVATE Source/Platform/KeySenderWin.cpp)
    target_link_libraries (GuitarPad PRIVATE winmm)
elseif (APPLE)
    target_sources (GuitarPad PRIVATE Source/Platform/KeySenderMac.mm)
    target_link_libraries (GuitarPad PRIVATE "-framework ApplicationServices")
else()
    target_sources (GuitarPad PRIVATE Source/Platform/KeySenderStub.cpp)
endif()

target_compile_definitions (GuitarPad PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GuitarPad,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GuitarPad,JUCE_VERSION>")

target_link_libraries (GuitarPad
    PRIVATE
        guitarpad_core
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
