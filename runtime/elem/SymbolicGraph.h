#pragma once

#include <numeric>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "HashUtils.h"
#include "Types.h"
#include "Value.h"


namespace elem {
    struct SymbolicGraphNodeShallow {
        NodeId hash;
        std::string kind;
        js::Object props;
        // TODO: Is int appropriate or should I use a different size of int
        int outputChannel;
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
        // TODO: Make sure that all usages are efficient (correct move semantics)
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

