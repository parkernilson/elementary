#pragma once

#include <unordered_map>
#include <vector>

#include "Value.h"

namespace elem {
    // TODO: Should we delete the copy constructors so that these must be moved?

    struct SymbolicGraphNode {
        std::string type;
        std::unordered_map<std::string, js::Value> props;
        std::vector<SymbolicGraphNode> children;
    };

    struct SymbolicAudioGraph {
        std::vector<SymbolicGraphNode> roots;
    };
}
