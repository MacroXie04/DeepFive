# DeepFive

DeepFive is a high-performance, modern C++ implementation of the classic board game **Gomoku** (Five-in-a-Row). It features a robust AI engine combining **Monte Carlo Tree Search (MCTS)** with **VCF (Victory by Continuous Four)** solvers, wrapped in a clean UI rendered with OpenGL and FLTK.

## Features

### Intelligent AI
- **Hybrid Engine**: Combines MCTS for strategic planning with specialized IDDFS (Iterative Deepening DFS) for tactical kill calculations (VCF/VCF-Block).
- **Multiple Difficulty Modes**:
  - **Instant**: Quick responses for casual play.
  - **Thinking**: Balanced depth and speed (default).
  - **Pro**: Deep analysis for challenging the strongest opponents.
  - **Auto**: Dynamically adjusts time budget based on game stage.
- **VCF Solver**: Capable of seeing 25+ steps ahead for forced wins, ensuring the bot never misses a kill or a forced defense.

### Modern UI
- **OpenGL Rendering**: Smooth, hardware-accelerated graphics for the board and stones.
- **Real-time Analysis**: Displays win rates, simulation counts, and search progress while the bot thinks.
- **Responsive Design**: Built with FLTK for cross-platform compatibility.

## Building from Source

### Prerequisites
- **C++ Compiler**: C++17 compatible (GCC, Clang, MSVC).
- **CMake**: Version 3.10 or higher.
- **FLTK**: Fast Light Toolkit (1.3 or 1.4).
- **OpenGL**: Usually comes with your OS or graphics drivers.

### Build Steps

1. **Clone the repository**:
   ```bash
   git clone https://github.com/MacroXie04/DeepFive.git
   cd DeepFive
   ```

2. **Create a build directory**:
   ```bash
   mkdir build
   cd build
   ```

3. **Configure and Build**:
   ```bash
   cmake ..
   make
   ```

4. **Run the game**:
   ```bash
   ./DeepFive
   ```

## Project Structure

- `src/core/`: Core game logic and board representation.
- `src/bot/`: The MCTS engine and Bot logic.
- `src/search/`: Dedicated BFS/DFS solvers for VCF (Victory by Continuous Four).
- `src/ui/`: Window management and OpenGL rendering code.
- `bobcat_ui/`: Custom UI component wrappers.

## Technical Details

The AI decision-making process follows a **Funnel Model**:
1. **Tactical Check**: The bot first runs a high-priority BFS/DFS search to detect any forced wins (VCF) or forced losses. If a forced sequence is found, it executes it immediately.
2. **Strategic Search**: If no immediate tactical end is visible, the MCTS engine takes over to explore the game tree, using UCT (Upper Confidence Bound 1 applied to trees) for node selection and random rollouts with heuristics.

## License

[MIT License](LICENSE)
