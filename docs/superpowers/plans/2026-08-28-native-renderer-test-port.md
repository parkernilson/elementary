# Port core/mc/hashing JS Tests to NativeRendererTests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the remaining tests from `core.test.js`, `mc.test.js`, and `hashing.test.js` (JS reconciler test suite) into `tests/` as GTest cases against the C++ native `Renderer`/`Runtime`, fixing three latent bugs uncovered along the way that would otherwise make the ported tests meaningless.

**Architecture:** Each test builds a symbolic graph with `elem::lib::*` helpers (and, where the JS test uses a node shape no `elem::lib` helper supports, `elem::SymbolicGraph::createNode` directly), renders it via `elem::Renderer<float>::renderGraph`, and asserts on the returned `RenderResult` counts plus a `Runtime::snapshot()` JSON diff against a committed fixture (via `elem::test::verifyGraphSnapshot`) — the same pattern as the two existing tests in `tests/NativeRendererTests.cpp`.

**Tech Stack:** C++20, GoogleTest/GoogleMock, CMake/Ninja, nlohmann::json (vendored).

## Global Constraints

- Every new/modified `.cpp` file must build via `cmake --build build --target native-renderer-tests -j 8` with zero new errors.
- All exact `EXPECT_EQ` count values below were verified empirically against the real (fixed) implementation via a scratch harness — do not recompute them by hand; use the values given.
- Follow the file's existing conventions exactly: `elem::Runtime<float>`, `elem::Renderer<float>`, `elem::test::verifyGraphSnapshot`, `elem::js::serialize(elem::js::Value(runtime->snapshot()))`, one `TEST(NativeRendererSnapshotTests, ...)` per case.
- Snapshot fixtures live in `tests/snapshots/<Name>.snapshot.json` (+ a `.snapshot.md` Mermaid companion, auto-written by `verifyGraphSnapshot`). On first run (no fixture) or with `UPDATE_SNAPSHOTS=1`, the fixture is (re)written and the test passes; commit the fixture alongside the test.
- Never use `--no-verify`/skip hooks; never force-push.
- Spec: `docs/superpowers/specs/2026-08-28-native-renderer-test-port-design.md`.

---

## Task 1: Fix `SymbolicGraph::createNode`'s hash to mix in each child's output channel

**Files:**
- Modify: `runtime/elem/SymbolicGraph.h:43-58`
- Test: existing `tests/NativeRendererTests.cpp` (regression-check only; no new test in this task)

**Interfaces:**
- Produces: `SymbolicGraph::createNode` now hashes children as `HashUtils::mixNumber(child->hash, child->outputChannel)` instead of raw `child->hash`. This is depended on by Task 10 (mc.test.js port), where it's the behavior under test.

**Context:** `runtime/elem/HashUtils.h` already defines `mixNumber(seed, n) = (seed ^ n) * 0x01000193`. `js/packages/core/src/NodeRepr.res:45-49` computes a node's hash by first mapping each child to `HashUtils.mixNumber(n.hash, n.outputChannel)`, then folding those into `hashNode`. The current C++ `createNode` skips this per-child channel mixing, so two nodes that reference *different* output channels of the same multi-channel child collapse to the same hash — exactly the bug `mc.test.js`'s single test exists to catch. Because `mixNumber(h, 0) != h` in general, this fix changes hash values for every node in the graph (not just multi-channel ones), which is why Task 2 must regenerate the two existing committed snapshot fixtures.

- [ ] **Step 1: Apply the fix**

In `runtime/elem/SymbolicGraph.h`, change:

```cpp
        static std::shared_ptr<SymbolicGraphNode> createNode(std::string kind, js::Object props,
                                            std::vector<std::shared_ptr<SymbolicGraphNode>> children) {
            std::vector<NodeId> childHashes;
            childHashes.reserve(children.size());
            for (const auto &child: children) {
                childHashes.push_back(child->hash);
            }
```

to:

```cpp
        static std::shared_ptr<SymbolicGraphNode> createNode(std::string kind, js::Object props,
                                            std::vector<std::shared_ptr<SymbolicGraphNode>> children) {
            std::vector<NodeId> childHashes;
            childHashes.reserve(children.size());
            for (const auto &child: children) {
                // A node's hash must depend not just on each child's hash, but also on the
                // outputChannel being addressed on that child. Otherwise two nodes referencing
                // different outputs of the same multi-channel child would hash identically,
                // even though they represent different signal paths. Matches NodeRepr.res.
                childHashes.push_back(HashUtils::mixNumber(child->hash, child->outputChannel));
            }
```

