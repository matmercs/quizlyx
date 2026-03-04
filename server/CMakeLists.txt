set(SERVER_DOMAIN_SOURCES
    domain/quiz.cpp
    domain/session.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/interfaces)
