#pragma once

#include <numeric>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "HashUtils.h"
#include "Types.h"
#include "Value.h"


namespace elem {
    using OutputChannel = uint32_t;

    struct SymbolicGraphNodeShallow {
        NodeId hash;
        std::string kind;
        js::Object props;
        OutputChannel outputChannel;
    };

    /**
     * The symbolic representation of an Elementary Audio Graph.
     * This represents the structure without being coupled to the implementation of
     * the audio engine, and is used to describe the desired state that the renderer
     * should realize.
     */
    // TODO: Remove copy constructor and assigment
    struct SymbolicGraphNode : SymbolicGraphNodeShallow {
        std::vector<SymbolicGraphNode> children;
    };

    namespace SymbolicGraph {
        static SymbolicGraphNode createNode(std::string kind, js::Object props, std::vector<SymbolicGraphNode> children) {
            std::vector<NodeId> childHashes;
            childHashes.reserve(children.size());
            for (const auto& child: children) {
                childHashes.push_back(child.hash);
            }

            return SymbolicGraphNode{
                HashUtils::hashNode(kind, props, childHashes),
                std::move(kind),
                std::move(props),
                0,
                std::move(children)
            };
        }
    }
}

