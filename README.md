# Wallace

Simple Wayland gtk4-layer-shell clone of Gromit-MPX.

## Usage

- `GSK_RENDERER=cairo wallace` starts faster?
- `pkill -SIGUSR1 wallace`: toggle bottom/overlay layer
- `pkill -SIGUSR2 wallace`: toggle drawing/passthrough mode
- Mouse Left: draw stuff
- Mouse Right: eraser
- Mouse Middle: clear current monitor
- Mouse Wheel: change color
- Escape: exit

## Build

```
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -Bbuild
cmake --build build --config Release
```
