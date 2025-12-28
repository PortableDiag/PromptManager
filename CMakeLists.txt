cmake_minimum_required(VERSION 3.16)
project(PromptSnippetManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    main.cpp
    mainwindow.cpp
    foldertreemodel.cpp
)

set(HEADERS
    mainwindow.h
    foldertreeitem.h
    foldertreemodel.h
)

add_executable(PromptSnippetManager ${SOURCES} ${HEADERS})

target_link_libraries(PromptSnippetManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS PromptSnippetManager DESTINATION bin)
