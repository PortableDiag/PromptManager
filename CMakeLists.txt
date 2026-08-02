cmake_minimum_required(VERSION 3.16)
project(PromptManager VERSION 2.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    main.cpp
    mainwindow.cpp
    foldertreemodel.cpp
    foldersortfilterproxymodel.cpp
    apiserver.cpp
)

set(RESOURCES
    resources/resources.qrc
)

set(HEADERS
    mainwindow.h
    foldertreeitem.h
    foldertreemodel.h
    foldersortfilterproxymodel.h
    apiserver.h
)

add_executable(PromptManager ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(PromptManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

install(TARGETS PromptManager DESTINATION bin)

if(UNIX AND NOT APPLE)
    install(FILES resources/promptmanager.desktop
            DESTINATION share/applications)
    foreach(SIZE 16 24 32 48 64 128 256 512)
        install(FILES resources/icons/app-${SIZE}.png
                DESTINATION share/icons/hicolor/${SIZE}x${SIZE}/apps
                RENAME promptmanager.png)
    endforeach()
endif()
