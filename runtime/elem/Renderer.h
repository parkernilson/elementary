#pragma once
#include "Runtime.h"
#include "SymbolicGraphNode.h"

// TODO: We should create an elemcli-native target that uses the native renderer so that these files
// are included in an actual compiled target, and to show what it looks like as an example.

namespace elem {
    // TODO: Would this be better as a class if we need the other things that the JS renderer provides?
    namespace Renderer {
        // TODO: Implement the graph reconciliation
        // TODO: How is this going to work with event polling and createRef
        void renderGraph(std::shared_ptr<Runtime<float>> runtime, SymbolicAudioGraph graph);
    };
}
