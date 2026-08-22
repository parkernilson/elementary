#pragma once

#include <algorithm>
#include <chrono>
#include <unordered_set>

#include "Runtime.h"
#include "RuntimeInterface.h"
#include "SymbolicGraph.h"

// TODO: We should create an elemcli-native target that uses the native renderer so that these files
// are included in an actual compiled target, and to show what it looks like as an example.

namespace elem {
    namespace JsInstructionType {
        static constexpr js::Number CREATE_NODE = static_cast<js::Number>(RuntimeInstructionType::CREATE_NODE);
        static constexpr js::Number APPEND_CHILD = static_cast<js::Number>(RuntimeInstructionType::APPEND_CHILD);
        static constexpr js::Number SET_PROPERTY = static_cast<js::Number>(RuntimeInstructionType::SET_PROPERTY);
        static constexpr js::Number ACTIVATE_ROOTS = static_cast<js::Number>(RuntimeInstructionType::ACTIVATE_ROOTS);
        static constexpr js::Number COMMIT_UPDATES = static_cast<js::Number>(RuntimeInstructionType::COMMIT_UPDATES);
    }

    struct InstructionBatch {
        js::Array createNode;
        js::Array appendChild;
        js::Array setProperty;
        js::Array activateRoots;
        js::Array commitUpdates;