- [ ] **Step 2: Rebuild and confirm the two existing tests now fail on snapshot mismatch (expected — hash values changed)**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests
```

Expected: both `NativeRendererSnapshotTests.RendersBasicSineWave` and `NativeRendererSnapshotTests.NumericLiteralIsResolvedToConstantNode` FAIL with a snapshot mismatch (different hex hash keys), but their `EXPECT_EQ` count assertions (`nodesAdded==6`, `edgesAdded==5`, `propsWritten==5`, `result==Ok`) still PASS. If the counts fail too, stop and investigate — that would mean the fix broke something beyond hash values.

- [ ] **Step 3: Regenerate the two existing snapshot fixtures**

```bash
UPDATE_SNAPSHOTS=1 ./build/tests/native-renderer-tests
./build/tests/native-renderer-tests
```

Expected: first run PASSES (fixtures rewritten), second run (without the env var) also PASSES (fixtures now match). Confirm via `git diff --stat tests/snapshots/` that only `BasicSineWaveGraph.snapshot.json`, `BasicSineWaveGraph.snapshot.md`, `BasicSineWaveGraphNumericLiteral.snapshot.json`, `BasicSineWaveGraphNumericLiteral.snapshot.md` changed, and via `git diff tests/snapshots/BasicSineWaveGraph.snapshot.json` that only hash-string values changed (same `kind`s, same `props`, same graph shape).

- [ ] **Step 4: Commit**

```bash
git add runtime/elem/SymbolicGraph.h tests/snapshots/BasicSineWaveGraph.snapshot.json tests/snapshots/BasicSineWaveGraph.snapshot.md tests/snapshots/BasicSineWaveGraphNumericLiteral.snapshot.json tests/snapshots/BasicSineWaveGraphNumericLiteral.snapshot.md
git commit -m "Mix child outputChannel into node hash, matching NodeRepr.res"
```

---

## Task 2: Fix `Renderer.h`'s child-channel bug and `NodeRef::setter`'s return type/instruction bug

**Files:**
- Modify: `runtime/elem/Renderer.h:70-73` (`NodeRef` struct), `:118-130` (`mount`), `:204-230` (`createRef`)
- Test: existing `tests/NativeRendererTests.cpp` (regression-check only; no new test in this task)

**Interfaces:**
- Produces: `NodeRef::setter` is now `std::function<RenderResult(js::Object newProps)>` (was `std::function<void(js::Object)>`). Depended on by Task 9 (`refs` test), which calls `ref.setter(...)` and asserts on the returned `RenderResult`.
- Produces: `mount()`'s `appendChild` instructions now use `child->outputChannel` instead of `node.outputChannel`. Depended on by Task 10 (mc test).

**Context:** Confirmed against `js/packages/core/src/Reconciler.res:53` (`RenderDelegate.appendChild(delegate, node.hash, child.hash, child.outputChannel)`) that `mount()` should use the *child's* output channel, not the parent's own. Separately, `createRef`'s setter lambda currently calls `runtime->applyInstructions(instructions)`, passing the `InstructionBatch` struct directly where a `js::Array` is expected — this only compiles today because `createRef` is a template never instantiated anywhere in the codebase; it would be a real compile error the moment a test calls it.

- [ ] **Step 1: Fix the `appendChild` channel bug in `mount()`**

In `runtime/elem/Renderer.h`, change:

```cpp
            for (const auto &child: node.children) {
                batch.appendChild.emplace_back(makeAppendChildInstruction(node.hash, child->hash, node.outputChannel));
            }
```

to:

```cpp
            for (const auto &child: node.children) {
                batch.appendChild.emplace_back(makeAppendChildInstruction(node.hash, child->hash, child->outputChannel));
            }
```

- [ ] **Step 2: Change `NodeRef::setter`'s type and `createRef`'s implementation**

Change the struct:

```cpp
    struct NodeRef {
        std::shared_ptr<SymbolicGraphNode> node;
        std::function<void(js::Object newProps)> setter;
    };
```

to:

```cpp
    struct NodeRef {
        std::shared_ptr<SymbolicGraphNode> node;
        std::function<RenderResult(js::Object newProps)> setter;
    };
```

Change the setter lambda body inside `createRef` from:

```cpp
        auto setProperty = [hash = node->hash, wRuntime](const js::Object& newProps) {
            const auto& runtime = wRuntime.lock();
            if (runtime == nullptr) {
                // TODO: Return an error code or throw an error or something
                return;
            }

            if (const auto* existing = runtime->findNode(hash); existing != nullptr) {
                InstructionBatch instructions;
                updateNodeProps(hash, existing->getProperties(), newProps, instructions);
                instructions.commitUpdates.emplace_back(makeCommitUpdatesInstruction());
                runtime->applyInstructions(instructions);
            }
        };
```

to:

```cpp
        auto setProperty = [hash = node->hash, wRuntime](const js::Object& newProps) -> RenderResult {
            RenderResult stats;

            const auto& runtime = wRuntime.lock();
            if (runtime == nullptr) {
                stats.result = ReturnCode::NodeNotFound();
                return stats;
            }

            const auto* existing = runtime->findNode(hash);
            if (existing == nullptr) {
                stats.result = ReturnCode::NodeNotFound();
                return stats;
            }

            InstructionBatch instructions;
            updateNodeProps(hash, existing->getProperties(), newProps, instructions);
            stats.propsWritten = static_cast<int32_t>(instructions.setProperty.size());

            instructions.commitUpdates.emplace_back(makeCommitUpdatesInstruction());
            stats.result = runtime->applyInstructions(instructions.takeBatchedInstructions());

            return stats;
        };
