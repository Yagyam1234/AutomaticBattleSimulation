Automatic Battle Simulation
Project Overview
This project consists of:

An Unreal Engine 4 client for visualizing the battle simulation.
A C++ server that handles the simulation logic and networking.
Unreal Engine Client
Developed using Unreal Engine 4.
The client connects to the server and updates the battle simulation in real-time.
Located in the UnrealProject/ folder.

Running the Unreal Client
Open Unreal Engine 4.
Navigate to the UnrealProject/ folder.
Open the .uproject file.
Click Play to start the client.

C++ Server
A standalone C++ application that runs the battle simulation and communicates with the Unreal client.
Uses CMake to generate the necessary build files.
Located in the SimulationServer/ folder.

Building & Running the Server
Generate build files using CMake:

cd SimulationServer
cmake -S . -B build -G "Visual Studio 17 2022"
Compile the server:

cmake --build build
Run the server:

cd build
./SimulationServer