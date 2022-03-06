
# `poked`

*NOTE* This is a temporary branch which is currently not dependent on the main
code.

## Build

To build `poked` and pokelets just call `make` (GNU make).
It'll build the code under `./build/` subdirectory.

## Dependencies

- `poke-fuse` depends on [`libfuse`](https://github.com/libfuse/libfuse)
- `poke-disasm` depends on [`capstone`](https://www.capstone-engine.org/index.html)
- `poke-out.py`, `poke-vu` and `poke-treevu` depend on `python3`
- `poke-websocket` depends on `python3` and `python3-websocket`
