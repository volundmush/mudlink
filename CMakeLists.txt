cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 23)

set(CPM_DOWNLOAD_VERSION 0.36.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake")
    file(DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})

project(mudlink)

CPMAddPackage(
        NAME json
        GITHUB_REPOSITORY nlohmann/json
        VERSION 3.11.2
)

CPMAddPackage(
        NAME mudtelnet
        GITHUB_REPOSITORY volundmush/mudtelnet
        GIT_TAG main
)

CPMAddPackage(
        NAME entt
        GITHUB_REPOSITORY skypjack/entt
        VERSION 3.10.3
)

find_package(Boost REQUIRED COMPONENTS program_options regex)

set(MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MAIN_PROJECT ON)
endif()


file(GLOB MUDLINK_INCLUDE include/mudlink/*.h)
file(GLOB MUDLINK_SRC src/*.cpp)

add_library(mudlink ${MUDLINK_INCLUDE} ${MUDLINK_SRC})
link_libraries(mudlink pthread ${Boost_LIBRARIES} mudtelnet)

include_directories(PUBLIC include
        ${BOOST_LIBRARY_INCLUDES}
        ${json_SOURCE_DIR}/single_include
        ${mudtelnet_SOURCE_DIR}/include
        ${entt_SOURCE_DIR}/single_include
        )

if(${MAIN_PROJECT})
    add_executable(mudlink_test apps/mudlink_test.cpp)
endif()