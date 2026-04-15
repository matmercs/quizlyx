find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)

qt_standard_project_setup()

set(CLIENT_SOURCES
        app/Application.cpp
        model/Quiz.cpp
        model/SessionState.cpp
        model/Protocol.cpp
        network/NetworkClient.cpp
        controller/GameController.cpp
        ui/MainWindow.cpp
        ui/Theme.cpp
        ui/pages/ConnectPage.cpp
        ui/pages/RoleSelectPage.cpp
        ui/pages/QuizEditorPage.cpp
        ui/pages/HostLobbyPage.cpp
        ui/pages/JoinPage.cpp
        ui/pages/PlayerLobbyPage.cpp
        ui/pages/QuestionPage.cpp
        ui/pages/LeaderboardPage.cpp
        ui/pages/FinalPage.cpp
        ui/widgets/AnswerButton.cpp
        ui/widgets/CountdownBar.cpp
        ui/widgets/PlayerListWidget.cpp
        ui/widgets/LeaderboardWidget.cpp
)

qt_add_library(client_lib STATIC ${CLIENT_SOURCES})
target_include_directories(client_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client_lib PUBLIC
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets)

qt_add_resources(client_lib "client_resources"
        PREFIX "/client"
        FILES
        resources/style.qss
        resources/chevron-down.svg
        resources/chevron-up.svg)

if (WIN32)
    qt_add_executable(Client WIN32 main.cpp)
else ()
    qt_add_executable(Client main.cpp)
endif ()
target_link_libraries(Client PRIVATE client_lib)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
