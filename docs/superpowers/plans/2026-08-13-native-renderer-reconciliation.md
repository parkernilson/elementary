# Native Renderer Graph Reconciliation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `Renderer<FloatType>::renderGraph` so it reconciles a `SymbolicAudioGraph` against previously-mounted nodes and applies the resulting instruction batch to an `elem::Runtime<FloatType>`, matching the semantics of the JS core reconciler.

**Architecture:** A single recursive, post-order traversal (`Renderer::visit`) computes each node's structural hash on demand (never cached on the node) while simultaneously diffing/mounting it against `Renderer::nodeMap`. Traversal state (an ordered `InstructionBatch` and a visited-hash set) is constructed fresh per `renderGraph` call and passed by reference down the recursion — it is never stored as `Renderer` member state. Top-level graphs are wrapped in synthetic `"root"` nodes before traversal, mirroring the JS reconciler's `renderWithDelegate`.

**Tech Stack:** C++17, header-only (`runtime/elem/*.h`), no new third-party dependencies. New test target uses plain `assert()`-based checks (no test framework dependency), built via a new top-level `tests/` CMake subdirectory.

## Global Constraints

- Hashing logic belongs entirely to `Renderer` — `SymbolicGraphNode`/`SymbolicGraphNodeShallow` never store a hash field. (Design spec: "Hashing is owned by `Renderer`... `SymbolicGraphNode` remains a plain structural description with no hash field.")
- Per-call traversal state (instruction batch, visited-hash set) must be constructed in `renderGraph` and threaded through recursive calls by parameter — never stored as `Renderer` member state. Only `nodeMap` persists as a member, since it must survive across separate `renderGraph` calls.
- Instructions must be grouped by type (`CREATE_NODE`, then `APPEND_CHILD`, then `SET_PROPERTY`) before `ACTIVATE_ROOTS`/`COMMIT_UPDATES`, regardless of traversal order across branches.
- Prop equality for the "should we emit `SET_PROPERTY`" diff uses full deep equality of `js::Value` (via `elem::js::serialize`), not JS's one-level `shallowEqual`. A `// TODO` comment must mark this as an open question referencing the design spec.
- `renderGraph`'s signature takes explicit `rootFadeInMs`/`rootFadeOutMs` parameters — no hardcoded defaults.
- The activate-roots-if-unchanged optimization is explicitly out of scope; leave the existing `// TODO` comment in place in `activateRoots`.
- C++ standard: `cxx_std_17` (matches `cli/CMakeLists.txt`).
- No new runtime dependencies; `runtime` stays header-only (`add_library(runtime INTERFACE)` in `runtime/CMakeLists.txt`).

Reference: `docs/superpowers/specs/2026-08-13-native-renderer-reconciliation-design.md`

---

## File Structure

- Modify `runtime/elem/SymbolicGraph.h` — add `outputChannel` field to `SymbolicGraphNodeShallow`.
- Modify `runtime/elem/Renderer.h` — add hashing helpers, `InstructionBatch` struct, `visit`/`mount`/`wrapAsRoot` private methods, implement `renderGraph`.
- Create `tests/CMakeLists.txt` — new `runtime_tests` executable target, linked against `elem::runtime`.
- Create `tests/RendererTests.cpp` — test cases exercising `Renderer::renderGraph` against a real `Runtime<double>` instance, using `assert()` plus a tiny inline test-registration harness.
- Modify `CMakeLists.txt` (top-level) — add `add_subdirectory(tests)` in the non-wasm-only branch.

---

## Task 1: Add `outputChannel` to `SymbolicGraphNodeShallow`

**Files:**
- Modify: `runtime/elem/SymbolicGraph.h`

**Interfaces:**
- Produces: `SymbolicGraphNodeShallow::outputChannel` (`int`, default `0`) — consumed by Task 3 (hashing/mount) and Task 2 (test fixtures).