```

- [ ] **Step 3: Rebuild and confirm the two existing tests still pass (channel is always 0 in those graphs, so no behavior change expected there)**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests
```

Expected: both tests PASS with no snapshot changes (their graphs have no multi-channel nodes, so `child->outputChannel` is always 0, same as the old `node.outputChannel` default).

- [ ] **Step 4: Commit**

```bash
git add runtime/elem/Renderer.h
git commit -m "Fix appendChild to use child's outputChannel; fix NodeRef setter's return type and instruction batch bug"
```

---

## Task 3: Create `tests/CoreRendererTests.cpp` with the `distinguish by props` test

**Files:**
- Create: `tests/CoreRendererTests.cpp`
- Modify: `tests/CMakeLists.txt:7` (add the new source file to `add_executable`)

**Interfaces:**
- Consumes: `elem::Renderer<float>`, `elem::Runtime<float>`, `elem::test::verifyGraphSnapshot` (from `tests/GraphSnapshotTestUtils.h`), `elem::lib::le`, `elem::lib::phasor`, `elem::lib::constant` (all from `elem/lib/Math.h`/`elem/lib/Core.h`, which pull in `elem/lib/NodeUtils.h` transitively), `elem::SymbolicGraph::createNode`, `elem::AudioBufferResource`.
- Produces: `tests/CoreRendererTests.cpp`, extended by Tasks 4-10 with one `TEST()` each. Also carries, near the top, the comment block explaining why `lib.test.js` isn't ported.

**Context — ground truth for this test:** Port of `core.test.js`'s `distinguish by props`. Two renderVoice calls (`renderVoice('test/path.wav', {0,0,1})`, `renderVoice('test/path.wav', {0,1,0})`) share a `le`/`phasor`/const subtree (unkeyed, identical props) but have distinct `seq` nodes (different `seq` array), which makes their parent `sample` nodes distinct too, wrapped in 2 distinct `root` nodes (channel 0 vs 1). `elem::lib::sample`/`elem::lib::seq` don't fit this shape (they require 2 children each, matching the real `SampleNode`/`SequenceNode` contract), so this test builds `sample`/`seq` nodes directly via `SymbolicGraph::createNode`, matching the JS test's literal (simplified, not fully "correct") graph. The real `SampleNode`'s `path` property requires a registered resource, so the test registers a dummy one via `Runtime::addSharedResource` before rendering (verified empirically: `nodesAdded=10, edgesAdded=9, propsWritten=12, result=Ok`).

- [ ] **Step 1: Write `tests/CoreRendererTests.cpp` with the skip-comment and this first test**

```cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/AudioBufferResource.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/SymbolicGraph.h"

#include "GraphSnapshotTestUtils.h"
#include "elem/lib/Core.h"
#include "elem/lib/Math.h"
#include "elem/lib/Mc.h"
#include "elem/lib/Oscillators.h"

// Port of js/packages/core/__tests__/core.test.js and mc.test.js.
//
// lib.test.js's single test ("errors on graph construction") is not ported here: both of its
// assertions are compile-time-enforced in C++, not runtime-throwable conditions.
//   - el.seq({}, 1) checks a missing required argument; elem::lib::seq has fixed arity
//     (props, trigger, reset) enforced by the compiler.
//   - el.mul(1, 2, '4') checks a string where a number is expected; ElemNode is
//     std::variant<NodeRepr, js::Number> with no implicit conversion from string literals.
// Neither is expressible as a GTest runtime assertion.

TEST(NativeRendererSnapshotTests, DistinguishByProps) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    runtime->addSharedResource("test/path.wav", std::make_unique<elem::AudioBufferResource>(1, 512));
    elem::Renderer<float> renderer(runtime);

    auto renderVoice = [](std::string path, std::vector<elem::js::Number> seq) {
        return elem::SymbolicGraph::createNode("sample", elem::js::Object{{"path", path}}, {
            elem::SymbolicGraph::createNode("seq", elem::js::Object{{"seq", elem::js::Array(seq.begin(), seq.end())}}, {
                elem::lib::le(
                    elem::lib::phasor(elem::lib::constant(2.0)),
                    elem::lib::constant(0.5)
                )
            })
        });
    };

    const auto result = renderer.renderGraph({
        renderVoice("test/path.wav", {0, 0, 1}),
        renderVoice("test/path.wav", {0, 1, 0}),
    });

    elem::test::verifyGraphSnapshot(
        "DistinguishByProps",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 10);
    EXPECT_EQ(result.edgesAdded, 9);
    EXPECT_EQ(result.propsWritten, 12);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Add the new file to the test target**

In `tests/CMakeLists.txt`, change:

```cmake
add_executable(native-renderer-tests EXCLUDE_FROM_ALL NativeRendererTests.cpp)
```

to:

```cmake
add_executable(native-renderer-tests EXCLUDE_FROM_ALL
    NativeRendererTests.cpp
    CoreRendererTests.cpp
)
```

- [ ] **Step 3: Reconfigure, build, and run**

```bash
cmake -S . -B build
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.DistinguishByProps
```

Expected: PASSES (fixture auto-written on this first run since `tests/snapshots/DistinguishByProps.snapshot.json` doesn't exist yet).

- [ ] **Step 4: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/CMakeLists.txt tests/snapshots/DistinguishByProps.snapshot.json tests/snapshots/DistinguishByProps.snapshot.md
git commit -m "Port core.test.js 'distinguish by props' to NativeRendererTests"
```

