# Native Renderer Graph Reconciliation

## Context

`elem::Renderer<FloatType>` (`runtime/elem/Renderer.h`) is the C++/native counterpart
to the JS core package's reconciler (`js/packages/core/src/Reconciler.res`,
`NodeRepr.res`, `HashUtils.res`, `Hash.ts`). It exists so that platforms with direct
access to the C++ layer can drive an `elem::Runtime<FloatType>` without going through
a JS engine at all.

`Renderer::renderGraph` is currently a stub. This spec describes the algorithm it
should implement: turning a `SymbolicAudioGraph` (a plain tree description of the
desired audio graph) into a batch of `RuntimeInstructionType` instructions
(`CREATE_NODE`, `APPEND_CHILD`, `SET_PROPERTY`, `ACTIVATE_ROOTS`, `COMMIT_UPDATES`)
applied to the `Runtime`, reusing existing nodes across renders wherever their
structural hash matches a previously-mounted node.

This is a same-process, in-memory port. There is no message-passing/serialization
boundary between `Renderer` and `Runtime` the way there is between the JS `Renderer`
and a native engine over IPC/threads — `renderGraph` calls
`runtime->applyInstructions(...)` directly.

## Goals

- Match the reconciliation semantics of the JS core renderer: nodes are identified by
  a structural hash of (type, props, child hashes + output channels); revisiting an
  equal-hash node updates only changed props; new nodes get created and wired up via
  `appendChild`.
- Hashing is owned entirely by `Renderer` (not stored on `SymbolicGraphNode`), and is
  recomputed on demand during each `renderGraph` call. `SymbolicGraphNode` remains a
  plain structural description with no hash field.
- Per-call traversal state (the instruction batch, the visited-hash set) is not
  `Renderer` member state — it is constructed fresh in `renderGraph` and threaded
  through the recursive traversal by parameter. Only `nodeMap` (needed to persist
  reconciliation state *across* separate `renderGraph` calls) remains a `Renderer`
  member.

## Non-goals

- Skipping `ACTIVATE_ROOTS` emission when the requested roots are already active
  (existing TODO in `Renderer.h`) is out of scope for this change.
- Bit-for-bit hash parity with the JS implementation's hash values is not required;
  only parity of *semantics* (structural identity → same treatment) is required,
  since the native renderer talks to its own independent `Runtime`/`nodeMap`, never
  to a JS-side node map.
- GC/pruning of stale nodes from `nodeMap` is not addressed here.

## Data model change

`SymbolicGraphNodeShallow` (`runtime/elem/SymbolicGraph.h`) gains an `outputChannel`
field, mirroring `NodeRepr.shallow.outputChannel` in the JS implementation. This is
necessary because a node's hash must depend on which output channel of a child is
being addressed — two references to different output channels of the same child
node represent different signal paths and must hash differently.

```cpp
struct SymbolicGraphNodeShallow {
    std::string type;
    std::unordered_map<std::string, js::Value> props;
    int outputChannel = 0;
};
```

## Hashing

Ported from `HashUtils.res`'s FNV-1a-style scheme, as private helpers used only by
`Renderer`. Implemented with `uint32_t` for well-defined wraparound multiplication
(avoiding both signed-integer-overflow UB and the JS version's incidental float64
precision loss for very large intermediate values — a latent quirk of the original
that we are not required to reproduce, since we're not required to match hash values
bit-for-bit).

- `mixNumber(seed, n)` — `(seed XOR n) * 0x01000193`, direct port of the FNV mixing
  step.
- `hashString(seed, s)` — folds each character of `s` into `seed` via `mixNumber`.
- `finalizeHash(n)` — masks off the sign bit (`n & 0x7fffffff`) so results are always
  non-negative, matching the JS `finalize` step.
- `hashProps(seed, props)`:
  - If `props` contains a `"key"` entry that is a string, hash *only* that string
    (matches the JS special case that lets callers opt into cheap identity/memo
    hashing via an explicit `key` prop, bypassing full prop serialization).
  - Otherwise, build a sorted `js::Object` (`std::map`, so iteration order is
    deterministic) from the node's `unordered_map<std::string, js::Value>` props,
    serialize it with the existing `elem::js::serialize` (`JSON.h`), and hash the
    resulting string.

Node hashing itself is folded directly into the traversal (see below) rather than
being a separate standalone function, so that each node's hash is computed exactly
once per occurrence in the tree during the single recursive pass.

## Reconciliation algorithm

Hashing is inherently post-order: a node's hash depends on its children's hashes, so
children must be visited (and hashed) before their parent's hash can be computed.
Given that a recursive call is unavoidable for hashing, the mount/diff step is folded
into the *same* recursive pass rather than using a separate pre-order or iterative
worklist traversal — this avoids ever computing a given node occurrence's hash more
than once.

Instruction order across independent branches of the tree does not matter to the
`Runtime`; what matters is that all `CREATE_NODE` instructions are applied before any
`APPEND_CHILD`, and all `APPEND_CHILD` before any `SET_PROPERTY`-dependent state is
assumed (matching the JS `Delegate.getPackedInstructions()` bucketing). This is
achieved by accumulating instructions into an ordered `InstructionBatch` (grouped by
type) as traversal state, then concatenating the buckets in fixed order at the end —
never by relying on call-order across recursive branches.

