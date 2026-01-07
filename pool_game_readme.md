# Real-Time 8-Ball Pool Game

A real-time 8-ball pool game written in C using the Allegro 5 library.

## Prerequisites

You need to have these installed:

- **GCC** (C compiler)
- **Make**
- **Allegro 5** library

### Installation

**On Linux/Ubuntu:**
```bash
sudo apt-get update
sudo apt-get install build-essential liballegro5-dev
```

**On macOS:**
```bash
brew install allegro
```

## How to Build and Run

1. **Clone the repository:**
```bash
git clone https://github.com/EG98NU/Real-time-8-ball-pool-game.git
cd Real-time-8-ball-pool-game/Project_RTS
```

2. **Compile the game:**
```bash
make
```

3. **Run the game:**
```bash
./PoolGame
```

## How to Play

- Use your **mouse** to aim the cue stick
- Adjust the **power** of your shot
- **Click** to shoot the cue ball
- Follow standard 8-ball pool rules

## Makefile Commands

- `make` - Compiles the game
- `make clean` - Removes compiled files

## Troubleshooting

**If you get "allegro.h not found":**
- Install Allegro 5: `sudo apt-get install liballegro5-dev` (Linux)

**If you get "Permission denied":**
```bash
chmod +x ./PoolGame
```

## License

Open source project.

## Author

[@EG98NU](https://github.com/EG98NU)