---

## Task 4: Add the `multi-channel basics` test

**Files:**
- Modify: `tests/CoreRendererTests.cpp` (append a new `TEST()`)

**Interfaces:**
- Consumes: `elem::lib::cycle` (from `elem/lib/Oscillators.h`, already included).

**Context — ground truth:** Port of `core.test.js`'s `multi-channel basics`. The same `elem::lib::cycle(440.0)` graph (matching the existing `RendersBasicSineWave` test's graph exactly) rendered as two roots. Everything below the roots is shared; only the 2 root nodes differ (verified: `nodesAdded=7, edgesAdded=6, propsWritten=8, result=Ok`).

- [ ] **Step 1: Append this test to `tests/CoreRendererTests.cpp`**

```cpp
TEST(NativeRendererSnapshotTests, MultiChannelBasics) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    // Run the same thing in two channels; we expect structural sharing except for the root nodes.
    const auto result = renderer.renderGraph({
        elem::lib::cycle(440.0),
        elem::lib::cycle(440.0),
    });

    elem::test::verifyGraphSnapshot(
        "MultiChannelBasics",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 7);
    EXPECT_EQ(result.edgesAdded, 6);
    EXPECT_EQ(result.propsWritten, 8);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Rebuild and run just this test**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.MultiChannelBasics
```

Expected: PASSES (fixture auto-written).

- [ ] **Step 3: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/snapshots/MultiChannelBasics.snapshot.json tests/snapshots/MultiChannelBasics.snapshot.md
git commit -m "Port core.test.js 'multi-channel basics' to NativeRendererTests"
```

---

## Task 5: Add the `simple sharing` test

**Files:**
- Modify: `tests/CoreRendererTests.cpp` (append a new `TEST()`)

**Interfaces:**
- Consumes: `elem::lib::cycle`, `elem::lib::tanh` (from `elem/lib/Math.h`, already included).

**Context — ground truth:** Port of `core.test.js`'s `simple sharing`. First render: `cycle(440.0)` (identical to `RendersBasicSineWave`, verified `nodesAdded=6, edgesAdded=5, propsWritten=5`). Second render on the *same* `Renderer`: `tanh(cycle(440.0))`. Because a `root` node's hash depends on its child's hash, and the child changed (from the `sin` node to the new `tanh` node), the second render's `root` node is itself a *new* node (not a reuse of the first root) — verified: second render is `nodesAdded=2` (the new `tanh` node + the new `root` node), `edgesAdded=2` (`newRoot->tanh`, `tanh->existingSin`), `propsWritten=3` (the new root's `channel`/`fadeInMs`/`fadeOutMs`), `result=Ok`. The old root and the rest of the original subtree remain in the runtime (never GC'd) but are no longer active.

- [ ] **Step 1: Append this test to `tests/CoreRendererTests.cpp`**

```cpp
TEST(NativeRendererSnapshotTests, SimpleSharing) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({elem::lib::cycle(440.0)});

    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 5);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Second render inserts a tanh at the top; we should find the existing subtree and
    // share it, adding only the new tanh node and a new root (since the root's hash
    // depends on its child, which changed).
    const auto result2 = renderer.renderGraph({elem::lib::tanh(elem::lib::cycle(440.0))});

    elem::test::verifyGraphSnapshot(
        "SimpleSharing",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 2);
    EXPECT_EQ(result2.edgesAdded, 2);
    EXPECT_EQ(result2.propsWritten, 3);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Rebuild and run just this test**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.SimpleSharing
```

Expected: PASSES.

- [ ] **Step 3: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/snapshots/SimpleSharing.snapshot.json tests/snapshots/SimpleSharing.snapshot.md
git commit -m "Port core.test.js 'simple sharing' to NativeRendererTests"
```

---

## Task 6: Add the `distinguished subtrees by key` test

**Files:**
- Modify: `tests/CoreRendererTests.cpp` (append a new `TEST()`)

**Interfaces:**
- Consumes: `elem::lib::add`, `elem::lib::sin`, `elem::lib::mul`, `elem::lib::phasor`, `elem::lib::constant`, `elem::lib::PI<float>` (all already included).

