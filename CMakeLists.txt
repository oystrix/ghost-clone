cmake_minimum_required(VERSION 3.21)

# Проект собирается со стандартом C++20, как того требует Geode SDK
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Архитектуры для macOS/iOS-сборок
if (NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    else()
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    endif()
endif()

project(GhostClone VERSION 1.0.0)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Не найден Geode SDK! Установите переменную окружения GEODE_SDK, "
                         "либо выполните 'geode sdk install'.")
else()
    message(STATUS "Найден Geode SDK: $ENV{GEODE_SDK}")
endif()

# ВАЖНО: именно add_subdirectory, а не include() — иначе CMake будет искать
# служебные файлы SDK (VERSION, cmake/CPM.cmake) в неправильном месте
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Автоматически подхватываем все .cpp файлы из src/
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

setup_geode_mod(${PROJECT_NAME})
