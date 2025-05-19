# Arduino Simulator

A comprehensive Arduino microcontroller simulator built with Qt and C++.

## Features

- Circuit design with drag-and-drop components
- Real-time electrical simulation
- Arduino pin configuration and monitoring
- Component library (resistors, LEDs, switches, etc.)
- Wire connections with automatic routing

## Building

### Prerequisites

- Qt 5.15 or later
- CMake 3.16+
- C++17 compatible compiler
- Eigen3 (optional, for advanced matrix operations)

### Build Instructions

```bash
cd arduino-simulator
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running

```bash
./bin/ArduinoSimulator
```

## Project Structure

```
arduino-simulator/
├── src/
│   ├── core/           # Component models (LED, Resistor, etc.)
│   ├── simulation/     # Circuit simulation engine
│   └── ui/             # Qt GUI components
├── include/            # Header files
├── resources/          # Qt resources (icons, etc.)
└── tests/              # Unit tests
```

## Development Status

This project is in active development. Current status:

- [x] Basic project structure
- [x] Core component classes
- [ ] Circuit simulation engine
- [ ] Qt UI implementation
- [ ] Arduino model
- [ ] Component graphics
- [ ] Wire routing
- [ ] Save/load functionality

## Contributing

Contributions are welcome! Please read the contributing guidelines before submitting pull requests.

## License

This project is open source. License details TBD.
