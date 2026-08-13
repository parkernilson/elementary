#pragma once

#include <unordered_map>
#include <vector>

#include "Value.h"

namespace elem {
    struct SymbolicGraphNodeShallow {
        std::string type;
        std::unordered_map<std::string, js::Value> props;

        // Identifies which output channel of this node is being addressed when it
        // is referenced as a child of another node. This is mixed into the parent's
        // structural hash so that two references to different output channels of
        // the same node produce different hashes.
        int outputChannel = 0;
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
