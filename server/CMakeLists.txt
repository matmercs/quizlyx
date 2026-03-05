set(SERVER_DOMAIN_SOURCES
    domain/quiz.cpp
    domain/session.cpp
    services/in_memory_quiz_storage.cpp
    services/quiz_registry.cpp
    services/scoring.cpp
    services/session_manager.cpp
    services/session_timer_service.cpp
)

if (WIN32)
    add_executable(Server WIN32
            main.cpp
            ${SERVER_DOMAIN_SOURCES}
    )
else ()
    add_executable(Server
            main.cpp
            ${SERVER_DOMAIN_SOURCES}
    )
endif ()

target_include_directories(Server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/domain
        ${CMAKE_CURRENT_SOURCE_DIR}/events ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/services)