**Context — ground truth:** Port of `core.test.js`'s `distinguished subtrees by key`. Four voices (`renderVoice(key, freq)` = `sin(mul(constant(2*PI), phasor(constant(freq, key))))`), all `freq=440` but with distinct keys `fq1`..`fq4`, combined via `add`. Since each leaf `const` has a distinct string key, `HashUtils::hashProps` hashes on the key alone, so all 4 voices are structurally distinct end-to-end (only the shared `const(2*PI)` node is deduplicated). Verified: `nodesAdded=19, edgesAdded=21, propsWritten=12, result=Ok`.

- [ ] **Step 1: Append this test to `tests/CoreRendererTests.cpp`**

```cpp
static elem::lib::NodeRepr renderKeyedVoice(std::string key, double freq) {
    return elem::lib::sin(elem::lib::mul({
        elem::lib::constant(2.0 * elem::lib::PI<float>),
        elem::lib::phasor(elem::lib::constant(freq, key)),
    }));
}

TEST(NativeRendererSnapshotTests, DistinguishedSubtreesByKey) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 440),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    elem::test::verifyGraphSnapshot(
        "DistinguishedSubtreesByKey",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 19);
    EXPECT_EQ(result.edgesAdded, 21);
    EXPECT_EQ(result.propsWritten, 12);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Rebuild and run just this test**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.DistinguishedSubtreesByKey
```

Expected: PASSES.

- [ ] **Step 3: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/snapshots/DistinguishedSubtreesByKey.snapshot.json tests/snapshots/DistinguishedSubtreesByKey.snapshot.md
git commit -m "Port core.test.js 'distinguished subtrees by key' to NativeRendererTests"
```

---

## Task 7: Add the `structural equality with value change` test

**Files:**
- Modify: `tests/CoreRendererTests.cpp` (append a new `TEST()`)

**Interfaces:**
- Consumes: `renderKeyedVoice` (defined in Task 6, same file).

**Context — ground truth:** Port of `core.test.js`'s `structural equality with value change`. Same 4-voice graph as Task 6 rendered twice on the same `Renderer`; the second render changes `fq1`'s frequency from 440 to 441 but keeps its key `fq1`. Because `HashUtils::hashProps` hashes on the key alone (ignoring `value`), the node is reused — but `mount()` still diffs old vs. new props for *reused* nodes and emits a `setProperty` for the changed `value`. Verified: second render is `nodesAdded=0, edgesAdded=0, propsWritten=1, result=Ok` (one prop update, zero structural change).

- [ ] **Step 1: Append this test to `tests/CoreRendererTests.cpp`**

```cpp
TEST(NativeRendererSnapshotTests, StructuralEqualityWithValueChange) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 440),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    EXPECT_EQ(result1.nodesAdded, 19);
    EXPECT_EQ(result1.edgesAdded, 21);
    EXPECT_EQ(result1.propsWritten, 12);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Change one of the keyed values; we expect structural equality (no new nodes or
    // edges) since the node is found by its key, but the changed value is still written.
    const auto result2 = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 441),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    elem::test::verifyGraphSnapshot(
        "StructuralEqualityWithValueChange",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 0);
    EXPECT_EQ(result2.edgesAdded, 0);
    EXPECT_EQ(result2.propsWritten, 1);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Rebuild and run just this test**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.StructuralEqualityWithValueChange
```

Expected: PASSES.

- [ ] **Step 3: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/snapshots/StructuralEqualityWithValueChange.snapshot.json tests/snapshots/StructuralEqualityWithValueChange.snapshot.md
git commit -m "Port core.test.js 'structural equality with value change' to NativeRendererTests"
```

---

## Task 8: Add the `switch and switch back` test

**Files:**
- Modify: `tests/CoreRendererTests.cpp` (append a new `TEST()`)

**Interfaces:**
- Consumes: `renderKeyedVoice` (defined in Task 6, same file).

