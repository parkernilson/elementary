# Porting core.test.js / mc.test.js / lib.test.js / hashing.test.js to NativeRendererTests

## Context

The C++ native renderer (`runtime/elem/Renderer.h`, `Runtime.h`, `SymbolicGraph.h`) is a
from-scratch reimplementation of the JS reconciliation logic in
`js/packages/core/src/Reconciler.res`. `tests/NativeRendererTests.cpp` has two example tests
(`RendersBasicSineWave`, `NumericLiteralIsResolvedToConstantNode`) establishing the pattern:
build a graph with `elem::lib::*` helpers, call `Renderer::renderGraph`, assert on the returned
`RenderResult` counts (`nodesAdded`/`edgesAdded`/`propsWritten`/`result`), and diff a JSON
snapshot of `Runtime::snapshot()` against a committed fixture via `verifyGraphSnapshot`
(auto-written on first run or when `UPDATE_SNAPSHOTS=1`).

This spec covers porting the remaining JS tests in `core.test.js`, `mc.test.js`,
`lib.test.js`, and `hashing.test.js` to this same idiom.

## Renderer.h changes

Two targeted changes to `runtime/elem/Renderer.h`, required to make the ported tests
meaningful (not just test infrastructure):

1. **Bug fix in `mount()`**: the `appendChild` instruction is built with
   `makeAppendChildInstruction(node.hash, child->hash, node.outputChannel)` — using the
   *parent* node's own output channel for every child edge, instead of the *child's*.
   Confirmed against `Reconciler.res` line 53
   (`RenderDelegate.appendChild(delegate, node.hash, child.hash, child.outputChannel)`),
   which correctly uses `child.outputChannel`. Without this fix, a multi-channel node's
   non-zero output channels (e.g. channel 1 of an `unpack()`'d `mc.sampleseq2`) can never be
   wired correctly to a parent. Fix: change to `child->outputChannel`. No snapshot changes
   are expected for the two existing tests, since neither uses multi-channel nodes (channel
   is always 0 by construction).

2. **`NodeRef::setter` return type**: currently `std::function<void(js::Object)>`. Change to
   `std::function<RenderResult(js::Object)>`, reusing the `RenderResult` struct already
   returned by `renderGraph`. The setter's build-and-apply-instructions body already
   constructs an `InstructionBatch`; compute `propsWritten` from its size, `nodesAdded`/
   `edgesAdded` are always 0 (a setter only ever updates props), and `result` is the return
   of `runtime->applyInstructions(...)`. If the runtime's `weak_ptr` has expired or the node
   isn't found, return a `RenderResult` with `result = ReturnCode::NodeNotFound()`. This lets
   the ported `refs` test assert on setter effects the same way other tests assert on
   `renderGraph`'s output, without needing a lower-level instruction-batch inspection API.

No other consumers of `NodeRef`/`createRef` exist in the codebase, so this is a safe,
localized signature change.

## File layout

- `tests/CoreRendererTests.cpp` — port of the 7 not-yet-covered tests from `core.test.js`,
  plus the 1 test from `mc.test.js`. A short comment block near the top explains why
  `lib.test.js` is not ported (see below).
- `tests/HashingRendererTests.cpp` — port of the 2 tests from `hashing.test.js`, using the
  normal `renderGraph` + `verifyGraphSnapshot` pattern.
- Both files added to `tests/CMakeLists.txt`'s `add_executable(native-renderer-tests ...)`
  source list.

## Skipped: lib.test.js

`lib.test.js`'s single test, `errors on graph construction`, checks two JS runtime throws:

1. `el.seq({}, 1)` — missing a required argument.
2. `el.mul(1, 2, '4')` — a string where a number is expected.

Both are compile-time-enforced in C++: `elem::lib::seq` has fixed arity (props, trigger,
reset), and `ElemNode` is `std::variant<NodeRepr, js::Number>` with no implicit conversion
from string literals. Neither condition is expressible as a GTest runtime assertion. This
file is not ported; a comment documents why.

## Skipped: hashing.test.js's mask-id mechanism

`hashing.test.js` uses a custom `HashlessRenderer` that hooks into the JS-only
`renderWithDelegate` abstraction, replacing real hash values with sequential "mask ids" so
tests can assert on instruction *shape* independent of the hashing algorithm. The C++
`Renderer` has no delegate abstraction — it always uses real `NodeId` hashes directly, with
no equivalent hook point. This specific hash-independence property is not ported/verified.

What *is* ported: the two graphs these tests build (`el.cycle(440)`, and the more complex
composed synth-voice graph using `adsr`, `lowpass`, `seq`, `blepsaw`, `blepsquare`, `train`)
are rendered and snapshotted using the existing `renderGraph` + `verifyGraphSnapshot`
pattern, exercising this more complex graph construction end-to-end. A comment in
`HashingRendererTests.cpp` documents that the hash-independence property specifically is not
covered.

## Test-by-test mapping (core.test.js + mc.test.js)

| JS test | C++ approach |
|---|---|
| `the basics`, `numeric literals` | Already covered by existing `RendersBasicSineWave` / `NumericLiteralIsResolvedToConstantNode`. Not duplicated. |
| `distinguish by props` | `renderGraph({voice1, voice2})` — 2 roots sharing a pulse-train subtree (`le`/`phasor`/const nodes, unkeyed, identical props/children) but distinct `sample`/`seq` nodes (different `seq` prop). Assert `RenderResult` counts + snapshot. |
| `multi-channel basics` | `renderGraph({monoProcess, monoProcess})` — identical subtree rendered as two roots; only the 2 root nodes differ. Assert counts + snapshot. |
| `simple sharing` | Two sequential `renderGraph` calls on the same `Renderer`. First renders the base tree; second wraps it in `tanh`. Assert the second call's `RenderResult` shows exactly 1 node/1 edge/0 props added (only the new `tanh` node), confirming the rest of the tree was structurally shared. Snapshot of final state. |
| `distinguished subtrees by key` | Single render: `add` of 4 keyed voices (`fq1`..`fq4`, same freq). Assert counts + snapshot. |
| `structural equality with value change` | Two sequential renders of the same 4-voice graph; second changes `voices[0].freq` (441 instead of 440) but keeps its key. Assert the second render is a full no-op (`nodesAdded==0, edgesAdded==0, propsWritten==0`), confirming key-based hashing (via `HashUtils::hashProps`) ignores the value change. |
| `switch and switch back` | Three sequential renders: voice A (key `hi`), voice B (key `bye`), voice A again. Assert the third render adds no new nodes/edges (A's subtree was never GC'd — this codebase never calls `Runtime::gc()` automatically, matching JS's default behavior), while still confirming the root-activation result is `Ok`. |
| `refs` | `createRef` + call the returned setter with new props. Assert the returned `RenderResult` has `propsWritten==1, nodesAdded==0, edgesAdded==0, result==ReturnCode::Ok()`. |
| `mc.test.js`: hashing reflects outputChannel from child nodes | Build `add(...sampleseq2(...).map(mul))` using `elem::lib::mc::sampleseq2`/`unpack`, render, then inspect `runtime->snapshot()` (parsed as `nlohmann::json`, same approach as `GraphSnapshotTestUtils`) for at least one inlet with `outletChannel == 1`. This directly exercises the Renderer.h bug fix above. |

## Verification approach for count-bearing assertions

For each test asserting exact `nodesAdded`/`edgesAdded`/`propsWritten` values (matching the
style of the two existing example tests), the counts will be determined by building and
running the test once, reading the actual `RenderResult` values, then hardcoding them as
`EXPECT_EQ`. Snapshot fixtures auto-generate on first run per the existing `verifySnapshot`
behavior (no fixture yet, or `UPDATE_SNAPSHOTS=1`, writes the fixture and passes).

## Out of scope

- Any GC/pruning behavior (`Runtime::gc()`) — none of the ported JS tests call anything
  GC-related; this codebase's default (never auto-GC) already matches JS's observed
  behavior for these tests.
- Fixing or replicating the mask-id/delegate abstraction from `hashing.test.js`.
- Any change to `elem::lib::*` node-construction helpers — this is a test-only + one
  `Renderer.h` bug-fix change.
