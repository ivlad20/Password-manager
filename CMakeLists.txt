cmake_minimum_required(VERSION 3.21...4.4)

# Must be set before project(): vcpkg reads them to decide which dependencies
# from vcpkg.json to install (Qt is only needed for the GUI).
option(BUILD_GUI "Build the Qt 6 graphical interface" ON)
if (BUILD_GUI)
    list(APPEND VCPKG_MANIFEST_FEATURES "gui")
endif ()


project(PasswordManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(unofficial-sodium CONFIG REQUIRED)

# Everything that does not depend on the user interface.
# Both the CLI and the GUI link against this library.
add_library(vault_core STATIC
        src/crypto/crypto.cpp
        src/crypto/password_generator.cpp
        src/core/vault.cpp
        src/storage/vault_file.cpp
        src/storage/serialization.cpp
)
target_include_directories(vault_core PUBLIC src)
target_link_libraries(vault_core PUBLIC unofficial-sodium::sodium)

# Command line interface
add_executable(PasswordManager
        src/main.cpp
        src/cli/console.cpp
        src/cli/cli.cpp
)
target_link_libraries(PasswordManager PRIVATE vault_core)

# Qt 6 desktop interface
if (BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
    qt_standard_project_setup()

    # WIN32 = a windowed application, without a console window behind it.
    qt_add_executable(PasswordManagerGui WIN32
            src/gui/main.cpp
            src/gui/unlock_dialog.h
            src/gui/unlock_dialog.cpp
            src/gui/main_window.h
            src/gui/main_window.cpp
            src/gui/entry_dialog.h
            src/gui/entry_dialog.cpp
            resources/app.rc
    )
    target_link_libraries(PasswordManagerGui PRIVATE vault_core Qt6::Widgets Qt6::Concurrent)

    # Qt loads these at runtime from folders next to the .exe, so copy them after each build.
    #   platforms/qwindows       - required, without it the app cannot open a window
    #   styles/qmodernwindowsstyle - native Windows 10/11 look instead of the generic Fusion style
    if (WIN32)
        add_custom_command(TARGET PasswordManagerGui POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:PasswordManagerGui>/platforms"
                        "$<TARGET_FILE_DIR:PasswordManagerGui>/styles"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                        "$<TARGET_FILE_DIR:PasswordManagerGui>/platforms"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Qt6::QModernWindowsStylePlugin>"
                        "$<TARGET_FILE_DIR:PasswordManagerGui>/styles"
                COMMENT "Copying Qt plugins next to PasswordManagerGui")
    endif ()
endif ()

# Automated tests (GoogleTest), run with: ctest --test-dir <build folder>
include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

# Copy the two executables into the "bin" folder of the install location
install(TARGETS PasswordManager
        # Also copy the DLLs the exe loads (libsodium.dll).
        # Windows' own DLLs are skipped: every PC already has them.
        RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*"
        DIRECTORIES "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
        RUNTIME DESTINATION bin)

if (BUILD_GUI)
    install(TARGETS PasswordManagerGui
            RUNTIME_DEPENDENCIES
            PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
            POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*"
            DIRECTORIES "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
            RUNTIME DESTINATION bin)

    # Generates a script that runs windeployqt on the installed exe:
    # it copies the Qt DLLs and plugins (platforms/, styles/, ...) next to it.
    qt_generate_deploy_app_script(
            TARGET PasswordManagerGui
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT "${deploy_script}")
endif ()

# Copies the Visual C++ runtime DLLs (msvcp140.dll, vcruntime140.dll, ...)
# so the app also starts on PCs without Visual Studio or the VC++ Redistributable.
include(InstallRequiredSystemLibraries)

# Installer settings (read by "cpack")
set(CPACK_GENERATOR "INNOSETUP")
set(CPACK_PACKAGE_NAME "PasswordManager")
set(CPACK_PACKAGE_VENDOR "ivlad20")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local encrypted password manager")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Password Manager")   
set(CPACK_INNOSETUP_ARCHITECTURE "x64")                    # 64-bit only, installs to Program Files (not x86)
set(CPACK_PACKAGE_EXECUTABLES "PasswordManagerGui" "Password Manager")
set(CPACK_CREATE_DESKTOP_LINKS "PasswordManagerGui")
include(CPack)