**Context — ground truth:** Port of `core.test.js`'s `switch and switch back`. Three sequential renders on the same `Renderer`: voice A (key `hi`, freq 440), voice B (key `bye`, freq 880), then voice A again. This codebase never calls `Runtime::gc()` automatically (matching JS's default behavior for this test — nothing in the JS test triggers GC either), so A's subtree from the first render is never removed; the third render finds it unchanged. Verified: render 1 is `nodesAdded=6, edgesAdded=5, propsWritten=6` (note: 6 props, not 5, because this voice's `const` node carries *two* props, `key` and `value`, unlike the unkeyed `cycle(440.0)` used in Tasks 4-5); render 2 is `nodesAdded=5, edgesAdded=5, propsWritten=5` (B's `sin`/`mul`/`phasor`/`const` are new, but the `const(2*PI)` is shared with A, and B's `root` node is new — the old root from render 1 is deactivated, not deleted); render 3 is `nodesAdded=0, edgesAdded=0, propsWritten=0, result=Ok` (a true no-op — A's root/subtree and its props are all unchanged from render 1).

- [ ] **Step 1: Append this test to `tests/CoreRendererTests.cpp`**

```cpp
// Testing here to ensure that root activation/deactivation works as expected across
// renders, and that nodes are not garbage collected just because they became inactive.
TEST(NativeRendererSnapshotTests, SwitchAndSwitchBack) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({renderKeyedVoice("hi", 440)});
    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 6);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    const auto result2 = renderer.renderGraph({renderKeyedVoice("bye", 880)});
    EXPECT_EQ(result2.nodesAdded, 5);
    EXPECT_EQ(result2.edgesAdded, 5);
    EXPECT_EQ(result2.propsWritten, 5);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());

    // Third render switches back to A. We expect this to be a full no-op: A's subtree
    // was never garbage collected, so nothing new needs to be created or written.
    const auto result3 = renderer.renderGraph({renderKeyedVoice("hi", 440)});

    elem::test::verifyGraphSnapshot(
        "SwitchAndSwitchBack",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result3.nodesAdded, 0);
    EXPECT_EQ(result3.edgesAdded, 0);
    EXPECT_EQ(result3.propsWritten, 0);
    EXPECT_EQ(result3.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Rebuild and run just this test**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.SwitchAndSwitchBack
```

Expected: PASSES.

- [ ] **Step 3: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/snapshots/SwitchAndSwitchBack.snapshot.json tests/snapshots/SwitchAndSwitchBack.snapshot.md
git commit -m "Port core.test.js 'switch and switch back' to NativeRendererTests"
```

---

## Task 9: Add the `refs` test

**Files:**
- Modify: `tests/CoreRendererTests.cpp` (append a new `TEST()`)

**Interfaces:**
- Consumes: `elem::Renderer<float>::createRef` returning `elem::NodeRef{node, setter}` where `setter` is `std::function<RenderResult(js::Object)>` (from Task 2), `elem::lib::ElemNode`, `elem::lib::sin`, `elem::lib::mul`, `elem::lib::phasor`, `elem::lib::constant`, `elem::lib::PI<float>`.

**Context — ground truth:** Port of `core.test.js`'s `refs`. Builds a sine tone with its frequency `const` node created via `createRef` (no key, matching the JS test's `tr.createRef("const", {value: 440}, [])`), renders it once, then calls the ref's setter with a new value and asserts on the returned `RenderResult` directly (this is the API extension from Task 2 — the JS test instead asserted on a raw instruction batch, which the C++ `Renderer` doesn't expose). Verified: initial render is `nodesAdded=6, edgesAdded=5, propsWritten=5, result=Ok`; the setter call is `nodesAdded=0, edgesAdded=0, propsWritten=1, result=Ok` (a single prop update, no structural change).

- [ ] **Step 1: Append this test to `tests/CoreRendererTests.cpp`**

```cpp
TEST(NativeRendererSnapshotTests, RefSetterUpdatesPropsWithoutRecreatingTree) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    // Sine tone with a frequency set by ref.
    auto ref = renderer.createRef("const", elem::js::Object{{"value", 440.0}}, {});

    const auto result1 = renderer.renderGraph({
        elem::lib::sin(elem::lib::mul({
            elem::lib::constant(2.0 * elem::lib::PI<float>),
            elem::lib::phasor(elem::lib::ElemNode(ref.node)),
        }))
    });

    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 5);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Using our ref setter: we expect a single prop update, no structural change.
    const auto result2 = ref.setter(elem::js::Object{{"value", 550.0}});

    elem::test::verifyGraphSnapshot(
        "RefSetterUpdatesPropsWithoutRecreatingTree",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 0);
    EXPECT_EQ(result2.edgesAdded, 0);
    EXPECT_EQ(result2.propsWritten, 1);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Rebuild and run just this test**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.RefSetterUpdatesPropsWithoutRecreatingTree
```

Expected: PASSES. (This is also the first real compilation of `Renderer::createRef` in the codebase — if it fails to compile, re-check Task 2's Step 2 was applied exactly.)

- [ ] **Step 3: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/snapshots/RefSetterUpdatesPropsWithoutRecreatingTree.snapshot.json tests/snapshots/RefSetterUpdatesPropsWithoutRecreatingTree.snapshot.md
git commit -m "Port core.test.js 'refs' to NativeRendererTests"
```

---

## Task 10: Add the `mc.test.js` outputChannel-hashing test

**Files:**
- Modify: `tests/CoreRendererTests.cpp` (append a new `TEST()`)

**Interfaces:**
- Consumes: `elem::lib::sampleseq2`, `elem::lib::MCSampleSeq2Props`, `elem::lib::ValueTimeSeqStep` (from `elem/lib/Mc.h`/`elem/lib/Core.h`, already included), `elem::lib::mul`, `elem::lib::add`, `elem::AudioBufferResource`.

