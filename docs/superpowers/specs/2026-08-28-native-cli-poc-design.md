# Native Renderer CLI Proof-of-Concept

## Problem

The `cli/` directory demonstrates driving Elementary's native `Runtime`/`Renderer` from
a JavaScript graph, evaluated through an embedded QuickJS interpreter (`choc::javascript`).
That's the right shape for an end-user-facing tool, but it makes it impossible to isolate
the cost of the native engine itself from the cost of the JS layer (interpreter startup,
`__postNativeMessage__` JSON marshalling, `JSON.parse`, etc.) when benchmarking or profiling.

Elementary's runtime already exposes everything needed to build and render a graph purely
in C++: `elem::SymbolicGraph::createNode`, `elem::Renderer<FloatType>::renderGraph`, and a
native DSL under `elem::lib` (`runtime/elem/lib/{Core,Math,Oscillators,Envelopes,...}.h`)
mirroring the JS `el.*` helpers (`cycle`, `mul`, `add`, `train`, `adsr`, `seq`, ...). No new
graph-building abstraction needs to be invented — the same `elem::lib` calls used in
`tests/NativeRendererTests.cpp` are sufficient.

## Goal

Add a proof-of-concept sibling project, `cli-native/`, that plays and benchmarks a native
C++-constructed graph with no JS/QuickJS involved at all, so the two paths (JS-driven vs.
fully-native) can be benchmarked against each other on equal footing.

## Scope

- One example graph, ported from `cli/examples/00_HelloSine.js`:
  `mul(0.3, cycle(440))` on the left channel, `mul(0.3, cycle(441))` on the right.
- Two binaries, mirroring the existing `elemcli`/`elembench` pair conceptually but written
  fresh (not sharing code with `cli/Realtime.cpp` / `cli/Benchmark.cpp`, since removing the
  JS layer changes enough of the shape that sharing would add more coupling than value):
  - `elemcli-native` — realtime audio playback via miniaudio.
  - `elembench-native` — timed `process()` loop benchmark, run for both `float` and `double`.
- No JS fallback, no file-argument graph selection, no runtime graph switching. The graph
  to run is chosen at compile time (a single function call in each `main`).

## Non-goals

- No general-purpose native graph-building DSL beyond what `elem::lib` already provides.
- No shared abstraction/base between `cli/` and `cli-native/` — this is a standalone PoC.
- No porting of `01_FMArp.js` or `02_StrangerThings.js` (can follow later if useful).
- No CLI argument parsing for selecting graphs/files.

## Structure

```
cli-native/
  CMakeLists.txt
  HelloSine.h / HelloSine.cpp   # buildHelloSineGraph(): the native graph construction
  RealtimeMain.cpp              # elemcli-native entry point
  BenchmarkMain.cpp             # elembench-native entry point
  miniaudio.h                   # single-header dep, copied from cli/ (no shared build target)
```

### `HelloSine.h` / `.cpp`

Exposes:

```cpp
std::vector<std::shared_ptr<elem::SymbolicGraphNode>> buildHelloSineGraph();
```

Implemented using `elem::lib::mul({0.3, elem::lib::cycle(440.0)})` and
`elem::lib::mul({0.3, elem::lib::cycle(441.0)})`, matching `00_HelloSine.js` exactly.

### `RealtimeMain.cpp` (`elemcli-native`)

- Constructs `elem::Runtime<float>(44100.0, blockSize)`.
- Constructs `elem::Renderer<float>` around it and calls
  `renderer.renderGraph(buildHelloSineGraph())` once, before audio starts.
- Opens a miniaudio playback device (same `DeviceProxy`-style wrapper as `cli/Realtime.cpp`,
  minus the QuickJS context, the JS file argument, and the `__postNativeMessage__`/`__log__`
  message-channel plumbing — instructions are applied directly by `renderGraph`, not parsed
  from JSON).
- Blocks on stdin ("Press Enter to exit...") like the existing `elemcli`.

### `BenchmarkMain.cpp` (`elembench-native`)

Mirrors `cli/Benchmark.cpp`'s timing methodology so numbers are comparable:

1. Construct `elem::Runtime<FloatType>(44100.0, 512)`.
2. Build the renderer and call `renderGraph(buildHelloSineGraph())` — this replaces loading
   and evaluating a JS file.
3. Run one `process()` call to flush the initial render.
4. Sleep 1s (profiling timeline demarcation, as in the original).
5. Loop `process()` 10,000 times, timing each with `std::chrono::steady_clock`.
6. Report total and average iteration time.
7. Run for both `float` and `double`, as `BenchmarkMain.cpp` does today.

Because `buildHelloSineGraph()` is templated implicitly via `ElemNode`/`js::Number` (not
tied to `FloatType`), the same graph-building call works for both instantiations — the
`FloatType` template parameter only affects `Runtime<FloatType>`/`Renderer<FloatType>`.

## Build wiring

- Add `cli-native/CMakeLists.txt` defining `elemcli-native` and `elembench-native`
  executables, linking only against `elem::runtime` (no `choc`/QuickJS include dirs or
  dependencies at all).
- Add `add_subdirectory(cli-native)` next to `add_subdirectory(cli)` in the top-level
  `CMakeLists.txt`, inside the same `else()` branch that already builds `cli` and `tests`
  (i.e. skipped when `ONLY_BUILD_WASM` is set).

## Testing

This is a benchmarking PoC, not user-facing functionality — no new automated tests. Manual
verification: build both binaries, confirm `elemcli-native` plays the expected binaural
beating tone, and confirm `elembench-native` prints comparable timing output to
`elembench` when run against `cli/examples/dist/00_HelloSine.js`.
