# Wallace

Simple Wayland gtk4-layer-shell clone of Gromit-MPX, which is an on-screen annotation tool.
Basically this just draws red circles around things, but supports wlr-layer-shell, multi-monitor and multi-touch.

## Usage

- `GSK_RENDERER=cairo wallace` starts faster?
- `pkill -SIGUSR1 wallace`: toggle bottom/overlay layer
- `pkill -SIGUSR2 wallace`: toggle drawing/passthrough mode
- `pkill wallace` / Escape: exit
- Mouse Left / fingers: draw stuff
- Mouse Right: eraser
- Mouse Middle: clear current monitor
- Mouse Wheel: change color

## Build & Install

```
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -Bbuild
cmake --build build --config Release
sudo cmake --install build
```
