# Native Renderer CLI Proof-of-Concept Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a new `cli-native/` project, sibling to `cli/`, that plays and benchmarks a purely C++-constructed audio graph — no QuickJS/JS layer involved — so the native-only path can be benchmarked against the existing JS-driven `cli/` path.

**Architecture:** Two new executables (`elemcli-native`, `elembench-native`) each link a small static library containing one function, `elem::lib::buildHelloSineGraph()`, that constructs the same graph as `cli/examples/00_HelloSine.js` using the native `elem::lib` DSL (`elem::lib::cycle`, `elem::lib::mul`) and `elem::Renderer<FloatType>::renderGraph`. `elemcli-native` plays it through miniaudio; `elembench-native` times the `process()` loop for `float` and `double`, mirroring `cli/Benchmark.cpp`'s methodology.

**Tech Stack:** C++17, CMake, `elem::runtime` (header-only interface library), miniaudio (single-header, vendored copy from `cli/miniaudio.h`).

## Global Constraints

- No shared code with `cli/Realtime.cpp` / `cli/Benchmark.cpp` — `cli-native/` is a standalone project (per spec: independence was chosen deliberately over reuse).
- No JS/QuickJS/choc dependency anywhere in `cli-native/` — headers, includes, and CMake link libraries must not reference `choc` or QuickJS.
- Only the `00_HelloSine` example is ported (`mul(0.3, cycle(440))` left channel, `mul(0.3, cycle(441))` right channel) — no file-argument graph selection, no other examples.
- Graph selection is compile-time (a single function call in each `main`) — no runtime argument parsing for graph choice.
- No new automated test framework — per spec, this is a benchmarking PoC and verification is build success + manual run confirmation (spec explicitly excludes automated tests for this component).
- Sample rate 44100.0 Hz throughout, matching both `cli/Realtime.cpp` and `cli/Benchmark.cpp`.
- `elembench-native` runs the timed benchmark for both `float` and `double` instantiations, matching `cli/BenchmarkMain.cpp`.

---

## File Structure

```
cli-native/
  CMakeLists.txt         # defines elemcli_native_graph (static lib), elemcli-native, elembench-native
  HelloSine.h             # declares buildHelloSineGraph()
  HelloSine.cpp           # implements it with elem::lib::cycle/mul
  miniaudio.h              # vendored copy of cli/miniaudio.h (single-header audio I/O)
  RealtimeMain.cpp         # elemcli-native: opens playback device, renders graph, blocks on stdin
  BenchmarkMain.cpp        # elembench-native: timed process() loop, float + double
CMakeLists.txt             # (top-level, modified) add_subdirectory(cli-native)
```

---

### Task 1: Scaffold `cli-native/` and implement the native graph builder

**Files:**
- Create: `cli-native/HelloSine.h`
- Create: `cli-native/HelloSine.cpp`
- Create: `cli-native/CMakeLists.txt`
- Modify: `CMakeLists.txt:14` (top-level — add `add_subdirectory(cli-native)` next to `add_subdirectory(cli)`)

**Interfaces:**
- Produces: `std::vector<std::shared_ptr<elem::SymbolicGraphNode>> elem::lib::buildHelloSineGraph()` — used by Task 2 (`RealtimeMain.cpp`) and Task 3 (`BenchmarkMain.cpp`).
- Produces: CMake static library target `elemcli_native_graph`, `PUBLIC`-linked against `elem::runtime`, exposing `cli-native/` as a public include directory (so `#include "HelloSine.h"` works from sibling `.cpp` files added in later tasks).

- [ ] **Step 1: Write `cli-native/HelloSine.h`**

```cpp
#pragma once

#include <memory>
#include <vector>

#include "elem/SymbolicGraph.h"

namespace elem::lib {
    std::vector<std::shared_ptr<elem::SymbolicGraphNode>> buildHelloSineGraph();
}
```

- [ ] **Step 2: Write `cli-native/HelloSine.cpp`**

```cpp
#include "HelloSine.h"

#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

namespace elem::lib {
    std::vector<std::shared_ptr<elem::SymbolicGraphNode>> buildHelloSineGraph() {
        return {
            mul({0.3, cycle(440.0)}),
            mul({0.3, cycle(441.0)}),
        };
    }
}
```

