cmake_minimum_required(VERSION 3.28)
project(SvetaAssistant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "Sveta Assistant targets Windows only.")
endif()

add_subdirectory(src)
