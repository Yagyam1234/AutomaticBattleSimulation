cmake_minimum_required(VERSION 3.10)

# Set project name
project(SimulationServer)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Specify source files explicitly
set(SOURCES
    SimulationServer.cpp  
    GameConfig.h
    NetworkManager.h
    NetworkManager.cpp
    SimulationManager.cpp
    SimulationManager.h
    Ball.cpp
    Ball.h
    # Add other necessary .cpp files, but NOT extra main() files!
)

# Define the executable
add_executable(SimulationServer ${SOURCES})
