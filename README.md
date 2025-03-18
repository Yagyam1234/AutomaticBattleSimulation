Automatic Battle Simulation
Project Overview
This project consists of:

An Unreal Engine 5 client for visualizing the battle simulation.

A C++ server that handles the simulation logic and networking.
Unreal Engine Client
Developed using Unreal Engine 5.

The client connects to the server and updates the battle simulation in real time.

Located in the UnrealSimulation/ folder.

Running the Unreal Client

Open Unreal Engine 5.

Navigate to the UnrealSimulation/ folder.

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

How to Play

1. Start the Simulation Server
Follow the Building & Running the Server steps above to compile and run the server.
The server must be running before starting the Unreal Engine client.

2. Launch the Unreal Engine Client
Open Unreal Editor and load the UnrealSimulation/ project.
Click Play to start the simulation.

3. Watch the Battle Simulation
The Unreal client will automatically connect to the server.
The battle will unfold in real time based on the server’s logic.