```cpp
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
```

Traversal (pseudocode; `visited` and `batch` are passed by reference down the call
stack, not stored on `Renderer`):

```
visit(node, visited, batch) -> hash:
    childHashes = []                      // list of (hash, outputChannel)
    for child in node.children:
        childHashes.push({ hash: visit(child, visited, batch), outputChannel: child.outputChannel })

    hash = hashString(FNV_OFFSET_BASIS, node.type)
    hash = hashProps(hash, node.props)
    for (childHash, outputChannel) in childHashes:
        hash = mixNumber(hash, mixNumber(childHash, outputChannel))
    hash = finalizeHash(hash)

    if hash in visited:
        return hash                       // already mounted/updated this render pass

    visited.insert(hash)
    mount(node, hash, childHashes, batch)
    return hash

mount(node, hash, childHashes, batch):
    if hash not in nodeMap:
        batch.createNode.push(createNode(node.type, hash))
        for (key, value) in node.props:
            batch.setProperty.push(setProperty(hash, key, value))
        for (childHash, outputChannel) in childHashes:
            batch.appendChild.push(appendChild(hash, childHash, outputChannel))
        nodeMap[hash] = shallowCopy(node)   // type + props + outputChannel, no children
    else:
        existing = nodeMap[hash]
        for (key, value) in node.props:
            if !existing.props.count(key) || !valuesEqual(existing.props[key], value):
                batch.setProperty.push(setProperty(hash, key, value))
                existing.props[key] = value
```

`valuesEqual(a, b)` uses full deep equality on `js::Value` (comparing
`elem::js::serialize(a) == elem::js::serialize(b)`), rather than JS's one-level
`shallowEqual`. This is a deliberate simplification enabled by `js::Value` already
being a value type with recursive structural equality available "for free" via
serialization — deep equality is a superset check that will never *miss* a real
change JS would have caught, but could in theory fire an extra `setProperty` in a
case where JS's shallow check would have short-circuited on nested reference
equality alone. This is expected to be a non-issue in practice since `js::Value`
props are typically small.

```cpp
// TODO: verify valuesEqual's full deep-equality semantics match the JS
// implementation's one-level shallowEqual closely enough in practice
// (e.g. for array/sequence props) — see spec at
// docs/superpowers/specs/2026-08-13-native-renderer-reconciliation-design.md
```

## Root wrapping & `renderGraph`

```cpp
void Renderer<FloatType>::renderGraph(
    SymbolicAudioGraph graph, double rootFadeInMs, double rootFadeOutMs)
{
    InstructionBatch batch;
    std::unordered_set<int> visited;
    std::vector<int> rootHashes;

    for (size_t i = 0; i < graph.graphs.size(); ++i) {
        SymbolicGraphNode root = wrapAsRoot(graph.graphs[i], (int) i, rootFadeInMs, rootFadeOutMs);
        rootHashes.push_back(visit(root, visited, batch));
    }

    runtime->applyInstructions(batch.flatten(rootHashes));
}
```

- `renderGraph`'s signature grows `rootFadeInMs`/`rootFadeOutMs` parameters, mirroring
  the JS `renderWithOptions(options, ...)` entry point (JS's plain `render()` is just
  a convenience wrapper defaulting both to 20ms; that convenience wrapper is not part
  of this native port's scope, callers pass explicit values).
- `wrapAsRoot(g, channel, fadeInMs, fadeOutMs)` synthesizes a `SymbolicGraphNode` of
  type `"root"` with `props = {"channel": channel, "fadeInMs": fadeInMs, "fadeOutMs":
  fadeOutMs}` and a single child `g` (at `g`'s own `outputChannel`), matching
  `NodeRepr.create("root", {...}, [g])` in `Reconciler.res`.
- After traversal, `activateRoots(rootHashes)` and `commitUpdates()` are appended
  (via `InstructionBatch::flatten`) and the full batch is applied to `runtime` in one
  call.
- The activate-roots-if-changed optimization remains a `// TODO` in `Renderer.h`, not
  implemented here.

## Testing

- Unit tests (new, alongside existing `runtime` test infrastructure) covering:
  - First render of a simple graph produces the expected `createNode`/
    `appendChild`/`setProperty` sequence and mounts nodes into `nodeMap`.
  - Re-rendering an identical graph produces no new `createNode`/`appendChild`
    instructions (structural hashes match) and no spurious `setProperty` calls.
  - Re-rendering with a changed prop value on an existing node produces exactly one
    `setProperty` for the changed key, none for unchanged keys.
  - A node referenced twice in the same tree (shared subtree, e.g. via a `key` prop)
    is only mounted once (`createNode` emitted once) but `appendChild` fires once per
    reference.
  - Two references to different `outputChannel`s of the same child node hash
    differently and are treated as distinct edges via `appendChild`'s
    `childOutputChannel` argument (not distinct *nodes* — the child node is mounted
    once for the shared underlying node identity, output-channel differences are
    encoded at the edge/appendChild level, matching `NodeRepr.res`'s use of
    `outputChannel` in the hash mix).
  - Multiple top-level graphs each get wrapped in their own `"root"` node with the
    correct `channel` index, and `activateRoots` includes all root hashes.
