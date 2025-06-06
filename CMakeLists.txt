# Test-specific CMakeLists.txt for LED and Wire functionality
cmake_minimum_required(VERSION 3.16)

project(LEDWireTest VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug info for debug builds
set(CMAKE_BUILD_TYPE Debug)

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

# Enable automatic MOC, UIC, and RCC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Source files for the test
set(CORE_SOURCES
    src/core/Component.cpp
    src/core/ElectricalComponent.cpp
    src/core/LED.cpp
    src/core/Resistor.cpp
    src/core/Wire.cpp
    src/core/ArduinoPin.cpp
    src/core/Arduino.cpp
)

set(SIMULATION_SOURCES
    src/simulation/Circuit.cpp
    src/simulation/Node.cpp
    src/simulation/CircuitSimulator.cpp
    src/simulation/MatrixSolver.cpp
)

set(UI_SOURCES
    src/ui/ComponentGraphicsItem.cpp
    src/ui/LEDGraphicsItem.cpp
    src/ui/WireGraphicsItem.cpp
    src/ui/CircuitCanvas.cpp
    src/ui/ArduinoGraphicsItem.cpp
)

# Header files (for MOC processing)
set(HEADERS
    include/core/Component.h
    include/core/ElectricalComponent.h
    include/core/LED.h
    include/core/Resistor.h
    include/core/Wire.h
    include/core/ArduinoPin.h
    include/core/Arduino.h
    include/simulation/Circuit.h
    include/simulation/Node.h
    include/simulation/CircuitSimulator.h
    include/simulation/MatrixSolver.h
    include/ui/ComponentGraphicsItem.h
    include/ui/LEDGraphicsItem.h
    include/ui/WireGraphicsItem.h
    include/ui/CircuitCanvas.h
    include/ui/ArduinoGraphicsItem.h
)

# Create test executable
add_executable(LEDWireTest
    src/test_led_wire_main.cpp
    ${CORE_SOURCES}
    ${SIMULATION_SOURCES}
    ${UI_SOURCES}
    ${HEADERS}
)

# Link Qt5 libraries
target_link_libraries(LEDWireTest
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
)

# Compiler-specific options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(LEDWireTest PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Print configuration summary
message(STATUS "=== LED Wire Test Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Qt5 version: ${Qt5_VERSION}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "=======================================")