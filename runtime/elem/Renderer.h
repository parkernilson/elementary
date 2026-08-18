#pragma once
#include <algorithm>
#include <unordered_set>

#include "HashUtils.h"
#include "Runtime.h"
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
        std::vector<js::Array> createNode;
        std::vector<js::Array> appendChild;
        std::vector<js::Array> setProperty;
        std::vector<js::Array> activateRoots;
        std::vector<js::Array> commitUpdates;

        [[nodiscard]] std::vector<js::Array> getBatchedInstructions() const {
            std::vector<js::Array> instructions;
            instructions.reserve(
                createNode.size()
                + appendChild.size()
                + setProperty.size()
                + activateRoots.size()
                + commitUpdates.size()
            );
            for (const auto& v : createNode) {
                instructions.push_back(v);
            }
            for (const auto& v : appendChild) {
                instructions.push_back(v);
            }
            for (const auto& v : setProperty) {
                instructions.push_back(v);
            }
            for (const auto& v : activateRoots) {
                instructions.push_back(v);
            }
            for (const auto& v : commitUpdates) {
                instructions.push_back(v);
            }
        }
    };

    struct RenderOptions {
        int32_t fadeInMs = 20;
        int32_t fadeOutMs = 20;
    };

    template <typename FloatType>
    class Renderer {
    public:

        explicit Renderer(std::shared_ptr<Runtime<FloatType>> runtime);

        // TODO: return statistics for benchmarking
        void renderGraph(std::vector<SymbolicGraphNode> graphs, RenderOptions options);
    private:
        static js::Array makeCreateNodeInstruction(std::string type, int hash);
        static js::Array makeAppendChildInstruction(int parentHash, int childHash, int childOutputChannel);
        static js::Array makeSetPropertyInstruction(int hash, std::string key, js::Value value);
        static js::Array makeActivateRootsInstruction(std::vector<int> roots);
        static js::Array makeCommitUpdatesInstruction();

        void visit(const SymbolicGraphNode& node, InstructionBatch& batch);

        std::shared_ptr<Runtime<FloatType>> runtime;
        std::unordered_map<NodeId, SymbolicGraphNodeShallow> nodeMap;
    };

    template <typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<Runtime<FloatType>> runtime) : runtime{std::move(runtime)} {}

    // TODO: This could probably be named something else because the syntax has different semantics in c++ than
    // in rescript.
    template <typename FloatType>
    void Renderer<FloatType>::visit(const SymbolicGraphNode& node, InstructionBatch& batch) {
        if (const auto& existing = nodeMap.find(node.hash); existing != nodeMap.end()) {
            // updateNodeProps (compare to existing node in nodeMap)
            for (const auto& [key, value] : node.props) {
                // TODO: Is there a better way to check equality without adding the thing I did in the js header?
                // I bet we could store non-js types on the SymbolicAudioGraph and then only convert to json once
                // we are creating the instructions
                if (const auto& found = existing->second.props.find(key);
                    found == existing->second.props.end() || !js::shallowEqual(existing->second.props, value)) {
                    batch.setProperty.push_back(makeSetPropertyInstruction(node.hash, key, value));
                }
            }
        } else {
            batch.createNode.push_back(makeCreateNodeInstruction(node.kind, node.hash));
            for (const auto& [key, value] : node.props) {
                batch.setProperty.push_back(makeSetPropertyInstruction(node.hash, key, value));
            }
            for (const auto& child : node.children) {
                batch.appendChild.push_back(makeAppendChildInstruction(node.hash, child.hash, node.outputChannel));
            }
        }
        nodeMap[node.hash] = static_cast<SymbolicGraphNodeShallow>(node);
    }

    template <typename FloatType>
    void Renderer<FloatType>::renderGraph(std::vector<SymbolicGraphNode> graphs, const RenderOptions options) {
        std::unordered_set<NodeId> visited;
        InstructionBatch instructions;

        // Wrap the roots of the graph in root-type nodes
        std::vector<SymbolicGraphNode> roots;
        roots.reserve(graphs.size());
        for (int i = 0; i < graphs.size(); ++i) {
            roots.push_back(
                SymbolicGraph::createNode(
                    "root",
                    {
                        {"channel", static_cast<js::Number>(i)},
                        {"fadeInMs", static_cast<js::Number>(options.fadeInMs)},
                        {"fadeOutMs", static_cast<js::Number>(options.fadeOutMs)},
                    },
                    {std::move(graphs[i])}
                )
            );
        }

        std::vector<const SymbolicGraphNode*> stack;
        for (const auto& root : roots) {
            stack.push_back(&root);
        }

        while (!stack.empty()) {
            const auto node = stack.back();
            stack.pop_back();

            if (visited.contains(node->hash)) { continue; }
            visited.insert(node->hash);

            visit(*node, instructions);

            for (const auto& child : node->children) {
                stack.push_back(&child);
            }

            stack.pop_back();
        }

        std::vector<NodeId> rootHashes;
        rootHashes.reserve(roots.size());
        for (const auto& root : roots) {
            rootHashes.push_back(root.hash);
        }

        instructions.activateRoots.push_back(
            makeActivateRootsInstruction(
                std::move(rootHashes)
            )
        );
        instructions.commitUpdates = {makeCommitUpdatesInstruction()};

        return runtime->applyInstructions(instructions.getBatchedInstructions());
        // TODO: return status? statistics?
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::makeCreateNodeInstruction(std::string type, int hash) {
        return {JsInstructionType::CREATE_NODE, static_cast<js::Number>(hash), js::Value(std::move(type))};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::makeAppendChildInstruction(int parentHash, int childHash, int childOutputChannel) {
        return {JsInstructionType::APPEND_CHILD, static_cast<js::Number>(parentHash),
            static_cast<js::Number>(childHash), static_cast<js::Number>(childOutputChannel)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::makeSetPropertyInstruction(int hash, std::string key, js::Value value) {
        return {JsInstructionType::SET_PROPERTY, static_cast<js::Number>(hash),
            js::Value(std::move(key)), std::move(value)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::makeActivateRootsInstruction(std::vector<int> roots) {
        // TODO: Don't activate roots if they are already active (see js core renderer)
        return {JsInstructionType::ACTIVATE_ROOTS, js::Array(roots.begin(), roots.end())};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::makeCommitUpdatesInstruction() {
        return {JsInstructionType::COMMIT_UPDATES};
    }
}