        [[nodiscard]] js::Array takeBatchedInstructions() {
            js::Array instructions;
            instructions.reserve(
                createNode.size()
                + appendChild.size()
                + setProperty.size()
                + activateRoots.size()
                + commitUpdates.size()
            );
            for (auto &v: createNode) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: appendChild) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: setProperty) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: activateRoots) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: commitUpdates) {
                instructions.push_back(std::move(v));
            }
            return std::move(instructions);
        }
    };

    struct RenderOptions {
        int32_t fadeInMs = 20;
        int32_t fadeOutMs = 20;
    };

    struct RenderResult {
        int result = 0;
        int32_t nodesAdded = 0;
        int32_t edgesAdded = 0;
        int32_t propsWritten = 0;
        double elapsedTimeMs = 0.0;
    };

    struct NodeRef {
        std::shared_ptr<SymbolicGraphNode> node;
        std::function<void(js::Object newProps)> setter;
    };

    template<typename FloatType>
    class Renderer {
    public:
        explicit Renderer(std::shared_ptr<RuntimeInterface<FloatType> > runtime);

        RenderResult renderGraph(std::vector<std::shared_ptr<SymbolicGraphNode>> graphs, RenderOptions options = {});

        NodeRef createRef(std::string kind, js::Object props, std::vector<std::shared_ptr<SymbolicGraphNode>> children);

    private:
        static js::Array makeCreateNodeInstruction(std::string kind, NodeId hash);

        static js::Array makeAppendChildInstruction(NodeId parentHash, NodeId childHash,
                                                    OutputChannel childOutputChannel);

        static js::Array makeSetPropertyInstruction(NodeId hash, std::string key, js::Value value);

        static js::Array makeActivateRootsInstruction(std::vector<NodeId> roots);

        static js::Array makeCommitUpdatesInstruction();

        // TODO: return ReturnCode?
        static void updateNodeProps(const std::shared_ptr<SymbolicGraphNodeShallow>& node, js::Object newProps, InstructionBatch &batch);

        void visit(const SymbolicGraphNode &node, InstructionBatch &batch);

        std::shared_ptr<RuntimeInterface<FloatType> > mRuntime;
        // TODO: Is it okay to store shared_ptr<SymbolicGraphNode> in nodeMap, using the same symbolic graph node's
        // that are passed to the renderGraph method?
        // I guess the unsafe thing about this would be that we don't know what the caller does with the shared_ptr's
        // after the call to renderGraph. They could continue to use them while making changes to the audio graph structure
        // or they could throw them away and use an entirely new set of shared_ptr's when doing the next render pass
        // (I think this is what the current implementation does because we call createNode in the lib helpers
        // i.e. every time you render the graph you would call `cycle(440.0)` which calls createNode under the hood)
        // Maybe we can make this contract work as long as the caller passes ownership of the shared_ptr's over to the
        // renderer when calling renderGraph
        // DECISION: The nodeMap should not be connected to any "living" audio graph via shared_ptr since it is intended
        // to represent a snapshot in time that the renderer controls so that we can get accurate diffs between the
        // current and previous renders. We cannot control what the caller does with their shared_ptr's so let's just
        // shallow copy into nodeMap and perform updates to it explicitly.
        std::unordered_map<NodeId, SymbolicGraphNodeShallow> nodeMap;
        NodeId nextRefId = 0;
    };

    template<typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<RuntimeInterface<FloatType> > runtime) : mRuntime{std::move(runtime)} {
    }

    // TODO: updates to nodes in nodeMap should be updated via nodeMap[key].props = {...}, not via shared_ptr access
    template<typename FloatType>
    void Renderer<FloatType>::updateNodeProps(const std::shared_ptr<SymbolicGraphNodeShallow>& node, js::Object newProps, InstructionBatch &batch) {
        for (auto& [key, value] : newProps) {
            if (auto oldProp = node->props.find(key); oldProp == node->props.end() || oldProp->second != value) {
                // TODO: I think the reason we use node->hash and don't recalculate a hash is because
                // the only way that we have an oldNode and newNode with different props but the same
                // hash (i.e. an "existing" node in nodeMap when we do a render pass) is if the node
                // has a "key" identity. Therefore, the node->hash is stable across differing props
                batch.setProperty.emplace_back(makeSetPropertyInstruction(node->hash, key, value));
                // TODO: technically we probably wouldn't want to commit this change to the node until we
                // successfully send a commitUpdates instruction to the runtime, or at the very least we would
                // need to roll this back.
                // we could probably have a bunch of lambda's that we can add to a updatesToCommit vector
                // in the renderer that only gets run upon successful return from applyInstructions, which we could
                // register here to update the node in nodeMap
                node->props[key] = std::move(value);
            }
        }
    }

    // TODO: This should probably be named "mount" because "visit" in js core is the mechanisms that performs the
    // DFS pre-order worklist, while "mount" is what actually generates the instructions and updates the nodeMap
    template<typename FloatType>
    void Renderer<FloatType>::visit(const SymbolicGraphNode &node, InstructionBatch &batch) {
        if (const auto &existingNode = nodeMap.find(node.hash); existingNode != nodeMap.end()) {
            // TODO: Since we are storing SymbolicGraphNodeShallow by value (and not shared_ptr<SymbolicGraphNode>),
            // we can't update the node props by shared_ptr dereferencing.
            // we either need to store shared_ptr in nodeMap or update the node via nodeMap[hash].props = {...};
            updateNodeProps(existingNode->second, node.props, batch);
        } else {
            batch.createNode.emplace_back(makeCreateNodeInstruction(node.kind, node.hash));
            for (const auto &[key, value]: node.props) {
                batch.setProperty.emplace_back(makeSetPropertyInstruction(node.hash, key, value));
            }
            for (const auto &child: node.children) {
                batch.appendChild.emplace_back(makeAppendChildInstruction(node.hash, child->hash, node.outputChannel));
            }
            // Copy the node into the nodeMap but without the recursive children
            // TODO: We probably don't need to do this anymore since children are shared_ptr's and are trivially copiable
            // and it might give the added benefit of keeping references between the nodes even in the nodeMap...
            // except that might not be what we want. We probably do want to have a conceptual disconnect between
            // the incoming symbolic graph for the render pass and the persisten nodeMap used for comparing values
            // between render passes
            nodeMap[node.hash] = static_cast<SymbolicGraphNodeShallow>(node);
        }
    }

    template<typename FloatType>
    RenderResult Renderer<FloatType>::renderGraph(std::vector<std::shared_ptr<SymbolicGraphNode>> graphs, const RenderOptions options) {
        auto const t0 = std::chrono::steady_clock::now();

        std::unordered_set<NodeId> visited;
        InstructionBatch instructions;

        // Wrap the roots of the graph in root-type nodes
        std::vector<std::shared_ptr<SymbolicGraphNode>> roots;
        roots.reserve(graphs.size());
        for (int i = 0; i < graphs.size(); ++i) {
            std::vector<std::shared_ptr<SymbolicGraphNode>> children;
            children.push_back(std::move(graphs[i]));
            roots.push_back(
                SymbolicGraph::createNode(
                    "root",
                    {
                        {"channel", static_cast<js::Number>(i)},
                        {"fadeInMs", static_cast<js::Number>(options.fadeInMs)},
                        {"fadeOutMs", static_cast<js::Number>(options.fadeOutMs)},
                    },
                    std::move(children)
                )
            );
        }

        std::vector<std::shared_ptr<SymbolicGraphNode>> stack;
        for (const auto &root: roots) {
            stack.push_back(root);
        }

        while (!stack.empty()) {
            const auto node = stack.back();
            stack.pop_back();

            if (const auto &found = visited.find(node->hash);
                found != visited.end()) { continue; }
            visited.insert(node->hash);

            visit(*node, instructions);

            for (const auto &child: node->children) {
                stack.push_back(child);
            }
        }

        std::vector<NodeId> rootHashes;
        rootHashes.reserve(roots.size());
        for (const auto &root: roots) {
            rootHashes.push_back(root->hash);
        }

        instructions.activateRoots.emplace_back(makeActivateRootsInstruction(
                std::move(rootHashes)
            )
        );
        instructions.commitUpdates.emplace_back(makeCommitUpdatesInstruction());

        RenderResult stats;
        stats.nodesAdded = static_cast<int32_t>(instructions.createNode.size());
        stats.edgesAdded = static_cast<int32_t>(instructions.appendChild.size());
        stats.propsWritten = static_cast<int32_t>(instructions.setProperty.size());

        auto const t1 = std::chrono::steady_clock::now();
        stats.elapsedTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        stats.result = mRuntime->applyInstructions(instructions.takeBatchedInstructions());

        return stats;
    }

    template<typename FloatType>
    NodeRef Renderer<FloatType>::createRef(std::string kind, js::Object props, std::vector<std::shared_ptr<SymbolicGraphNode>> children) {
        if (props.count("key") < 0) props["key"] = "__refKey:" + nextRefId++;
        auto node = SymbolicGraph::createNode(std::move(kind), std::move(props), std::move(children));
        // TODO: js core returns a `setter` that takes `newProps` and applies all the new props via
        // updateNodeProps which compares newProps against the current props in the nodeMap then updates them.
        // This optimizes for batching instructions together in the same js->c++ message and instructions commit.
        auto setProperty = [node](js::Object newProps) {
            // TODO: updateNodeProps

            // TODO: send setProperties instructions + commitUpdates instruction to applyInstructions of runtime

            // TODO: update the property in nodeMap here (if we aren't able to do it through shared_ptr in updateNodeProps
            // TODO: If we want to do this with proper transaction semantics (which I'm not sure we are committed to in this PR,
            // we would need to queue the update to nodeMap in a way that only triggers upon successfully applying the
            // instructions).
        };
        return NodeRef{
            std::move(node),
            std::move(setProperty)
        };
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeCreateNodeInstruction(std::string kind, const NodeId hash) {
        return {JsInstructionType::CREATE_NODE, static_cast<js::Number>(hash), js::Value(std::move(kind))};
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeAppendChildInstruction(const NodeId parentHash, const NodeId childHash,
                                                              const OutputChannel childOutputChannel) {
        return {
            JsInstructionType::APPEND_CHILD, static_cast<js::Number>(parentHash),
            static_cast<js::Number>(childHash), static_cast<js::Number>(childOutputChannel)
        };
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeSetPropertyInstruction(const NodeId hash, std::string key, js::Value value) {
        return {
            JsInstructionType::SET_PROPERTY, static_cast<js::Number>(hash),
            js::Value(std::move(key)), std::move(value)
        };
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeActivateRootsInstruction(std::vector<NodeId> roots) {
        // TODO: Don't activate roots if they are already active (see js core renderer)
        return {JsInstructionType::ACTIVATE_ROOTS, js::Array(roots.begin(), roots.end())};
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeCommitUpdatesInstruction() {
        return {JsInstructionType::COMMIT_UPDATES};
    }
}
