find_package(Qt6 REQUIRED COMPONENTS Test WebSockets)

qt_standard_project_setup()

set(CLIENT_TESTS protocol session_state network_client game_controller player_list_widget ui_theme e2e)

foreach (tst ${CLIENT_TESTS})
    qt_add_executable(client_test_${tst} test_${tst}.cpp)
    target_link_libraries(client_test_${tst} PRIVATE client_lib Qt6::Test Qt6::WebSockets)
    add_test(NAME client_${tst} COMMAND client_test_${tst})
endforeach ()

# The E2E test spawns the Server binary; make sure it's built first.
add_dependencies(client_test_e2e Server)