This mirrors `cli/examples/00_HelloSine.js`'s `el.mul(0.3, el.cycle(440))` / `el.mul(0.3, el.cycle(441))` exactly, using the native DSL from `runtime/elem/lib/{Math,Oscillators}.h` instead of the JS `el.*` helpers.

- [ ] **Step 3: Write `cli-native/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.15)
project(cli-native VERSION 0.11.0)

add_library(elemcli_native_graph STATIC HelloSine.cpp)

target_include_directories(elemcli_native_graph PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(elemcli_native_graph PUBLIC
  cxx_std_17)

target_link_libraries(elemcli_native_graph PUBLIC
  elem::runtime)

if(MSVC)
  target_compile_options(elemcli_native_graph PRIVATE /W4)
else()
  target_compile_options(elemcli_native_graph PRIVATE -Wall -Wextra)
endif()
```

- [ ] **Step 4: Wire `cli-native` into the top-level build**

In `CMakeLists.txt` (repo root), find:

```cmake
else()
  enable_testing()
  add_subdirectory(runtime)
  add_subdirectory(cli)
  add_subdirectory(tests)
endif()
```

Change to:

```cmake
else()
  enable_testing()
  add_subdirectory(runtime)
  add_subdirectory(cli)
  add_subdirectory(cli-native)
  add_subdirectory(tests)
endif()
```

- [ ] **Step 5: Configure and build the `elemcli_native_graph` target to verify it compiles**

Run from the repo root:

```bash
cmake -S . -B build
cmake --build build --target elemcli_native_graph
```

