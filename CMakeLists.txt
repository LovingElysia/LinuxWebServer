cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_BUILD_TYPE Debug)

project(Webserver)

set(CMAKE_CXX_STANDARD 17)


add_executable(Webserver
        CGImysql/connection_pool.cpp
        lock/locker.cpp
        http_conn/http_conn.cpp
        log/log.cpp
        timer/time_heap.cpp
        config.cpp
        webserver.cpp
        main.cpp)

target_include_directories(Webserver PRIVATE /usr/include/mysql)

target_link_libraries(Webserver mysqlclient)

target_link_libraries(Webserver pthread)

