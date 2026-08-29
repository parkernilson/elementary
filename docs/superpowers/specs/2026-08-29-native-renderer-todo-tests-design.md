# NativeRendererTests TODO coverage

## Context

`tests/NativeRendererTests.cpp` ports the JS reconciliation test suite
(`js/packages/core/__tests__/core.test.js` etc.) to C++, using
`elem::Renderer<float>::renderGraph` + `elem::test::verifyGraphSnapshot` +
assertions on `RenderResult::{nodesAdded,edgesAdded,propsWritten,result}`.
The file ends with a block of TODO comments describing untested behavior.
None of these TODOs have a JS analog to port — they're net-new C++-only
tests, designed from first principles using the same reasoning pattern as
the existing tests (particularly `StructuralEqualityWithValueChange` and
`SwitchAndSwitchBack`).

Renderer reconciliation is hash-based: each node's hash is a function of its
`kind`, `props`, and its children's hashes (`SymbolicGraph::createNode`).
`renderGraph` walks the new graph and, per node, either finds an existing
node with the same hash (share, only write changed props) or creates a new
one. Because a child's hash feeds into its parent's hash, any prop change on
an unkeyed node changes that node's hash and every unkeyed ancestor's hash
transitively, while leaving unrelated subtrees (found by hash) shared.

`Runtime::gc()` is manual — nothing calls it automatically, and no existing
test calls it. It prunes any node not referenced by the live render sequence
and returns the set of pruned node ids.

## Scope

Four new `TEST(NativeRendererSnapshotTests, ...)` cases appended to
`tests/NativeRendererTests.cpp`, replacing four of the six TODO comments.
The two custom-node TODOs are out of scope for this work and are
consolidated into a single forward-looking TODO instead of being
implemented.

## Tests

### 1. `ChangingLeafPropRecreatesWholeTree`

Build an unkeyed graph where a leaf's prop feeds into every ancestor:
`sin(mul({constant(2*PI), phasor(constant(440.0))}))`. Render, then
re-render with the frequency literal changed (e.g. 441.0). Because nothing
is keyed, the changed leaf's hash changes, cascading through `phasor` ->
`mul` -> `sin` -> `root`. Assert `nodesAdded`/`edgesAdded` reflect all of
those nodes being recreated, while the untouched `2*PI` constant is found
and shared (contributes 0).

### 2. `ChangingMiddleNodeRedrawsOnlyParents`

Build `add({ mul({gain, sin(phasor(440.0))}), otherVoice })`, unkeyed.
Re-render with `gain` changed on the `mul` node. Assert:
- The `sin`/`phasor` subtree below `mul` is found/shared (0 new nodes from
  that branch).
- `mul` itself and its ancestors (`add`, `root`) are recreated.

This demonstrates recreation propagates upward from the changed node, not
downward.

### 3. `AddingMiddleNodeRedrawsOnlyParents`

Build a chain where a subtree `C` is nested under `B` under `A`
(e.g. `A = mul(k, B)`, `B = sin(C)`, `C = phasor(440.0)`). Re-render with a
new node `D` spliced in between `B` and `C` (e.g. `B' = sin(mul(2.0, C))`
inserting `D = mul(2.0, C)`). Assert `C` is found/shared (0 new), `D` is
newly created, and `B`/`A` are recreated since their hashes changed by the
inserted child.

### 4. `GcCleansUpUnusedNodes`

Mirror `SwitchAndSwitchBack`'s setup: render keyed voice "hi", then switch
to keyed voice "bye". Instead of switching back (which demonstrates nodes
are *never* GC'd), call `runtime->gc()` after switching away from "hi".
Assert the returned pruned node-id set is non-empty and, via
`runtime->snapshot()`, that voice "hi"'s nodes are no longer present. This
contrasts directly with the existing `SwitchAndSwitchBack` test.

### TODO consolidation

Replace:
```
// TODO: Custom node tests
// TODO: Custom node is created successfully
```
with:
```
// TODO: Create a test that exercises a custom node
```
The `// TODO: use gc() in tests (...)` TODO was removed—the new GC test directly
addresses what that TODO was asking for by demonstrating gc() usage in a
realistic scenario.

## Snapshot fixtures

Each of tests 1–4 that calls `verifyGraphSnapshot` needs a corresponding
`tests/snapshots/<Name>.snapshot.json` (+ `.md` mermaid diagram) generated
by building and running the suite once with `UPDATE_SNAPSHOTS=1`, then
reviewing the generated fixture for correctness before committing — per
`SnapshotTestUtils.h`'s existing fixture-generation behavior. Numeric
assertions (`nodesAdded` etc.) are taken from the real observed
`RenderResult` of that run, not hand-computed.

## Testing

Build and run `NativeRendererTests` (find the CMake/build target used by
the existing suite) both to generate fixtures and to confirm all new and
existing tests pass.
