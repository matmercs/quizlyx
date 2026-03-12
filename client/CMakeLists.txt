find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Boost REQUIRED)

if (WIN32)
    add_executable(Client WIN32
            main.cpp
    )
else ()
    add_executable(Client
            main.cpp
    )
endif ()

target_link_libraries(Client PRIVATE qt boost)
