# DeepFive

DeepFive is a modern C++ implementation of the classic board game Gomoku (Five-in-a-Row), built using the **Bobcat UI** library and **OpenGL** for rendering.

## Features

- **Graphical Interface**: Smooth 2D board rendering using hardware-accelerated OpenGL via `bobcat::Canvas_`.
- **Game Modes**:
  - **Human vs Bot**: Play against an AI opponent.
  - **Human vs Human**: Hotseat multiplayer.
- **AI Engine**:
  - Configurable difficulty (Easy, Normal, Hard).
  - Uses heuristic evaluation and local search (Minimax with Alpha-Beta pruning).
- **Controls**:
  - Undo support.
  - Configurable Bot side (White/Black).
  - Real-time status updates.

## Architecture

The project follows a clean, modular architecture:

- **src/board**: Core grid representation and efficient winner detection.
- **src/game**: Game flow orchestration, history tracking, and rule enforcement.
- **src/bot**: AI logic with heuristic scoring and minimax search.
- **src/gomoku_canvas**: Custom UI widget inheriting from `bobcat::Canvas_` for OpenGL rendering.
- **src/main**: Application entry point and UI wiring.

## Building

Requirements:
- C++17 compatible compiler
- CMake 3.10+
- FLTK library
- OpenGL

```bash
mkdir build
cd build
cmake ..
make
./DeepFive
```

## Usage

- **Left Click**: Place a stone.
- **New Game**: Reset the board and apply current settings.
- **Undo**: Revert the last move (or pair of moves in Human vs Bot).
- **Mode**: Switch between Player vs Player and Player vs Bot.
- **Difficulty**: Adjust the Bot's strength.

## License

This project is provided as-is for educational purposes.
