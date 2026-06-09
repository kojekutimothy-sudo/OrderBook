cmake_minimum_required(VERSION 3.20)
project(OrderBook)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(OrderBook
    core/OrderPool.cpp
    main.cpp
)

if(WIN32)
    target_link_options(OrderBook PRIVATE -mconsole)
endif()