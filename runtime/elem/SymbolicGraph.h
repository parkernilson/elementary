#pragma once

#include <numeric>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "HashUtils.h"
#include "Types.h"
#include "Value.h"


    namespace elem {
        struct SymbolicGraphNode {
            NodeId hash;
            std::string kind;
            js::Object props;
            int outputChannel;
            std::vector<NodeId> children;
        };

        /**
         * The symbolic representation of an Elementary Audio Graph.
         * This represents the structure without being coupled to the implementation of
         * the audio engine, and is used to describe the desired state that the renderer
         * should realize.
         */
        struct SymbolicAudioGraph {
            std::unordered_map<NodeId, SymbolicGraphNode> nodes;
            // TODO: Should this be called roots? That's a little confusing because we
            // wrap the root nodes in Root nodes in the render method...
            std::vector<SymbolicGraphNode> roots;
        };

        namespace SymbolicGraph {
            static SymbolicGraphNode createNode(std::string kind, js::Object props, const std::vector<SymbolicGraphNode>& children) {
                auto childHashes = children
                   | std::views::transform([](const auto& child){return child.hash;})
                   | std::ranges::to<std::vector<NodeId>>();

                return SymbolicGraphNode{
                    HashUtils::hashNode(kind, props, childHashes),
                    std::move(kind),
                    std::move(props),
                    0,
                    std::move(childHashes)
                };
            }
        }
    }

