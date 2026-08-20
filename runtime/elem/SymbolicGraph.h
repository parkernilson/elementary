#pragma once

#include <ranges>
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
    struct SymbolicGraphNode : SymbolicGraphNodeShallow {
        std::vector<std::shared_ptr<SymbolicGraphNode>> children;

        SymbolicGraphNode(const NodeId hash, std::string kind, js::Object props, const OutputChannel outputChannel,
                          std::vector<std::shared_ptr<SymbolicGraphNode>> children)
            : SymbolicGraphNodeShallow{hash, std::move(kind), std::move(props), outputChannel}
              , children(std::move(children)) {
        }

        SymbolicGraphNode(SymbolicGraphNode const &) = delete;

        SymbolicGraphNode &operator=(SymbolicGraphNode const &) = delete;

        SymbolicGraphNode(SymbolicGraphNode &&) = default;

        SymbolicGraphNode &operator=(SymbolicGraphNode &&) = default;
    };

    namespace SymbolicGraph {
        static std::shared_ptr<SymbolicGraphNode> createNode(std::string kind, js::Object props,
                                            std::vector<std::shared_ptr<SymbolicGraphNode>> children) {
            std::vector<NodeId> childHashes;
            childHashes.reserve(children.size());
            for (const auto &child: children) {
                childHashes.push_back(child->hash);
            }

            return std::make_shared<SymbolicGraphNode>(
                HashUtils::hashNode(kind, props, childHashes),
                std::move(kind),
                std::move(props),
                0,
                std::move(children)
            );
        }
    }
}