**Context — ground truth:** Port of `mc.test.js`'s single test, `hashing reflects outputChannel from child nodes`. Builds `add(mul(0.5, ch0), mul(0.5, ch1))` where `ch0`/`ch1` are the two `unpack()`'d channels of one `mc.sampleseq2` node, renders it, then inspects `Runtime::snapshot()` for an inlet with `outletChannel == 1` — directly exercising Task 1's hash fix (without it, both `mul` nodes collapse into one, and the channel-1 connection to `mc.sampleseq2` is lost). Verified: `nodesAdded=7, edgesAdded=8, propsWritten=8, result=Ok`, and the channel-1 inlet is present.

- [ ] **Step 1: Append this test to `tests/CoreRendererTests.cpp`**

```cpp
TEST(NativeRendererSnapshotTests, McHashingReflectsOutputChannelFromChildNodes) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    runtime->addSharedResource("/v/path", std::make_unique<elem::AudioBufferResource>(2, 512));
    elem::Renderer<float> renderer(runtime);

    auto channels = elem::lib::sampleseq2(
        elem::lib::MCSampleSeq2Props{
            .path = std::string("/v/path"),
            .seq = {{.value = 1.0, .time = 0.0}},
            .duration = 2.0,
        },
        2.0,
        1.0
    );

    std::vector<elem::lib::ElemNode> muls;
    for (auto& channel : channels) {
        muls.push_back(elem::lib::mul({0.5, elem::lib::ElemNode(channel)}));
    }

    const auto result = renderer.renderGraph({elem::lib::add(std::move(muls))});

    const auto snapshotJson = elem::js::serialize(elem::js::Value(runtime->snapshot()));
    elem::test::verifyGraphSnapshot("McHashingReflectsOutputChannelFromChildNodes", snapshotJson);

    EXPECT_EQ(result.nodesAdded, 7);
    EXPECT_EQ(result.edgesAdded, 8);
    EXPECT_EQ(result.propsWritten, 8);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());

    // Demonstrates that both `mul` nodes above get visited/created independently during
    // traversal (they have different hashes because they address different output channels
    // of the same mc.sampleseq2 child), so the connection to the second output channel
    // survives in the rendered graph.
    const auto snapshot = nlohmann::json::parse(snapshotJson);
    bool foundChannelOneInlet = false;
    for (auto const& [nodeId, node] : snapshot.items()) {
        for (auto const& inlet : node.value("inlets", nlohmann::json::array())) {
            if (static_cast<int>(inlet.value("outletChannel", 0.0)) == 1) {
                foundChannelOneInlet = true;
            }
        }
    }
    EXPECT_TRUE(foundChannelOneInlet);
}
```

- [ ] **Step 2: Rebuild and run just this test**

```bash
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.McHashingReflectsOutputChannelFromChildNodes
```

Expected: PASSES.

- [ ] **Step 3: Commit**

```bash
git add tests/CoreRendererTests.cpp tests/snapshots/McHashingReflectsOutputChannelFromChildNodes.snapshot.json tests/snapshots/McHashingReflectsOutputChannelFromChildNodes.snapshot.md
git commit -m "Port mc.test.js 'hashing reflects outputChannel from child nodes' to NativeRendererTests"
```

---

## Task 11: Create `tests/HashingRendererTests.cpp`

**Files:**
- Create: `tests/HashingRendererTests.cpp`
- Modify: `tests/CMakeLists.txt` (add the new source file to `add_executable`)

**Interfaces:**
- Consumes: `elem::lib::cycle`, `elem::lib::mul`, `elem::lib::add`, `elem::lib::blepsaw`, `elem::lib::blepsquare`, `elem::lib::train` (`elem/lib/Oscillators.h`), `elem::lib::adsr` (`elem/lib/Envelopes.h`), `elem::lib::lowpass` (`elem/lib/Filters.h`).

**Context:** Port of `hashing.test.js`'s two tests, using the normal `renderGraph` + `verifyGraphSnapshot` pattern rather than the JS-only mask-id/delegate mechanism (per the approved spec — the C++ `Renderer` has no delegate abstraction to hook into hash-independence checking). What's ported is the graph construction and successful rendering of both graphs, including the more complex composed synth-voice graph (`adsr`, `lowpass`, `blepsaw`, `blepsquare`, `train`) exercised end-to-end for the first time in this test suite.

