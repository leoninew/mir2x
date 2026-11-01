# The launcher is an independent build product and install component.
option(MIR2X_BUILD_RUNTIME "Build the portable Python/MCP runtime launcher" ${WIN32})
if(NOT MIR2X_BUILD_RUNTIME)
    return()
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Portable runtime requires Windows; build the Python wheel on POSIX")
endif()
find_program(MIR2X_UV_EXECUTABLE NAMES uv REQUIRED)
get_filename_component(MIR2X_RUNTIME_SOURCE "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(MIR2X_RUNTIME_OUTPUT "${CMAKE_BINARY_DIR}/runtime-launcher" CACHE PATH
    "Output directory for portable runtime artifacts")
file(GLOB_RECURSE MIR2X_RUNTIME_INPUTS CONFIGURE_DEPENDS
    "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/src/*.py"
    "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/src/*.json"
    "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/src/*.toml"
    "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/src/*.md"
    "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/scripts/*.py")
add_custom_command(
    OUTPUT "${MIR2X_RUNTIME_OUTPUT}/stage/runtime-manifest.json"
    BYPRODUCTS "${MIR2X_RUNTIME_OUTPUT}/mir2x.exe"
    COMMAND "${MIR2X_PYTHON_EXECUTABLE}"
            "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/scripts/build_runtime.py"
            --uv "${MIR2X_UV_EXECUTABLE}" --output "${MIR2X_RUNTIME_OUTPUT}"
    DEPENDS ${MIR2X_RUNTIME_INPUTS}
            "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/pyproject.toml"
            "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/uv.lock"
            "${MIR2X_RUNTIME_SOURCE}/tools/mir2x-mcp/README.md"
            "${MIR2X_RUNTIME_SOURCE}/mir2x.cmd"
            "${MIR2X_RUNTIME_SOURCE}/.agents/skills/mir2x-runtime/SKILL.md"
    WORKING_DIRECTORY "${MIR2X_RUNTIME_SOURCE}"
    VERBATIM USES_TERMINAL)
add_custom_target(mir2x_runtime ALL
    DEPENDS "${MIR2X_RUNTIME_OUTPUT}/stage/runtime-manifest.json")
if(TARGET client)
    add_dependencies(client mir2x_runtime)
endif()
install(DIRECTORY "${MIR2X_RUNTIME_OUTPUT}/stage/" DESTINATION "." COMPONENT runtime)
