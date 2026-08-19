#pragma once

#include <algorithm>
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

    template<typename FloatType>
    class Renderer {
    public:
        explicit Renderer(std::shared_ptr<RuntimeInterface<FloatType> > runtime);

        // TODO: return statistics for benchmarking
        /*
        Should return something like this (see js core renderer):

        result (int status code returned by Runtime::applyInstructions. Since we are still in native layer we can use
        the decoded versions),
        nodesAdded,
        edgesAdded,
        propsWritten,
        elapsedTimeMs: t1 - t0,
         */
        void renderGraph(std::vector<SymbolicGraphNode> graphs, RenderOptions options);

    private:
        static js::Array makeCreateNodeInstruction(std::string kind, NodeId hash);

        static js::Array makeAppendChildInstruction(NodeId parentHash, NodeId childHash,
                                                    OutputChannel childOutputChannel);

        static js::Array makeSetPropertyInstruction(NodeId hash, std::string key, js::Value value);

        static js::Array makeActivateRootsInstruction(std::vector<NodeId> roots);

        static js::Array makeCommitUpdatesInstruction();

        void visit(const SymbolicGraphNode &node, InstructionBatch &batch);

        std::shared_ptr<RuntimeInterface<FloatType> > mRuntime;
        std::unordered_map<NodeId, SymbolicGraphNodeShallow> nodeMap;
    };

    template<typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<RuntimeInterface<FloatType> > runtime) : mRuntime{std::move(runtime)} {
    }

    template<typename FloatType>
    void Renderer<FloatType>::visit(const SymbolicGraphNode &node, InstructionBatch &batch) {
        if (const auto &existingNode = nodeMap.find(node.hash); existingNode != nodeMap.end()) {
            for (const auto &[key, newValue]: node.props) {
                if (const auto &oldProp = existingNode->second.props.find(key);
                    oldProp == existingNode->second.props.end() || oldProp->second != newValue) {
                    batch.setProperty.emplace_back(makeSetPropertyInstruction(node.hash, key, newValue));
                }
            }
        } else {
            batch.createNode.emplace_back(makeCreateNodeInstruction(node.kind, node.hash));
            for (const auto &[key, value]: node.props) {
                batch.setProperty.emplace_back(makeSetPropertyInstruction(node.hash, key, value));
            }
            for (const auto &child: node.children) {
                batch.appendChild.emplace_back(makeAppendChildInstruction(node.hash, child.hash, node.outputChannel));
            }
        }
        nodeMap[node.hash] = static_cast<SymbolicGraphNodeShallow>(node);
    }

    template<typename FloatType>
    void Renderer<FloatType>::renderGraph(std::vector<SymbolicGraphNode> graphs, const RenderOptions options) {
        std::unordered_set<NodeId> visited;
        InstructionBatch instructions;

        // Wrap the roots of the graph in root-type nodes
        std::vector<SymbolicGraphNode> roots;
        roots.reserve(graphs.size());
        for (int i = 0; i < graphs.size(); ++i) {
            std::vector<SymbolicGraphNode> children;
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

        std::vector<const SymbolicGraphNode *> stack;
        for (const auto &root: roots) {
            stack.push_back(&root);
        }

        while (!stack.empty()) {
            const auto node = stack.back();
            stack.pop_back();

            if (const auto &found = visited.find(node->hash);
                found != visited.end()) { continue; }
            visited.insert(node->hash);

            visit(*node, instructions);

            for (const auto &child: node->children) {
                stack.push_back(&child);
            }
        }

        std::vector<NodeId> rootHashes;
        rootHashes.reserve(roots.size());
        for (const auto &root: roots) {
            rootHashes.push_back(root.hash);
        }

        instructions.activateRoots.emplace_back(makeActivateRootsInstruction(
                std::move(rootHashes)
            )
        );
        instructions.commitUpdates = {makeCommitUpdatesInstruction()};

        mRuntime->applyInstructions(instructions.takeBatchedInstructions());
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