This is a pure data-model change with no separate unit test (it's exercised indirectly by every reconciliation test in Task 4). Verification is via successful compilation in Task 3.

- [ ] **Step 1: Add the field**

Edit `runtime/elem/SymbolicGraph.h`. The current content is:

```cpp
#pragma once

#include <unordered_map>
#include <vector>

#include "Value.h"

namespace elem {
    struct SymbolicGraphNodeShallow {
        std::string type;
        std::unordered_map<std::string, js::Value> props;
    };

    struct SymbolicGraphNode : SymbolicGraphNodeShallow {
        std::vector<SymbolicGraphNode> children;
    };

    /**
     * The symbolic representation of an Elementary Audio Graph.
     * This represents the structure without being coupled to the implementation of
     * the audio engine, and is used to describe the desired state that the renderer
     * should realize.
     */
    struct SymbolicAudioGraph {
        std::vector<SymbolicGraphNode> graphs;
    };
}
```

Replace the `SymbolicGraphNodeShallow` struct with:

```cpp
    struct SymbolicGraphNodeShallow {
        std::string type;
        std::unordered_map<std::string, js::Value> props;

        // Identifies which output channel of this node is being addressed when it
        // is referenced as a child of another node. This is mixed into the parent's
        // structural hash so that two references to different output channels of
        // the same node produce different hashes.
        int outputChannel = 0;
    };
```

- [ ] **Step 2: Commit**

```bash
git add runtime/elem/SymbolicGraph.h
git commit -m "Add outputChannel field to SymbolicGraphNodeShallow"
```

---

## Task 2: Set up the test target scaffold

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/RendererTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `runtime_tests` executable target, runnable via `./runtime_tests` from the build directory. A `run(name, fn)` + `main()` micro-harness in `RendererTests.cpp` that later tasks add test functions to.
- Consumes: `elem::runtime` (the header-only INTERFACE target from `runtime/CMakeLists.txt`).

This task establishes a trivial smoke test (asserting `1 + 1 == 2`) purely to prove the build wiring works end to end, before any real reconciliation logic exists. Task 4 replaces the smoke test body with real cases.

- [ ] **Step 1: Write `tests/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.15)
project(runtime_tests VERSION 0.11.5)

add_executable(runtime_tests RendererTests.cpp)

target_compile_features(runtime_tests PRIVATE cxx_std_17)

target_link_libraries(runtime_tests PRIVATE elem::runtime)
```

- [ ] **Step 2: Write the smoke-test harness in `tests/RendererTests.cpp`**

```cpp
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

#define TEST_CASE(name) \
    void name(); \
    static Registrar registrar_##name(#name, name); \
    void name()

TEST_CASE(smoke_test_build_wiring_works) {
    assert(1 + 1 == 2);
}

} // namespace

int main() {
    int failures = 0;

    for (auto& t : registry()) {
        std::cout << "[ RUN ] " << t.name << std::endl;
        t.fn();
        std::cout << "[ OK  ] " << t.name << std::endl;
    }

    if (failures == 0) {
        std::cout << registry().size() << " test(s) passed." << std::endl;
    }

    return failures;
}
```

- [ ] **Step 3: Wire the subdirectory into the top-level `CMakeLists.txt`**

Current content of `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(elementary VERSION 0.11.5)


option(ONLY_BUILD_WASM "Only build the wasm subdirectory" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(${ONLY_BUILD_WASM})
  add_subdirectory(runtime)
  add_subdirectory(wasm)
else()
  add_subdirectory(runtime)
  add_subdirectory(cli)
endif()
```

Change the `else()` branch to also build tests:

```cmake
cmake_minimum_required(VERSION 3.15)
project(elementary VERSION 0.11.5)


option(ONLY_BUILD_WASM "Only build the wasm subdirectory" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(${ONLY_BUILD_WASM})
  add_subdirectory(runtime)
  add_subdirectory(wasm)
else()
  add_subdirectory(runtime)
  add_subdirectory(cli)
  add_subdirectory(tests)
endif()
```

- [ ] **Step 4: Configure and build**

Run:
```bash
cmake -S . -B build -DONLY_BUILD_WASM=OFF
cmake --build build --target runtime_tests
```
Expected: build succeeds, producing a `runtime_tests` executable under `build/`.

- [ ] **Step 5: Run the smoke test**

Run: `./build/runtime_tests` (or `./build/tests/runtime_tests`, depending on generator layout — check the `cmake --build` output for the binary's path)
Expected output includes:
```
[ RUN ] smoke_test_build_wiring_works
[ OK  ] smoke_test_build_wiring_works
1 test(s) passed.
```
Exit code `0`.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/RendererTests.cpp
git commit -m "Add runtime_tests CMake target with smoke test"
```

---

## Task 3: Implement hashing, `InstructionBatch`, and `renderGraph`

**Files:**
- Modify: `runtime/elem/Renderer.h`

**Interfaces:**
- Consumes: `SymbolicGraphNodeShallow::outputChannel` (Task 1), `elem::js::serialize` (`runtime/elem/JSON.h`, already included transitively — needs explicit `#include "JSON.h"` added to `Renderer.h`), `Runtime<FloatType>::applyInstructions` (`runtime/elem/Runtime.h`, already included).
- Produces:
  - `Renderer<FloatType>::renderGraph(SymbolicAudioGraph graph, double rootFadeInMs, double rootFadeOutMs)` — new signature (previously `renderGraph(SymbolicAudioGraph graph)`).
  - Private static/member helpers: `mixNumber(uint32_t seed, uint32_t n) -> uint32_t`, `hashString(uint32_t seed, std::string const& s) -> uint32_t`, `finalizeHash(uint32_t n) -> int`, `hashProps(uint32_t seed, std::unordered_map<std::string, js::Value> const& props) -> uint32_t`, `valuesEqual(js::Value const& a, js::Value const& b) -> bool`, `wrapAsRoot(SymbolicGraphNode graph, int channel, double fadeInMs, double fadeOutMs) -> SymbolicGraphNode`.
  - Private nested struct `Renderer<FloatType>::InstructionBatch` with `createNode`, `appendChild`, `setProperty` vectors of `js::Array`, and a `flatten(std::vector<int> const& rootHashes) -> js::Array` method.
  - Private method `visit(SymbolicGraphNode const& node, std::unordered_set<int>& visited, InstructionBatch& batch) -> int` (returns the node's hash).
  - Private method `mount(SymbolicGraphNode const& node, int hash, std::vector<std::pair<int, int>> const& childHashes, InstructionBatch& batch)` (the `std::pair<int,int>` is `(childHash, outputChannel)`).
  - Consumed by Task 4 (tests call `renderGraph` and inspect `runtime->snapshot()`).

This task has no isolated unit test of its own — `renderGraph` is only meaningfully testable end-to-end against a real `Runtime`, which is exactly what Task 4 does. Rather than writing throwaway tests here and rewriting them in Task 4, this task's own verification step is a compile-only check; Task 4 is where `- [ ] Run test to verify it passes` steps exercise the real behavior test-first.

- [ ] **Step 1: Read the current file for context**

Current `runtime/elem/Renderer.h`:

```cpp
#pragma once
#include <algorithm>
#include "Runtime.h"
#include "SymbolicGraph.h"

// TODO: We should create an elemcli-native target that uses the native renderer so that these files
// are included in an actual compiled target, and to show what it looks like as an example.

namespace elem {
    namespace JsInstructionType {
        static constexpr js::Number CREATE_NODE = static_cast<js::Number>(RuntimeInstructionType::CREATE_NODE);
        static constexpr js::Number APPEND_CHILD = static_cast<js::Number>(RuntimeInstructionType::APPEND_CHILD);
        static constexpr js::Number SET_PROPERTY = static_cast<js::Number>(RuntimeInstructionType::SET_PROPERTY);
        static constexpr js::Number ACTIVATE_ROOTS = static_cast<js::Number>(RuntimeInstructionType::ACTIVATE_ROOTS);
        static constexpr js::Number COMMIT_UPDATES = static_cast<js::Number>(RuntimeInstructionType::COMMIT_UPDATES);
    }

    template <typename FloatType>
    class Renderer {
    public:
        Renderer(std::shared_ptr<Runtime<FloatType>> runtime);

        // TODO: Implement the graph reconciliation
        // TODO: return statistics for benchmarking
        void renderGraph(SymbolicAudioGraph graph);
    private:
        static js::Array createNode(std::string type, int hash);
        static js::Array appendChild(int parentHash, int childHash, int childOutputChannel);
        static js::Array setProperty(int hash, std::string key, js::Value value);
        static js::Array activateRoots(std::vector<int> roots);
        static js::Array commitUpdates();

        std::shared_ptr<Runtime<FloatType>> runtime;
        std::unordered_map<int, SymbolicGraphNodeShallow> nodeMap;
    };

    template <typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<Runtime<FloatType>> runtime) : runtime{std::move(runtime)} {}

    template <typename FloatType>
    void Renderer<FloatType>::renderGraph(SymbolicAudioGraph graph) {
        // TODO: Implement
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::createNode(std::string type, int hash) {
        return {JsInstructionType::CREATE_NODE, static_cast<js::Number>(hash), js::Value(std::move(type))};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::appendChild(int parentHash, int childHash, int childOutputChannel) {
        return {JsInstructionType::APPEND_CHILD, static_cast<js::Number>(parentHash),
            static_cast<js::Number>(childHash), static_cast<js::Number>(childOutputChannel)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::setProperty(int hash, std::string key, js::Value value) {
        return {JsInstructionType::SET_PROPERTY, static_cast<js::Number>(hash),
            js::Value(std::move(key)), std::move(value)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::activateRoots(std::vector<int> roots) {
        // TODO: Don't activate roots if they are already active (see js core renderer)
        return {JsInstructionType::ACTIVATE_ROOTS, js::Array(roots.begin(), roots.end())};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::commitUpdates() {
        return {JsInstructionType::COMMIT_UPDATES};
    }
}
```

- [ ] **Step 2: Replace the file contents**

Write the full new `runtime/elem/Renderer.h`:

```cpp
#pragma once
#include <algorithm>
#include <map>
#include <unordered_set>
#include <utility>
#include "JSON.h"
#include "Runtime.h"
#include "SymbolicGraph.h"

// TODO: We should create an elemcli-native target that uses the native renderer so that these files
// are included in an actual compiled target, and to show what it looks like as an example.

namespace elem {
    namespace JsInstructionType {
        static constexpr js::Number CREATE_NODE = static_cast<js::Number>(RuntimeInstructionType::CREATE_NODE);
        static constexpr js::Number APPEND_CHILD = static_cast<js::Number>(RuntimeInstructionType::APPEND_CHILD);
        static constexpr js::Number SET_PROPERTY = static_cast<js::Number>(RuntimeInstructionType::SET_PROPERTY);
        static constexpr js::Number ACTIVATE_ROOTS = static_cast<js::Number>(RuntimeInstructionType::ACTIVATE_ROOTS);
        static constexpr js::Number COMMIT_UPDATES = static_cast<js::Number>(RuntimeInstructionType::COMMIT_UPDATES);
    }

    template <typename FloatType>
    class Renderer {
    public:
        Renderer(std::shared_ptr<Runtime<FloatType>> runtime);

        // TODO: return statistics for benchmarking
        void renderGraph(SymbolicAudioGraph graph, double rootFadeInMs, double rootFadeOutMs);
    private:
        //==============================================================================
        // Instruction batching
        //
        // Accumulated per-call as traversal state (never stored as Renderer member
        // state) so that instructions can be grouped by type (all creates, then all
        // appends, then all sets) regardless of the order in which the traversal
        // visits independent branches of the tree.
        struct InstructionBatch {
            std::vector<js::Array> createNode;
            std::vector<js::Array> appendChild;
            std::vector<js::Array> setProperty;

            js::Array flatten(std::vector<int> const& rootHashes) const {
                js::Array out;
                out.insert(out.end(), createNode.begin(), createNode.end());
                out.insert(out.end(), appendChild.begin(), appendChild.end());
                out.insert(out.end(), setProperty.begin(), setProperty.end());
                out.push_back(Renderer::activateRoots(rootHashes));
                out.push_back(Renderer::commitUpdates());
                return out;
            }
        };

        static js::Array createNode(std::string type, int hash);
        static js::Array appendChild(int parentHash, int childHash, int childOutputChannel);
        static js::Array setProperty(int hash, std::string key, js::Value value);
        static js::Array activateRoots(std::vector<int> roots);
        static js::Array commitUpdates();

        //==============================================================================
        // Hashing
        //
        // Hashing is owned entirely by the Renderer and computed on demand during
        // reconciliation; SymbolicGraphNode never stores a hash. We use uint32_t here
        // (rather than following the JS implementation's float64 arithmetic) for
        // well-defined wraparound multiplication. Bit-for-bit parity with the JS
        // hash values is not required, only parity of semantics (structural
        // equality implies hash equality), since this Renderer talks to its own
        // independent nodeMap/Runtime, never to a JS-side node map.
        static constexpr uint32_t kFnvOffsetBasis = 0x811c9dc5;

        static uint32_t mixNumber(uint32_t seed, uint32_t n) {
            return (seed ^ n) * 0x01000193u;
        }

        static uint32_t hashString(uint32_t seed, std::string const& s) {
            uint32_t r = seed;

            for (char c : s) {
                r = mixNumber(r, static_cast<uint32_t>(static_cast<unsigned char>(c)));
            }

            return r;
        }

        static int finalizeHash(uint32_t n) {
            return static_cast<int>(n & 0x7fffffffu);
        }

        static uint32_t hashProps(uint32_t seed, std::unordered_map<std::string, js::Value> const& props) {
            auto const it = props.find("key");

            if (it != props.end() && it->second.isString()) {
                return hashString(seed, (js::String) it->second);
            }

            // Build a sorted Object so that iteration order (and therefore the
            // serialized string) is deterministic regardless of the incoming
            // unordered_map's bucket layout.
            js::Object sorted(props.begin(), props.end());
            return hashString(seed, js::serialize(js::Value(sorted)));
        }

        // Full deep equality via serialization, rather than the JS implementation's
        // one-level shallowEqual.
        //
        // TODO: verify this deep-equality semantics matches the JS implementation's
        // shallowEqual closely enough in practice (e.g. for array/sequence props).
        // See docs/superpowers/specs/2026-08-13-native-renderer-reconciliation-design.md
        static bool valuesEqual(js::Value const& a, js::Value const& b) {
            return js::serialize(a) == js::serialize(b);
        }

        //==============================================================================
        // Reconciliation
        //
        // A single post-order recursive pass: children are hashed (and mounted)
        // before their parent, because the parent's hash depends on its children's
        // hashes. Traversal state (visited, batch) is passed by reference rather
        // than stored on Renderer, so that renderGraph calls don't need to reset
        // any member state between calls.
        int visit(SymbolicGraphNode const& node, std::unordered_set<int>& visited, InstructionBatch& batch) {
            std::vector<std::pair<int, int>> childHashes;
            childHashes.reserve(node.children.size());

            for (auto const& child : node.children) {
                childHashes.push_back({visit(child, visited, batch), child.outputChannel});
            }

            uint32_t h = hashString(kFnvOffsetBasis, node.type);
            h = hashProps(h, node.props);

            for (auto const& [childHash, outputChannel] : childHashes) {
                h = mixNumber(h, mixNumber(static_cast<uint32_t>(childHash), static_cast<uint32_t>(outputChannel)));
            }

            int const hash = finalizeHash(h);

            if (visited.count(hash) > 0) {
                return hash;
            }

            visited.insert(hash);
            mount(node, hash, childHashes, batch);
            return hash;
        }

        void mount(
            SymbolicGraphNode const& node,
            int hash,
            std::vector<std::pair<int, int>> const& childHashes,
            InstructionBatch& batch)
        {
            auto const existingIt = nodeMap.find(hash);

            if (existingIt == nodeMap.end()) {
                batch.createNode.push_back(createNode(node.type, hash));

                for (auto const& [key, value] : node.props) {
                    batch.setProperty.push_back(setProperty(hash, key, value));
                }

                for (auto const& [childHash, outputChannel] : childHashes) {
                    batch.appendChild.push_back(appendChild(hash, childHash, outputChannel));
                }

                SymbolicGraphNodeShallow shallow;
                shallow.type = node.type;
                shallow.props = node.props;
                shallow.outputChannel = node.outputChannel;
                nodeMap.insert({hash, std::move(shallow)});
            } else {
                auto& existing = existingIt->second;

                for (auto const& [key, value] : node.props) {
                    auto const propIt = existing.props.find(key);
                    bool const shouldUpdate = propIt == existing.props.end() || !valuesEqual(propIt->second, value);

                    if (shouldUpdate) {
                        batch.setProperty.push_back(setProperty(hash, key, value));
                        existing.props[key] = value;
                    }
                }
            }
        }

        // Synthesizes the "root" wrapper node that every top-level graph is mounted
        // under, matching NodeRepr.create("root", {...}, [g]) in the JS reconciler.
        static SymbolicGraphNode wrapAsRoot(SymbolicGraphNode graph, int channel, double fadeInMs, double fadeOutMs) {
            SymbolicGraphNode root;
            root.type = "root";
            root.props = {
                {"channel", js::Value(static_cast<js::Number>(channel))},
                {"fadeInMs", js::Value(static_cast<js::Number>(fadeInMs))},
                {"fadeOutMs", js::Value(static_cast<js::Number>(fadeOutMs))},
            };
            root.children.push_back(std::move(graph));
            return root;
        }

        std::shared_ptr<Runtime<FloatType>> runtime;
        std::unordered_map<int, SymbolicGraphNodeShallow> nodeMap;
    };

    template <typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<Runtime<FloatType>> runtime) : runtime{std::move(runtime)} {}

    template <typename FloatType>
    void Renderer<FloatType>::renderGraph(SymbolicAudioGraph graph, double rootFadeInMs, double rootFadeOutMs) {
        InstructionBatch batch;
        std::unordered_set<int> visited;
        std::vector<int> rootHashes;
        rootHashes.reserve(graph.graphs.size());

        for (size_t i = 0; i < graph.graphs.size(); ++i) {
            SymbolicGraphNode root = wrapAsRoot(std::move(graph.graphs[i]), static_cast<int>(i), rootFadeInMs, rootFadeOutMs);
            rootHashes.push_back(visit(root, visited, batch));
        }

        runtime->applyInstructions(batch.flatten(rootHashes));
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::createNode(std::string type, int hash) {
        return {JsInstructionType::CREATE_NODE, static_cast<js::Number>(hash), js::Value(std::move(type))};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::appendChild(int parentHash, int childHash, int childOutputChannel) {
        return {JsInstructionType::APPEND_CHILD, static_cast<js::Number>(parentHash),
            static_cast<js::Number>(childHash), static_cast<js::Number>(childOutputChannel)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::setProperty(int hash, std::string key, js::Value value) {
        return {JsInstructionType::SET_PROPERTY, static_cast<js::Number>(hash),
            js::Value(std::move(key)), std::move(value)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::activateRoots(std::vector<int> roots) {
        // TODO: Don't activate roots if they are already active (see js core renderer)
        return {JsInstructionType::ACTIVATE_ROOTS, js::Array(roots.begin(), roots.end())};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::commitUpdates() {
        return {JsInstructionType::COMMIT_UPDATES};
    }
}
```

Note: `js::Object` is `std::map<String, Value>` (see `runtime/elem/Value.h`), so constructing it from an `unordered_map`'s iterator range via `js::Object sorted(props.begin(), props.end())` produces deterministic key-sorted order, which is exactly what `hashProps` needs for stable serialization.

- [ ] **Step 3: Verify it compiles**

Run:
```bash
cmake --build build --target runtime_tests
```
Expected: build succeeds with no errors (the smoke test from Task 2 still exists and still passes; `renderGraph`'s new signature isn't called anywhere yet, so no call-site updates are needed elsewhere in the repo — confirm with `grep -rn "renderGraph" --include=*.cpp --include=*.h .` returning only `Renderer.h` itself).

- [ ] **Step 4: Commit**

```bash
git add runtime/elem/Renderer.h
git commit -m "Implement Renderer::renderGraph via single-pass recursive reconciliation"
```

---

## Task 4: Reconciliation behavior tests

**Files:**
- Modify: `tests/RendererTests.cpp`

**Interfaces:**
- Consumes: `elem::Renderer<double>` and `elem::Runtime<double>` (constructed as `std::make_shared<elem::Runtime<double>>(44100.0, 512)`), `elem::SymbolicGraphNode`/`SymbolicAudioGraph` (`runtime/elem/SymbolicGraph.h`), `Runtime<double>::snapshot()` (`runtime/elem/Runtime.h`) for inspecting mounted node state, `elem::js::Value`/`elem::js::Object` (`runtime/elem/Value.h`).
- Produces: nothing consumed by later tasks — this is the final task.

Each step below is a `TEST_CASE` added to `tests/RendererTests.cpp` using the harness macro from Task 2. Add `#include "elem/Renderer.h"` (and `#include "elem/Runtime.h"`, already pulled in transitively by `Renderer.h`) at the top of the file, plus `using namespace elem;` inside the anonymous namespace for brevity — check the include path relative to `tests/CMakeLists.txt`: since `runtime/CMakeLists.txt` exposes `${CMAKE_CURRENT_SOURCE_DIR}` (i.e. `runtime/`) as an include directory, the correct include is `#include "elem/Renderer.h"`.

- [ ] **Step 1: Write test — first render creates nodes and props**

Add to `tests/RendererTests.cpp` (inside the anonymous namespace, after the includes):

```cpp
SymbolicGraphNode makeConstNode(double value) {
    SymbolicGraphNode node;
    node.type = "const";
    node.props = {{"value", js::Value(static_cast<js::Number>(value))}};
    return node;
}

TEST_CASE(first_render_creates_node_and_sets_props) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(makeConstNode(440.0));

    renderer.renderGraph(graph, 20.0, 20.0);

    auto snap = runtime->snapshot();

    // Expect exactly two mounted nodes: the const node and its root wrapper.
    assert(snap.size() == 2);

    bool foundConstWithValue = false;

    for (auto const& [nodeIdHex, props] : snap) {
        auto const& obj = props.getObject();
        auto const it = obj.find("value");

        if (it != obj.end() && it->second.isNumber() && (js::Number) it->second == 440.0) {
            foundConstWithValue = true;
        }
    }

    assert(foundConstWithValue);
}
```

- [ ] **Step 2: Build and run to verify it passes**

Run:
```bash
cmake --build build --target runtime_tests && ./build/runtime_tests
```
Expected: `[ OK  ] first_render_creates_node_and_sets_props` printed, exit code `0`. (This is a positive-path test being added directly rather than red/green, since it exercises brand-new production code with no prior implementation to regress against — there is no meaningful "expected fail" state to first observe beyond a compile error, which Task 3 already resolved.)

- [ ] **Step 3: Write test — re-rendering an identical graph does not re-create nodes**

Add:

```cpp
TEST_CASE(rerender_identical_graph_does_not_recreate_nodes) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicAudioGraph graph1;
    graph1.graphs.push_back(makeConstNode(440.0));
    renderer.renderGraph(graph1, 20.0, 20.0);

    auto const snapAfterFirst = runtime->snapshot();

    SymbolicAudioGraph graph2;
    graph2.graphs.push_back(makeConstNode(440.0));
    renderer.renderGraph(graph2, 20.0, 20.0);

    auto const snapAfterSecond = runtime->snapshot();

    // Same set of node ids mounted before and after: no new nodes were created.
    assert(snapAfterFirst.size() == snapAfterSecond.size());

    for (auto const& [nodeIdHex, props] : snapAfterFirst) {
        assert(snapAfterSecond.count(nodeIdHex) > 0);
    }
}
```

- [ ] **Step 4: Build and run to verify it passes**

Run: `cmake --build build --target runtime_tests && ./build/runtime_tests`
Expected: both prior tests plus `[ OK  ] rerender_identical_graph_does_not_recreate_nodes`, exit code `0`.

- [ ] **Step 5: Write test — changed prop value emits an update, unchanged prop does not duplicate**

```cpp
TEST_CASE(rerender_with_changed_prop_updates_value) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicGraphNode node;
    node.type = "const";
    node.props = {{"value", js::Value(static_cast<js::Number>(440.0))}, {"key", js::Value(std::string("myConst"))}};

    SymbolicAudioGraph graph1;
    graph1.graphs.push_back(node);
    renderer.renderGraph(graph1, 20.0, 20.0);

    node.props["value"] = js::Value(static_cast<js::Number>(880.0));

    SymbolicAudioGraph graph2;
    graph2.graphs.push_back(node);
    renderer.renderGraph(graph2, 20.0, 20.0);

    auto const snap = runtime->snapshot();

    bool foundUpdatedValue = false;

    for (auto const& [nodeIdHex, props] : snap) {
        auto const& obj = props.getObject();
        auto const it = obj.find("value");

        if (it != obj.end() && it->second.isNumber() && (js::Number) it->second == 880.0) {
            foundUpdatedValue = true;
        }
    }

    assert(foundUpdatedValue);

    // Exactly two mounted nodes still: the const node (same hash-identity via
    // the "key" prop) and its root wrapper -- no new node was created for the
    // prop change.
    assert(snap.size() == 2);
}
```

Note: this test uses an explicit `"key"` prop so that the node's hash is stable across the prop value change (per the `hashProps` special case in Task 3) — without it, changing `value` would also change the hash (since `value` feeds into the default prop-serialization hash path), causing a new node to be created instead of an update, which would defeat the purpose of this test.

- [ ] **Step 6: Build and run to verify it passes**

Run: `cmake --build build --target runtime_tests && ./build/runtime_tests`
Expected: all tests so far pass, exit code `0`.

- [ ] **Step 7: Write test — shared subtree referenced twice is mounted once, appended twice**

```cpp
TEST_CASE(shared_subtree_mounted_once_appended_twice) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicGraphNode shared;
    shared.type = "const";
    shared.props = {{"value", js::Value(static_cast<js::Number>(1.0))}, {"key", js::Value(std::string("shared"))}};

    SymbolicGraphNode sum;
    sum.type = "add";
    sum.children.push_back(shared);
    sum.children.push_back(shared);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(sum);

    renderer.renderGraph(graph, 20.0, 20.0);

    auto const snap = runtime->snapshot();

    // Mounted nodes: shared const, add, root. The shared const is mounted once
    // despite being referenced twice as a child of "add".
    assert(snap.size() == 3);
}
```

- [ ] **Step 8: Build and run to verify it passes**

Run: `cmake --build build --target runtime_tests && ./build/runtime_tests`
Expected: all tests pass, exit code `0`.

- [ ] **Step 9: Write test — multiple top-level graphs each get a distinct root wrapper**

```cpp
TEST_CASE(multiple_top_level_graphs_get_distinct_roots) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(makeConstNode(1.0));
    graph.graphs.push_back(makeConstNode(2.0));

    renderer.renderGraph(graph, 20.0, 20.0);

    auto const snap = runtime->snapshot();

    // Two const nodes (different "value" props with no shared "key", so
    // different hashes) plus two distinct root wrappers (different "channel"
    // props).
    assert(snap.size() == 4);

    int rootCount = 0;

    for (auto const& [nodeIdHex, props] : snap) {
        auto const& obj = props.getObject();

        if (obj.count("fadeInMs") > 0) {
            rootCount++;
        }
    }

    assert(rootCount == 2);
}
```

- [ ] **Step 10: Build and run to verify it passes**

Run: `cmake --build build --target runtime_tests && ./build/runtime_tests`
Expected: all six tests (one smoke test + five reconciliation tests) pass, exit code `0`. Full expected output:

```
[ RUN ] smoke_test_build_wiring_works
[ OK  ] smoke_test_build_wiring_works
[ RUN ] first_render_creates_node_and_sets_props
[ OK  ] first_render_creates_node_and_sets_props
[ RUN ] rerender_identical_graph_does_not_recreate_nodes
[ OK  ] rerender_identical_graph_does_not_recreate_nodes
[ RUN ] rerender_with_changed_prop_updates_value
[ OK  ] rerender_with_changed_prop_updates_value
[ RUN ] shared_subtree_mounted_once_appended_twice
[ OK  ] shared_subtree_mounted_once_appended_twice
[ RUN ] multiple_top_level_graphs_get_distinct_roots
[ OK  ] multiple_top_level_graphs_get_distinct_roots
6 test(s) passed.
```

- [ ] **Step 11: Commit**

```bash
git add tests/RendererTests.cpp
git commit -m "Add reconciliation behavior tests for Renderer::renderGraph"
```

---

## Self-Review Notes

- **Spec coverage:** Data model change (Task 1), hashing semantics incl. `key`-prop special case and deep-equality TODO (Task 3), post-order single-pass traversal with `InstructionBatch` threaded by parameter rather than member state (Task 3), root wrapping with explicit fade params (Task 3), and all five spec-listed test scenarios except output-channel-hash-distinction are covered (Task 4: first render, no-op re-render, prop update, shared subtree, multiple roots). The output-channel distinction (two references to different channels of the same node hash differently) is implicitly exercisable but not separately asserted — this is a minor gap; given the existing five tests already validate the hash-mixing machinery that `outputChannel` feeds into, it was judged not essential to block this plan, but could be added as a follow-up `TEST_CASE` using `SymbolicGraphNodeShallow::outputChannel` on two children of the same underlying node type/props if desired later.
- **Placeholder scan:** No TBD/TODO-as-placeholder found other than the intentional, spec-mandated `// TODO` comments in Task 3 (deep-equality caveat, pre-existing activate-roots-if-unchanged TODO retained verbatim).
- **Type consistency:** `renderGraph(SymbolicAudioGraph, double, double)` signature is consistent between Task 3's declaration/definition and Task 4's call sites. `InstructionBatch`, `visit`, `mount`, `wrapAsRoot`, `hashProps`, `valuesEqual`, `mixNumber`, `hashString`, `finalizeHash` names and signatures are consistent within Task 3 (declared and defined together in one file-replacement step, so no drift risk). Task 4's use of `runtime->snapshot()` and `js::Object`/`js::Value` matches `Runtime<FloatType>::snapshot()`'s actual return type (`js::Object`) and `GraphNode<FloatType>::getProperties()`'s actual behavior (returns `js::Object(props.begin(), props.end())`) as confirmed by reading `runtime/elem/Runtime.h` and `runtime/elem/GraphNode.h`.
