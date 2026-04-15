include(FetchContent)

find_package(nlohmann_json 3.11.3 QUIET CONFIG)

if (nlohmann_json_FOUND)
    message(STATUS "nlohmann_json found in the system.")
else ()
    message(STATUS "nlohmann_json not found. Downloading release archive...")
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(nlohmann_json)
endif ()
