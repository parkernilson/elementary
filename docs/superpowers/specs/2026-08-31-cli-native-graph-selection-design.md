# cli-native: usage, --list, and pluggable graph registry

## Problem

`cli-native` (the `elemcli-native` executable) currently hardcodes a single
graph (`HelloSine`) directly in `RealtimeMain.cpp`'s `main()`. There is no
usage/help output, no way to discover what graphs are available, and no way
to select a different graph without editing `RealtimeMain.cpp`. Adding a new
graph requires touching the entry point itself.

## Goals

- `elemcli-native --list` prints the available graphs (name + one-line
  description).
- `elemcli-native --graph <name>` renders the named graph.
- `elemcli-native` with no arguments, or `--help`/`-h`, prints usage and
  exits without starting the audio device.
- Adding a new graph should not require editing `RealtimeMain.cpp`.

## CLI interface

- `--list` — prints each registered graph's name and description, one per
  line (`name — description`), then exits 0.
- `--graph <name>` — looks up `<name>` in the graph registry and renders it.
  If `<name>` is not found, prints an error and the list of valid names to
  stderr, then exits 1.
- `--help` / `-h` — prints usage text (program name, available flags, an
  example invocation) and exits 0.
- No arguments — prints the same usage text to stderr and exits 1 (an
  explicit graph must always be requested; there is no default graph).

## Graph registry

New files `cli-native/graphs/GraphRegistry.h` and
`cli-native/graphs/GraphRegistry.cpp`:

```cpp
// GraphRegistry.h
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "elem/SymbolicGraph.h"

namespace elem::lib {
    struct GraphInfo {
        std::string name;
        std::string description;
        std::function<std::vector<std::shared_ptr<elem::SymbolicGraphNode>>()> build;
    };

    std::vector<GraphInfo> const& getGraphRegistry();
}
```

`GraphRegistry.cpp` defines `getGraphRegistry()` returning a hardcoded
`static const std::vector<GraphInfo>` literal, with one entry per graph,
e.g.:

```cpp
{"hello-sine", "A single 440Hz sine tone at 0.3 gain", buildHelloSineGraph}
```

Graph names are kebab-case identifiers distinct from the C++
`build...Graph` function names.

## File organization

- `cli-native/HelloSine.h` / `.cpp` move to `cli-native/graphs/HelloSine.h`
  / `.cpp` (contents unchanged).
- New `cli-native/graphs/GraphRegistry.h` / `.cpp`.
- `CMakeLists.txt`: the `elemcli_native_graph` static library's source list
  becomes `graphs/HelloSine.cpp graphs/GraphRegistry.cpp` (explicit list,
  matching the existing style — no globbing).

**Adding a new graph** going forward means:
1. Create `graphs/MyGraph.h` / `.cpp` exposing a
   `buildMyGraph()` function (same pattern as `HelloSine`).
2. Add one entry to the `std::vector<GraphInfo>` in `GraphRegistry.cpp`.
3. Add `graphs/MyGraph.cpp` to the source list in `CMakeLists.txt`.

No changes to `RealtimeMain.cpp` are needed to add a graph.

## `RealtimeMain.cpp` changes

- Parse `argv` for `--list`, `--graph <name>`, `--help`/`-h` before touching
  the audio device.
- `--help`/`-h` or no args: print usage, exit (0 for `--help`, 1 for no
  args).
- `--list`: iterate `getGraphRegistry()`, print `name — description` for
  each, exit 0.
- `--graph <name>`: search `getGraphRegistry()` for a matching `name`. If
  found, call its `build` function in place of today's hardcoded
  `elem::lib::buildHelloSineGraph()` call and proceed with the existing
  device-init/render/start flow. If not found, print an error naming the
  bad value and listing valid graph names to stderr, exit 1.
- All other existing behavior (device setup, render stats printing,
  press-Enter-to-exit) is unchanged.

## Out of scope

- No support for per-graph runtime parameters (e.g. frequency, gain) via
  CLI flags — a graph is a fixed, parameterless build function.
- No change to the audio device selection, sample rate, or block size
  logic.
- No self-registering/macro-based registration mechanism — the registry is
  a single hand-maintained list, matching the project's preference for
  explicit, simple code.
