if(WIN32)
    set(MIR2X_WORKBENCH_PACKAGE_DIR "${CMAKE_BINARY_DIR}/workbench/Mir2xWorkbench"
        CACHE PATH "Prepared Windows workbench package")

    add_custom_target(workbench
        COMMAND "${MIR2X_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_desktop.py"
            --build-dir "${CMAKE_BINARY_DIR}" --no-install
        USES_TERMINAL VERBATIM)

    install(CODE "
        if(NOT EXISTS \"${MIR2X_WORKBENCH_PACKAGE_DIR}/Mir2xWorkbench.exe\")
            if(CMAKE_INSTALL_COMPONENT STREQUAL \"workbench\")
                message(FATAL_ERROR \"Workbench package missing. Run make binary in tools/gameplay-workbench first.\")
            endif()
            message(STATUS \"Workbench not built; skipping optional workbench installation\")
        endif()
    " COMPONENT workbench)
    install(DIRECTORY "${MIR2X_WORKBENCH_PACKAGE_DIR}/" DESTINATION workbench
        COMPONENT workbench OPTIONAL
        PATTERN "logs" EXCLUDE
        PATTERN "game-launch*" EXCLUDE
        PATTERN ".env*" EXCLUDE)
endif()
