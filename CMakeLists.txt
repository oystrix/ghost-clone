cmake_minimum_required(VERSION 3.21)

# Проект собирается со стандартом C++20, как того требует Geode SDK
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(GhostClone VERSION 1.0.0)

# Автоматически подхватываем все .cpp файлы из src/
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Не найден Geode SDK! Установите переменную окружения GEODE_SDK, "
                         "либо выполните 'geode sdk install'.")
else()
    message(STATUS "Найден Geode SDK: $ENV{GEODE_SDK}")
endif()

include($ENV{GEODE_SDK}/CMakeLists.txt)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

setup_geode_mod(${PROJECT_NAME})
