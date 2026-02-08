find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Boost REQUIRED)

if (WIN32)
    add_executable(Server WIN32
            main.cpp
    )
else ()
    add_executable(Server
            main.cpp
    )
endif ()

target_link_libraries(Server PRIVATE qt boost)
