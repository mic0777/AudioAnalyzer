cmake_minimum_required(VERSION 3.5)
project(AudioAnalyzer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include(CTest)
enable_testing()

set(PROJECT_SOURCES
    main.cpp
    worker.h
    worker.cpp
    decodeAudio.cpp
)

add_executable(AudioAnalyzer ${PROJECT_SOURCES})

find_library(AVCODEC_LIBRARY avcodec)

find_library(AVFORMAT_LIBRARY avformat)

find_library(AVUTIL_LIBRARY avutil)

find_library(AVDEVICE_LIBRARY avdevice)

find_library(AUBIO_LIBRARY aubio)

include_directories(${CMAKE_SOURCE_DIR}/include)

target_compile_features(AudioAnalyzer PRIVATE cxx_std_17)
target_link_libraries(AudioAnalyzer keyfinder aubio 
        ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} 
stdc++fs)
#target_include_directories(decode_encode PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)

