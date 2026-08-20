# Native Renderer Tests

Tests for the native `Renderer`/`Runtime` layer under `runtime/elem/`, using
GoogleTest and GoogleMock (vendored as a git submodule, not fetched at
configure time).

## Building and running

```sh
# first time only, if you didn't clone with --recurse-submodules
git submodule update --init tests/third-party/googletest

cmake -S . -B build
cmake --build build --target native-renderer-tests
./build/tests/native-renderer-tests
```

You can also run a subset of tests with GoogleTest's `--gtest_filter`, e.g.:

```sh
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.*
```

## Mocking the Runtime

`Renderer<FloatType>` depends on `RuntimeInterface<FloatType>` (an abstract
interface implemented by `Runtime<FloatType>`), rather than the concrete
`Runtime` class directly. This makes it possible to construct a
`Renderer` with a GoogleMock mock in place of a real `Runtime`, so you can
assert on exactly what instructions get sent (`EXPECT_CALL(*mockRuntime,
applyInstructions(...))`) without needing a fully working audio engine
underneath. See `MockRuntime` in `NativeRendererTests.cpp` for an example.

Tests that care about realized graph state (see "Snapshot tests" below)
use a real `Runtime` instead, since the point there is to verify what the
`Runtime` actually does with the instructions it receives.

## Snapshot tests

Some behaviors of the renderer/runtime — like the full shape of the graph
after a render pass, including every node's props and connections — are
tedious and brittle to assert field-by-field. For these, we use a simple
snapshot/approval-testing pattern via `SnapshotTestUtils.h`.

### What gets snapshotted

`Runtime<FloatType>::snapshot()` returns a `js::Object` describing every
node currently in the runtime's internal graph, keyed by hex node ID:

```json
{
  "0x31dcdf01": {
    "kind": "const",
    "props": { "value": 42.0 },
    "inlets": [ { "source": "0x...", "outletChannel": 0 } ],
    "outlets": [ { "destination": "0x...", "outletChannel": 0 } ]
  }
}
```

- `kind` — the node type string (`"const"`, `"phasor"`, etc.)
- `props` — whatever's been set via `setProperty`
- `inlets`/`outlets` — the node's actual graph connections (source/destination
  node ID + channel)

This snapshot reflects the *realized* state of the `Runtime` after
instructions have been applied — not just what the `Renderer` intended to
send. That distinction matters: it catches bugs where the `Renderer` builds
the wrong instructions, or where the `Runtime` misapplies them.

### Writing a snapshot test

```cpp
auto const serialized = elem::js::serialize(elem::js::Value(runtime->snapshot()));
elem::test::verifySnapshot("SomeTestName", serialized);
```

`verifySnapshot` looks up `tests/snapshots/SomeTestName.snapshot.json`:

- **Fixture missing**, or the `UPDATE_SNAPSHOTS=1` environment variable is
  set: pretty-print the actual output and (re)write the fixture. The test
  passes. This is how you accept a new or changed snapshot.
- **Fixture exists, no update flag**: pretty-print the actual output the
  same way and compare it against the fixture's contents with `EXPECT_EQ`.
  Any difference fails the test with a message telling you to rerun with
  `UPDATE_SNAPSHOTS=1`.

### Workflow

1. Write the test calling `verifySnapshot("Name", ...)`. The first run
   auto-creates the fixture (there's nothing to compare against yet).
2. **Read the generated `tests/snapshots/Name.snapshot.json` and manually
   verify it's actually correct.** Auto-writing on a missing fixture doesn't
   validate anything — it just captures whatever came out. This step is
   what makes the test meaningful.
3. Commit the fixture alongside the test.
4. If a later change causes the test to fail, that's `EXPECT_EQ` telling you
   the realized graph state changed. If the change is intentional:
   ```sh
   UPDATE_SNAPSHOTS=1 ./build/tests/native-renderer-tests --gtest_filter=YourTest.Name
   git diff tests/snapshots/Name.snapshot.json   # review exactly what changed
   ```
   then commit the updated fixture. If the change is *not* intentional,
   you've found a regression — fix the code, not the fixture.

### When to use a snapshot test vs. other assertions

Snapshot tests are good for asserting the *entire* realized graph state
(every node, every prop, every connection) without hand-writing a large
assertion every time — especially when a change might have wide,
hard-to-predict ripple effects. They are not a good fit for precise
instruction-level claims like "exactly 3 `createNode` instructions were
sent this render" — for that, assert directly on the values returned by
`Renderer::renderGraph` (see `RenderStats` in `Renderer.h`) or on the mock's
recorded calls. The two approaches complement each other: use `RenderStats`/
mock assertions for what was *sent*, and snapshot tests for what the
`Runtime` ended up *with*.
