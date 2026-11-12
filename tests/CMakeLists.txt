find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# Tests for the core library: crypto, password generator, file format, vault.
add_executable(core_tests
        test_main.cpp
        test_crypto.cpp
        test_password_generator.cpp
        test_serialization.cpp
        test_vault_file.cpp
        test_vault.cpp
)
target_link_libraries(core_tests PRIVATE vault_core GTest::gtest)
# PRE_TEST: list the tests when ctest runs, not at build time, so the DLLs
# that vcpkg copies next to the .exe are already there.
gtest_discover_tests(core_tests DISCOVERY_MODE PRE_TEST)

# GUI tests drive the real dialogs and main window through their widgets.
# They use Qt's "offscreen" platform, so no window appears on screen and they
# also work on CI machines without a display.
if (BUILD_GUI)
    add_executable(gui_tests
            gui/gui_test_main.cpp
            gui/test_unlock_dialog.cpp
            gui/test_main_window.cpp
            ../src/gui/unlock_dialog.h
            ../src/gui/unlock_dialog.cpp
            ../src/gui/main_window.h
            ../src/gui/main_window.cpp
            ../src/gui/entry_dialog.h
            ../src/gui/entry_dialog.cpp
    )
    target_link_libraries(gui_tests PRIVATE vault_core GTest::gtest Qt6::Widgets Qt6::Concurrent)
    gtest_discover_tests(gui_tests DISCOVERY_MODE PRE_TEST
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    if (WIN32)
        add_custom_command(TARGET gui_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:gui_tests>/platforms"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                        "$<TARGET_FILE_DIR:gui_tests>/platforms")
    endif ()
endif ()
