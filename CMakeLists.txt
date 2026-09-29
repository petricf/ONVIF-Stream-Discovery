cmake_minimum_required(VERSION 3.16)

project(ONVIFStreamQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets Xml)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(ONVIFStreamQt
    main.cpp
    OnvifClient.cpp
    OnvifClient.h
    OnvifModels.h
    WsSecurity.cpp
    WsSecurity.h
    MainWindow.cpp
    MainWindow.h
    StreamInfo.h
)

target_link_libraries(ONVIFStreamQt PRIVATE Qt6::Core Qt6::Network Qt6::Widgets Qt6::Xml)