Expected: build succeeds with no errors (there's nothing to run yet — this target is a library with no `main`, so success here just proves `HelloSine.cpp` compiles and links against `elem::runtime` cleanly).

- [ ] **Step 6: Commit**

```bash
git add cli-native/HelloSine.h cli-native/HelloSine.cpp cli-native/CMakeLists.txt CMakeLists.txt
git commit -m "Scaffold cli-native project with native HelloSine graph builder"
```

---

### Task 2: Implement `elemcli-native` (realtime playback binary)

**Files:**
- Create: `cli-native/miniaudio.h` (vendored copy of `cli/miniaudio.h`)
- Create: `cli-native/RealtimeMain.cpp`
- Modify: `cli-native/CMakeLists.txt` (append `elemcli-native` executable target)

**Interfaces:**
- Consumes: `elem::lib::buildHelloSineGraph()` from Task 1 (`#include "HelloSine.h"`).
- Consumes: CMake target `elemcli_native_graph` from Task 1.

- [ ] **Step 1: Vendor miniaudio into `cli-native/`**

```bash
cp cli/miniaudio.h cli-native/miniaudio.h
```

- [ ] **Step 2: Write `cli-native/RealtimeMain.cpp`**

```cpp
#include <array>
#include <iostream>
#include <memory>
#include <vector>

#include "HelloSine.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"


// A simple struct to proxy between the audio device and the Elementary engine
struct DeviceProxy {
    DeviceProxy(double sampleRate, size_t blockSize)
        : scratchData(2 * blockSize), runtime(std::make_shared<elem::Runtime<float>>(sampleRate, blockSize))
    {}

    void process(float* outputData, size_t numChannels, size_t numFrames)
    {
        if (scratchData.size() < (numChannels * numFrames))
            scratchData.resize(numChannels * numFrames);

        auto* deinterleaved = scratchData.data();
        std::array<float*, 2> ptrs {deinterleaved, deinterleaved + numFrames};

        runtime->process(
            nullptr,
            0,
            ptrs.data(),
            numChannels,
            numFrames,
            nullptr
        );

        for (size_t i = 0; i < numChannels; ++i)
        {
            for (size_t j = 0; j < numFrames; ++j)
            {
                outputData[i + numChannels * j] = deinterleaved[i * numFrames + j];
            }
        }
    }

    std::vector<float> scratchData;
    std::shared_ptr<elem::Runtime<float>> runtime;
};

void audioCallback(ma_device* pDevice, void* pOutput, const void* /* pInput */, ma_uint32 frameCount)
{
    auto* proxy = static_cast<DeviceProxy*>(pDevice->pUserData);

    auto numChannels = static_cast<size_t>(pDevice->playback.channels);
    auto numFrames = static_cast<size_t>(frameCount);

    proxy->process(static_cast<float*>(pOutput), numChannels, numFrames);
}

int main()
{
    ma_result result;

    ma_device_config deviceConfig;
    ma_device device;

    std::unique_ptr<DeviceProxy> proxy = std::make_unique<DeviceProxy>(44100.0, 1024);

    deviceConfig = ma_device_config_init(ma_device_type_playback);

    deviceConfig.playback.pDeviceID = nullptr;
    deviceConfig.playback.format    = ma_format_f32;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = 44100;
    deviceConfig.dataCallback       = audioCallback;
    deviceConfig.pUserData          = proxy.get();

    result = ma_device_init(nullptr, &deviceConfig, &device);

    if (result != MA_SUCCESS) {
        std::cout << "Failed to start the audio device! Exiting..." << std::endl;
        return 1;
    }

    // Build and render the native graph before audio starts — no JS/QuickJS involved
    elem::Renderer<float> renderer(proxy->runtime);
    auto stats = renderer.renderGraph(elem::lib::buildHelloSineGraph());

    std::cout << "Render result: " << stats.result << std::endl;
    std::cout << "Nodes added: " << stats.nodesAdded << std::endl;
    std::cout << "Edges added: " << stats.edgesAdded << std::endl;
    std::cout << "Props written: " << stats.propsWritten << std::endl;

    ma_device_start(&device);

    std::cout << "Press Enter to exit..." << std::endl;
    getchar();

    ma_device_uninit(&device);
    return 0;
}
```

- [ ] **Step 3: Append the `elemcli-native` executable target to `cli-native/CMakeLists.txt`**

Add to the end of the file:

```cmake
add_executable(elemcli-native RealtimeMain.cpp)

target_link_libraries(elemcli-native PRIVATE elemcli_native_graph)

if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(elemcli-native PRIVATE
    Threads::Threads
    ${CMAKE_DL_LIBS})
endif()
```

- [ ] **Step 4: Build `elemcli-native`**

```bash
cmake --build build --target elemcli-native
```

Expected: build succeeds with no errors.

- [ ] **Step 5: Run it and verify it starts, renders, and exits cleanly**

```bash
(sleep 1; echo) | ./build/cli-native/elemcli-native
```

(Adjust the binary path if your CMake generator nests it differently, e.g. `./build/cli-native/Debug/elemcli-native` on multi-config generators.)

Expected: prints `Render result: 0`, `Nodes added: <N>`, `Edges added: <N>`, `Props written: <N>` (all non-zero except possibly result, which should be `0` for `elem::ReturnCode::Ok()`), then `Press Enter to exit...`, then exits with status 0 after the piped newline arrives.

Note: this confirms the binary builds, renders the graph successfully, and exits cleanly. Confirming the actual audio output (two binaurally-beating sine tones at 440Hz/441Hz) requires a human listening to the output on this machine's default audio device — call this out explicitly rather than assuming it from the exit code alone.

- [ ] **Step 6: Commit**

```bash
git add cli-native/miniaudio.h cli-native/RealtimeMain.cpp cli-native/CMakeLists.txt
git commit -m "Add elemcli-native realtime playback binary"
```

---

### Task 3: Implement `elembench-native` (benchmark binary)

**Files:**
- Create: `cli-native/BenchmarkMain.cpp`
- Modify: `cli-native/CMakeLists.txt` (append `elembench-native` executable target)

**Interfaces:**
- Consumes: `elem::lib::buildHelloSineGraph()` from Task 1 (`#include "HelloSine.h"`).
- Consumes: CMake target `elemcli_native_graph` from Task 1.

- [ ] **Step 1: Write `cli-native/BenchmarkMain.cpp`**

```cpp
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "HelloSine.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"


template <typename FloatType>
void runBenchmark(std::string const& name) {
    auto runtime = std::make_shared<elem::Runtime<FloatType>>(44100.0, 512);
    elem::Renderer<FloatType> renderer(runtime);

    // Build and render the native graph — no JS/QuickJS involved
    auto renderStats = renderer.renderGraph(elem::lib::buildHelloSineGraph());
    std::cout << "Render result: " << renderStats.result << std::endl;

    std::vector<std::vector<FloatType>> scratchBuffers;
    std::vector<FloatType*> scratchPointers;

    for (int i = 0; i < 2; ++i) {
        scratchBuffers.push_back(std::vector<FloatType>(512));
        scratchPointers.push_back(scratchBuffers[i].data());
    }

    // Run the first block to process the events
    runtime->process(
        nullptr,
        0,
        scratchPointers.data(),
        2,
        512,
        0
    );

    // Now we can measure the static render process. We have this sleep
    // here to clearly demarcate, in the profiling timeline, which work is
    // related to the event processing above and which work is related to this
    // work loop here
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<double> deltas;

    for (size_t i = 0; i < 10'000; ++i) {
        auto t0 = std::chrono::steady_clock::now();

        runtime->process(
            nullptr,
            0,
            scratchPointers.data(),
            2,
            512,
            0
        );

        auto t1 = std::chrono::steady_clock::now();
        auto diffms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        deltas.push_back(static_cast<double>(diffms));
    }

    // Reporting
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto const sum = std::accumulate(deltas.begin(), deltas.end(), 0.0);
    auto const avg = sum / (double) deltas.size();

    std::cout << "[Running " << name << "]:" << std::endl;
    std::cout << "Total run time: " << sum << "us " << "(" << (sum / 1000000) << "s)" << std::endl;
    std::cout << "Average iteration time: " << avg << "us" << std::endl;
    std::cout << "Done" << std::endl << std::endl;
}

int main()
{
    runBenchmark<float>("Float");
    runBenchmark<double>("Double");

    return 0;
}
```

- [ ] **Step 2: Append the `elembench-native` executable target to `cli-native/CMakeLists.txt`**

Add to the end of the file:

```cmake
add_executable(elembench-native BenchmarkMain.cpp)

target_link_libraries(elembench-native PRIVATE elemcli_native_graph)
```

- [ ] **Step 3: Build `elembench-native`**

```bash
cmake --build build --target elembench-native
```

Expected: build succeeds with no errors.

- [ ] **Step 4: Run it and verify the timing report**

```bash
./build/cli-native/elembench-native
```

(Adjust the binary path if your CMake generator nests it differently, e.g. `./build/cli-native/Debug/elembench-native`.)

Expected output shape (values will vary by machine):

```
Render result: 0
[Running Float]:
Total run time: <N>us (<N>s)
Average iteration time: <N>us
Done

Render result: 0
[Running Double]:
Total run time: <N>us (<N>s)
Average iteration time: <N>us
Done

```

Both `Render result:` lines must print `0` (`elem::ReturnCode::Ok()`). The command should take roughly 2 seconds to run per instantiation (the two 1-second demarcation sleeps) plus the 10,000-iteration `process()` loop, and exit with status 0.

- [ ] **Step 5: Compare against the existing JS-driven benchmark (manual sanity check)**

If `cli/examples/dist/00_HelloSine.js` has been built (per `cli/README.md`'s `examples/` `npm run build` step), run the existing benchmark for comparison:

```bash
./build/cli/elembench cli/examples/dist/00_HelloSine.js
```

Compare the "Average iteration time" lines between `elembench` and `elembench-native` for both Float and Double. They should be in the same order of magnitude — `elembench-native`'s numbers reflect only `process()` cost since both benchmarks measure the same steady-state `runtime->process()` loop; the difference between the two tools is entirely in *how the graph got built* (JS-evaluated-and-parsed vs. native C++ construction), not in the per-block audio processing being measured here. This step is for context, not a pass/fail gate — the initial `renderGraph` construction cost is not currently isolated in either benchmark's reported numbers, so record the observation but don't treat it as a discrepancy to fix in this PoC.

- [ ] **Step 6: Commit**

```bash
git add cli-native/BenchmarkMain.cpp cli-native/CMakeLists.txt
git commit -m "Add elembench-native benchmark binary"
```
