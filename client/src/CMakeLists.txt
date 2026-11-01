set(CLIENT_MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
mir2x_list_source_recursive_filtered(CLIENT_OBJ_SRCS
    EXCLUDE ${CLIENT_MAIN_SRC})
list(FILTER CLIENT_OBJ_SRCS EXCLUDE REGEX "/aibridge/(protocol|transport|instancefile|policy|controller|operation|events|audit|snapshot|movement|actions|npcmodel|economy|shopmodel)\\.cpp$")

find_package(nlohmann_json CONFIG REQUIRED)
set(AI_BRIDGE_CONTRACT "${CMAKE_SOURCE_DIR}/contracts/ai-bridge/v1/contract.schema.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${AI_BRIDGE_CONTRACT}")
file(READ "${AI_BRIDGE_CONTRACT}" AI_BRIDGE_CONTRACT_TEXT)
configure_file(aibridge/contract.hpp.in generated/aibridge_contract.hpp @ONLY)
add_library(mir2x_ai_bridge STATIC
    aibridge/protocol.cpp aibridge/transport.cpp
    aibridge/instancefile.cpp aibridge/policy.cpp
    aibridge/controller.cpp aibridge/operation.cpp aibridge/events.cpp aibridge/audit.cpp
    aibridge/snapshot.cpp aibridge/movement.cpp aibridge/actions.cpp aibridge/npcmodel.cpp aibridge/economy.cpp aibridge/shopmodel.cpp)
target_include_directories(mir2x_ai_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(mir2x_ai_bridge PUBLIC nlohmann_json::nlohmann_json asio::asio Threads::Threads PRIVATE utf8::cpp tinyxml2::tinyxml2)
if(WIN32)
    target_link_libraries(mir2x_ai_bridge PRIVATE bcrypt advapi32 ws2_32 mswsock)
endif()

# client_objs contains the client implementation without main.cpp so unit tests can reuse it.
add_library(client_objs OBJECT ${CLIENT_OBJ_SRCS})
add_dependencies(client_objs zsdbmaker mir2x_resources)
target_include_directories(client_objs PUBLIC ${MIR2X_COMMON_SOURCE_DIR})
target_include_directories(client_objs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(client_objs PUBLIC
    common
    mir2x_ai_bridge
    mir2x::sdl3_ext
    mir2x::libpinyin)

add_executable(client ${CLIENT_MAIN_SRC})
target_link_libraries(client PRIVATE client_objs)

install(TARGETS client DESTINATION client)
if(MIR2X_LIBPINYIN_DATA_DIR)
    install(DIRECTORY ${MIR2X_LIBPINYIN_DATA_DIR} DESTINATION client)
endif()
install(CODE "execute_process(
    COMMAND ${MIR2X_PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/installres/src/install_res.py
            --install-client
            --ignore-zsdbmaker-error
            --zsdbmaker=$<TARGET_FILE:zsdbmaker>
            --input-dir=${MIR2X_RES_REPO_PATH}
            --output-dir=${CMAKE_INSTALL_PREFIX}/client/res
    COMMAND_ERROR_IS_FATAL ANY
)")