Note: the `seq` node in the second test is built via `SymbolicGraph::createNode` directly, not `elem::lib::seq()`. `elem::lib::seq()`'s `SeqProps.seq` field is `Required<js::NumberArray>`, which serializes to a `js::Value` holding the `NumberArray` variant — but the native `SequenceNode::setProperty`'s "seq" handler requires `val.isArray()` (the `Array`-of-`Value` variant), so `elem::lib::seq()`'s "seq" prop is rejected at runtime by the real node (confirmed empirically: `renderGraph` returns `ReturnCode::InvalidPropertyType()`). This is a pre-existing bug in `Core.h` unrelated to this plan's scope (it's the first time `elem::lib::seq()` is exercised against a real `Runtime` in this codebase) — not fixed here; this test sidesteps it by constructing the node directly with a properly-typed `js::Array`, matching the precedent already set in Task 3.

- [ ] **Step 1: Write `tests/HashingRendererTests.cpp`**

```cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/SymbolicGraph.h"

#include "GraphSnapshotTestUtils.h"
#include "elem/lib/Core.h"
#include "elem/lib/Envelopes.h"
#include "elem/lib/Filters.h"
#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

// Port of js/packages/core/__tests__/hashing.test.js.
//
// The JS tests use a custom HashlessRenderer that hooks into the JS-only renderWithDelegate
// abstraction, replacing real hash values with sequential "mask ids" so the tests can assert
// on instruction *shape* independent of the hashing algorithm. The C++ Renderer has no
// delegate abstraction -- it always uses real NodeId hashes directly, with no equivalent hook
// point. That specific hash-independence property is NOT ported/verified here.
//
// What IS ported: the two graphs these tests build are rendered and snapshotted using the
// normal renderGraph + verifyGraphSnapshot pattern, exercising this more complex composed
// graph construction (adsr, lowpass, seq, blepsaw, blepsquare, train) end-to-end.

TEST(NativeRendererSnapshotTests, HashingCycleGraph) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({elem::lib::cycle(440.0)});

    elem::test::verifyGraphSnapshot(
        "HashingCycleGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, HashingComposedSynthVoiceGraph) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    auto synthVoice = [](elem::lib::ElemNode hz) {
        return elem::lib::mul({
            0.25,
            elem::lib::add({
                elem::lib::blepsaw(elem::lib::mul({hz, 1.001})),
                elem::lib::blepsquare(elem::lib::mul({hz, 0.994})),
                elem::lib::blepsquare(elem::lib::mul({hz, 0.501})),
                elem::lib::blepsaw(elem::lib::mul({hz, 0.496})),
            }),
        });
    };

    auto train = elem::lib::train(4.8);

    std::vector<double> arpSteps = {0, 4, 7, 11, 12, 11, 7, 4};
    std::vector<elem::js::Number> arp;
    for (double step : arpSteps) {
        arp.push_back(261.63 * 0.5 * std::pow(2.0, step / 12.0));
    }

    auto modulate = [](elem::lib::ElemNode x, elem::lib::ElemNode rate, elem::lib::ElemNode amt) {
        return elem::lib::add({x, elem::lib::mul({amt, elem::lib::cycle(rate)})});
    };

    auto env = elem::lib::adsr(0.01, 0.5, 0.0, 0.4, train);

    auto filt = [&](elem::lib::ElemNode x) {
        return elem::lib::lowpass(
            elem::lib::add({40.0, elem::lib::mul({modulate(1840.0, 0.05, 1800.0), env})}),
            1.0,
            x
        );
    };

    // Built directly (not via elem::lib::seq()) to work around the Core.h bug described
    // above: the native SequenceNode requires a js::Array for "seq", not a NumberArray.
    auto seqNode = elem::SymbolicGraph::createNode(
        "seq",
        elem::js::Object{{"seq", elem::js::Array(arp.begin(), arp.end())}, {"hold", true}},
        elem::lib::resolve({train, 0.0})
    );

    auto out = elem::lib::mul({0.25, filt(synthVoice(seqNode))});

    const auto result = renderer.renderGraph({out, out});

    elem::test::verifyGraphSnapshot(
        "HashingComposedSynthVoiceGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}
```

- [ ] **Step 2: Add the new file to the test target**

In `tests/CMakeLists.txt`, change:

```cmake
add_executable(native-renderer-tests EXCLUDE_FROM_ALL
    NativeRendererTests.cpp
    CoreRendererTests.cpp
)
```

to:

```cmake
add_executable(native-renderer-tests EXCLUDE_FROM_ALL
    NativeRendererTests.cpp
    CoreRendererTests.cpp
    HashingRendererTests.cpp
)
```

- [ ] **Step 3: Reconfigure, build, and run both new tests**

```bash
cmake -S . -B build
cmake --build build --target native-renderer-tests -j 8
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.Hashing*
```

Expected: both PASS (fixtures auto-written on this first run). If `train` alone (used both as a variable name and as `elem::lib::train`) causes a shadowing/ambiguity compile error, rename the local variable to `trainSignal` and update its two uses (`adsr(..., trainSignal)`, `seq(..., trainSignal, ...)`).

- [ ] **Step 4: Run the full suite to confirm no regressions across all tasks**

```bash
./build/tests/native-renderer-tests
```

Expected: every test in the binary PASSES (the 2 original + 9 from `CoreRendererTests.cpp` + 2 from `HashingRendererTests.cpp` = 13 total).

- [ ] **Step 5: Commit**

```bash
git add tests/HashingRendererTests.cpp tests/CMakeLists.txt tests/snapshots/HashingCycleGraph.snapshot.json tests/snapshots/HashingCycleGraph.snapshot.md tests/snapshots/HashingComposedSynthVoiceGraph.snapshot.json tests/snapshots/HashingComposedSynthVoiceGraph.snapshot.md
git commit -m "Port hashing.test.js graphs to NativeRendererTests"
```
