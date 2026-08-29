# cli-native

This is a proof-of-concept, standalone C++ project that plays and benchmarks an
Elementary audio graph built entirely in native code, with no JS/QuickJS layer
involved. It ports the graph from `cli/examples/00_HelloSine.js` to a native
graph builder (`HelloSine.h`/`.cpp`) so the native-construction path can be
compared against the existing JS-driven `cli/` path.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target elemcli-native elembench-native
```

Note: `-DCMAKE_BUILD_TYPE=Release` matters — a debug build has debug logging
enabled and no optimization, which makes benchmark timings meaningless.

## Running

- `./build/elemcli-native` plays the graph to the default audio device. Press
  Enter to exit.
- `./build/elembench-native` runs a timed benchmark of the render loop and
  prints the results, for both `float` and `double`.

You can compare `elembench-native`'s output against `cli/elembench <bundled-js-file>`
(see `cli/README.md` for how to build the JS bundle) to compare the native-construction
and JS-driven graph paths.

## macOS notes

`elemcli-native` shares the same vendored `miniaudio.h` as `cli/` (see
`third_party/miniaudio/miniaudio.h`), so see the "macOS notes" section in
`cli/README.md` for details on Core Audio backend loading.
