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
    };

    namespace SymbolicGraph {

        // TODO: I think we probably don't need a createRef function (or if we do, it could just call
        // createNode under the hood), we just need to set "key" on the props, and then provide a way to
        // setProperty nodeKey, propKey, propValue from the client.
        // Can we assume that props.key will always override the hashing? Is that an assumption we can make?

        static std::shared_ptr<SymbolicGraphNode> createNode(std::string kind, js::Object props,
                                            std::vector<std::shared_ptr<SymbolicGraphNode>> children) {
            std::vector<NodeId> childHashes;
            childHashes.reserve(children.size());
            for (const auto &child: children) {
                // A node's hash must depend not just on each child's hash, but also on the
                // outputChannel being addressed on that child. Otherwise two nodes referencing
                // different outputs of the same multi-channel child would hash identically,
                // even though they represent different signal paths. Matches NodeRepr.res.
                childHashes.push_back(HashUtils::mixNumber(child->hash, child->outputChannel));
            }

            return std::make_shared<SymbolicGraphNode>(
                HashUtils::hashNode(kind, props, childHashes),
                std::move(kind),
                std::move(props),
                0,
                std::move(children)
            );
        }

        static std::vector<std::shared_ptr<SymbolicGraphNode>> unpack(const std::shared_ptr<SymbolicGraphNode>& node, const int numChannels) {
            std::vector<std::shared_ptr<SymbolicGraphNode>> siblings;
            siblings.reserve(numChannels);

            node->outputChannel = 0;
            for (int i = 1; i < numChannels; i++) {
                auto sibling = std::make_shared<SymbolicGraphNode>(*node);
                sibling->outputChannel = i;
                siblings.push_back(sibling);
            }

            return siblings;
        }

        // TODO: createRef
    }
}

