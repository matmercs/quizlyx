set(SERVER_DOMAIN_SOURCES
    app/command_handler.cpp
    domain/quiz.cpp
    domain/session.cpp
    services/in_memory_quiz_storage.cpp
    services/quiz_registry.cpp
    services/scoring.cpp
    services/session_manager.cpp
    services/session_timer_service.cpp
)

add_library(quizlyx_server_lib STATIC ${SERVER_DOMAIN_SOURCES})
target_include_directories(quizlyx_server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/app ${CMAKE_CURRENT_SOURCE_DIR}/domain
        ${CMAKE_CURRENT_SOURCE_DIR}/events ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/services)

if (WIN32)
    add_executable(Server WIN32 main.cpp)
else ()
    add_executable(Server main.cpp)
endif ()
target_link_libraries(Server PRIVATE quizlyx_server_lib)

add_executable(ServerDemo demo/main.cpp)
target_link_libraries(ServerDemo PRIVATE quizlyx_server_lib)
