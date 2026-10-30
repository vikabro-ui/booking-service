cmake_minimum_required(VERSION 3.14)
project(project1 CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(cpp-httplib)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    pqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 7.9.2
)

set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pqxx)

add_executable(booking_service main.cpp)

target_compile_definitions(booking_service PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(booking_service PRIVATE 
    httplib::httplib 
    nlohmann_json::nlohmann_json 
    pqxx 
    OpenSSL::SSL 
    OpenSSL::Crypto
)