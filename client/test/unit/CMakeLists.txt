# AI-generated unit tests for individual client/src classes/submodules. each test is its
# own standalone executable (see mir2x_add_unit_test() in cmake/Mir2xTest.cmake), reusing
# client_objs (the client build minus main.cpp) so it exercises the exact same compiled
# code as the real client without recompiling it.
#
# these targets are EXCLUDE_FROM_ALL: they do not slow down normal "build everything" runs.
# build+run all registered tests project-wide via `cmake --build <builddir> --target check`
# (from the top of the build tree), or build/run a single one explicitly, e.g.:
#
#   cmake --build <builddir> --target test_xmltypeset
#   ctest --test-dir <builddir> -R '^xmltypeset/' --output-on-failure
#
# unit tests that need the packed client resources (fontex.zsdb / emoji.zsdb) require
# `cmake --install <builddir>` to have been run at least once; they default to reading
# from ${CMAKE_INSTALL_PREFIX}/client/res; override with MIR2X_TEST_RES_DIR.

mir2x_add_unit_test(
    NAME xmltypeset
    SOURCES test_xmltypeset.cpp
    LIBS client_objs
    DEPENDS zsdbmaker mir2x_resources
)
target_compile_definitions(test_xmltypeset PRIVATE
    MIR2X_TEST_DEFAULT_RES_DIR="${CMAKE_INSTALL_PREFIX}/client/res")

mir2x_add_unit_test(
    NAME netio
    SOURCES test_netio.cpp ../../src/netio.cpp
    LIBS common
)
target_include_directories(test_netio PRIVATE ../../src)

mir2x_add_unit_test(
    NAME widget
    SOURCES test_widget.cpp ../../src/widget.cpp
    LIBS common mir2x::sdl3_ext
)
target_include_directories(test_widget PRIVATE ../../src)

mir2x_add_unit_test(
    NAME ai_bridge_protocol
    SOURCES test_ai_bridge_protocol.cpp
    LIBS mir2x_ai_bridge
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(test_ai_bridge_protocol PRIVATE
    MIR2X_AI_CONTRACT_DIR="${CMAKE_SOURCE_DIR}/contracts/ai-bridge/v1"
    MIR2X_AI_TEST_DIR="${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge")

mir2x_add_unit_test(
    NAME ai_bridge_security
    SOURCES test_ai_bridge_security.cpp
    LIBS mir2x_ai_bridge
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(test_ai_bridge_security PRIVATE
    MIR2X_TEST_WORKSPACE_ROOT="${CMAKE_SOURCE_DIR}"
    MIR2X_AI_TEST_DIR="${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge")

mir2x_add_unit_test(NAME response_callbacks SOURCES test_response_callbacks.cpp)

mir2x_add_unit_test(
    NAME ai_bridge_controller
    SOURCES test_ai_bridge_controller.cpp
    LIBS mir2x_ai_bridge SDL3::SDL3
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(test_ai_bridge_controller PRIVATE
    MIR2X_AI_TEST_DIR="${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge")

mir2x_add_unit_test(
    NAME ai_bridge_runtime
    SOURCES test_ai_bridge_runtime.cpp
    LIBS mir2x_ai_bridge SDL3::SDL3
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

mir2x_add_unit_test(
    NAME ai_bridge_actions
    SOURCES test_ai_bridge_actions.cpp
    LIBS mir2x_ai_bridge common
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

mir2x_add_unit_test(
    NAME ai_bridge_npc
    SOURCES test_ai_bridge_npc.cpp
    LIBS mir2x_ai_bridge
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/npc-gui")
add_executable(test_ai_bridge_npc_gui EXCLUDE_FROM_ALL test_ai_bridge_npc_gui.cpp)
target_link_libraries(test_ai_bridge_npc_gui PRIVATE Catch2::Catch2WithMain client_objs)
add_dependencies(test_ai_bridge_npc_gui zsdbmaker mir2x_resources)
catch_discover_tests(test_ai_bridge_npc_gui
    TEST_PREFIX "ai_bridge_npc_gui/"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/npc-gui"
    PROPERTIES TIMEOUT 600 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
)
set_property(GLOBAL APPEND PROPERTY MIR2X_TEST_BUILD_TARGETS test_ai_bridge_npc_gui)
target_compile_definitions(test_ai_bridge_npc_gui PRIVATE
    MIR2X_TEST_DEFAULT_RES_DIR="${CMAKE_INSTALL_PREFIX}/client/res")

mir2x_add_unit_test(NAME ai_bridge_economy SOURCES test_ai_bridge_economy.cpp LIBS mir2x_ai_bridge WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
mir2x_add_unit_test(NAME ai_bridge_shop SOURCES test_ai_bridge_shop.cpp LIBS mir2x_ai_bridge WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/trade-gui")
add_executable(test_ai_bridge_trade_gui EXCLUDE_FROM_ALL test_ai_bridge_trade_gui.cpp)
target_link_libraries(test_ai_bridge_trade_gui PRIVATE Catch2::Catch2WithMain client_objs)
add_dependencies(test_ai_bridge_trade_gui zsdbmaker mir2x_resources)
catch_discover_tests(test_ai_bridge_trade_gui TEST_PREFIX "ai_bridge_trade_gui/"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/trade-gui"
    PROPERTIES TIMEOUT 600 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
set_property(GLOBAL APPEND PROPERTY MIR2X_TEST_BUILD_TARGETS test_ai_bridge_trade_gui)
target_compile_definitions(test_ai_bridge_trade_gui PRIVATE MIR2X_TEST_DEFAULT_RES_DIR="${CMAKE_INSTALL_PREFIX}/client/res")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/economy-gui")
add_executable(test_ai_bridge_economy_gui EXCLUDE_FROM_ALL test_ai_bridge_economy_gui.cpp)
target_link_libraries(test_ai_bridge_economy_gui PRIVATE Catch2::Catch2WithMain client_objs)
add_dependencies(test_ai_bridge_economy_gui zsdbmaker mir2x_resources)
catch_discover_tests(test_ai_bridge_economy_gui TEST_PREFIX "ai_bridge_economy_gui/"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/economy-gui"
    PROPERTIES TIMEOUT 600 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
set_property(GLOBAL APPEND PROPERTY MIR2X_TEST_BUILD_TARGETS test_ai_bridge_economy_gui)
target_compile_definitions(test_ai_bridge_economy_gui PRIVATE MIR2X_TEST_DEFAULT_RES_DIR="${CMAKE_INSTALL_PREFIX}/client/res")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/shop-gui")
add_executable(test_ai_bridge_shop_gui EXCLUDE_FROM_ALL test_ai_bridge_shop_gui.cpp)
target_link_libraries(test_ai_bridge_shop_gui PRIVATE Catch2::Catch2WithMain client_objs)
add_dependencies(test_ai_bridge_shop_gui zsdbmaker mir2x_resources)
catch_discover_tests(test_ai_bridge_shop_gui TEST_PREFIX "ai_bridge_shop_gui/"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.runtime/test-ai-bridge/shop-gui"
    PROPERTIES TIMEOUT 600 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
set_property(GLOBAL APPEND PROPERTY MIR2X_TEST_BUILD_TARGETS test_ai_bridge_shop_gui)
target_compile_definitions(test_ai_bridge_shop_gui PRIVATE MIR2X_TEST_DEFAULT_RES_DIR="${CMAKE_INSTALL_PREFIX}/client/res")
