cmake_minimum_required(VERSION 3.16)

project(ArduinoSimulator VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug info for debug builds
set(CMAKE_BUILD_TYPE Debug)

# Find Qt5 with fallback mechanisms
find_package(Qt5 QUIET COMPONENTS Core Widgets Gui Concurrent OpenGL)

# If Qt5 not found via find_package, try manual search
if(NOT Qt5_FOUND)
    # Common Qt5 installation paths
    set(QT5_SEARCH_PATHS
        /usr/lib/x86_64-linux-gnu/cmake
        /usr/lib/cmake
        /usr/share/qt5
        /opt/qt5
        $ENV{QT5_DIR}
        $ENV{QTDIR}
    )
    
    foreach(SEARCH_PATH ${QT5_SEARCH_PATHS})
        if(EXISTS "${SEARCH_PATH}/Qt5/Qt5Config.cmake")
            set(CMAKE_PREFIX_PATH "${SEARCH_PATH}" ${CMAKE_PREFIX_PATH})
            break()
        endif()
    endforeach()
    
    # Try again with updated prefix path
    find_package(Qt5 QUIET COMPONENTS Core Widgets Gui Concurrent OpenGL)
endif()

# Final check - error if still not found
if(NOT Qt5_FOUND)
    message(FATAL_ERROR "Qt5 not found! Please install Qt5 development packages or set QT5_DIR environment variable.")
endif()

# Find Eigen3 for matrix operations
find_package(Eigen3 QUIET)
if(NOT Eigen3_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(EIGEN3 eigen3)
        if(EIGEN3_FOUND)
            set(EIGEN3_INCLUDE_DIRS ${EIGEN3_INCLUDE_DIRS})
        endif()
    endif()
endif()

# Enable automatic MOC, UIC, and RCC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Add Eigen include if found
if(Eigen3_FOUND)
    include_directories(${EIGEN3_INCLUDE_DIR})
    add_definitions(-DHAVE_EIGEN3)
elseif(EIGEN3_FOUND)
    include_directories(${EIGEN3_INCLUDE_DIRS})
    add_definitions(-DHAVE_EIGEN3)
else()
    message(WARNING "Eigen3 not found. Matrix operations will use fallback implementation.")
endif()

# Source files
set(CORE_SOURCES
    src/core/Component.cpp
    src/core/ElectricalComponent.cpp
    src/core/Resistor.cpp
    src/core/LED.cpp
    src/core/Switch.cpp
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
    src/ui/MainWindow.cpp
    src/ui/CircuitCanvas.cpp
    src/ui/ComponentGraphicsItem.cpp
    src/ui/LEDGraphicsItem.cpp
    src/ui/SwitchGraphicsItem.cpp
    src/ui/ResistorGraphicsItem.cpp
    src/ui/WireGraphicsItem.cpp
    src/ui/ArduinoGraphicsItem.cpp
    src/ui/ComponentLibrary.cpp
)

# Header files (for MOC processing)
set(HEADERS
    include/core/Component.h
    include/core/ElectricalComponent.h
    include/core/Resistor.h
    include/core/LED.h
    include/core/Switch.h
    include/core/Wire.h
    include/core/ArduinoPin.h
    include/core/Arduino.h
    include/simulation/Circuit.h
    include/simulation/Node.h
    include/simulation/CircuitSimulator.h
    include/simulation/MatrixSolver.h
    include/ui/MainWindow.h
    include/ui/CircuitCanvas.h
    include/ui/ComponentGraphicsItem.h
    include/ui/LEDGraphicsItem.h
    include/ui/SwitchGraphicsItem.h
    include/ui/ResistorGraphicsItem.h
    include/ui/WireGraphicsItem.h
    include/ui/ArduinoGraphicsItem.h
    include/ui/ComponentLibrary.h
)

# Resource files (if any)
set(RESOURCES
    resources/resources.qrc
)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${CORE_SOURCES}
    ${SIMULATION_SOURCES}
    ${UI_SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# Link Qt5 libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Concurrent
)

# Link OpenGL if available
if(Qt5OpenGL_FOUND)
    target_link_libraries(${PROJECT_NAME} Qt5::OpenGL)
    add_definitions(-DHAVE_OPENGL)
endif()

# Compiler-specific options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set output directories
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "=== Arduino Simulator Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Qt5 version: ${Qt5_VERSION}")
message(STATUS "Qt5 dir: ${Qt5_DIR}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

if(Eigen3_FOUND OR EIGEN3_FOUND)
    message(STATUS "Eigen3: Found")
else()
    message(STATUS "Eigen3: Not found (using fallback)")
endif()

if(Qt5OpenGL_FOUND)
    message(STATUS "OpenGL: Enabled")
else()
    message(STATUS "OpenGL: Disabled")
endif()
message(STATUS "=======================================")
