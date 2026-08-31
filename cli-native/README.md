# cli-native

This version of the cli uses the native renderer instead of the js based renderer to render the Audio Graph.

The cli tool here is not meant to be a fully featured, end-user product, but rather
a concise demonstration of using Elementary's native renderer with the native engine.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target elemcli-native
```

## Running

- `./build/elemcli-native` plays the graph to the default audio device. Press
  Enter to exit.
