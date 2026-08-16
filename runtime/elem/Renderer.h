#pragma once
#include <algorithm>
#include <ranges>
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

        std::vector<js::Array> getBatchedInstructions() const {
            auto batches = std::array{
                std::ranges::ref_view(createNode),
                std::ranges::ref_view(appendChild),
                std::ranges::ref_view(setProperty),
                std::ranges::ref_view(activateRoots),
                std::ranges::ref_view(commitUpdates)
            };
            return batches | std::views::join | std::ranges::to<std::vector<js::Array>>();
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
        void renderGraph(const SymbolicAudioGraph& graph, RenderOptions options);
    private:
        static js::Array makeCreateNodeInstruction(std::string type, int hash);
        static js::Array makeAppendChildInstruction(int parentHash, int childHash, int childOutputChannel);
        static js::Array makeSetPropertyInstruction(int hash, std::string key, js::Value value);
        static js::Array makeActivateRootsInstruction(std::vector<int> roots);
        static js::Array makeCommitUpdatesInstruction();

        void visit(const SymbolicAudioGraph& graph, InstructionBatch& batch, const SymbolicGraphNode& node);

        std::shared_ptr<Runtime<FloatType>> runtime;
        std::unordered_map<NodeId, SymbolicGraphNode> nodeMap;
    };

    template <typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<Runtime<FloatType>> runtime) : runtime{std::move(runtime)} {}

    // TODO: This could probably be named something else because the syntax has different semantics in c++ than
    // in rescript.
    template <typename FloatType>
    void Renderer<FloatType>::visit(const SymbolicAudioGraph& graph, InstructionBatch& batch, const SymbolicGraphNode& node) {
        if (const auto& existing = nodeMap.find(node.hash); existing != nodeMap.end()) {
            // updateNodeProps (compare to existing node in nodeMap)
            for (const auto& [key, value] : node.props) {
                // TODO: Is there a better way to check equality without adding the thing I did in the js header?
                // I bet we could store non-js types on the SymbolicAudioGraph and then only convert to json once
                // we are creating the instructions
                if (!existing->second.props.contains(key) || !js::shallowEqual(existing->second.props, value)) {
                    batch.setProperty.push_back(makeSetPropertyInstruction(node.hash, key, value));
                }
            }
        } else {
            batch.createNode.push_back(makeCreateNodeInstruction(node.kind, node.hash));
            // TODO: How is node captured? Make it capture by reference because we are immediately instantiating
            // the view
            // TODO: Do this with a loop instead of a view (???)
            batch.setProperty.append_range(
                node.props | std::views::transform([node](const std::pair<std::string, js::Value>& prop) {
                    const auto& [key, value] = prop;
                    return makeSetPropertyInstruction(node.hash, key, value);
                }) | std::ranges::to<js::Array>()
            );
            // TODO: DO this with a loop instead of a view (???)
            batch.appendChild.append_range(
                node.children
                    | std::views::transform([node, graph](const NodeId child){ return makeAppendChildInstruction(node.hash, child, graph.nodes.at(child).outputChannel); })
                    | std::ranges::to<js::Array>()
            );
        }
        // TODO: Does this make a copy?
        // I think that because we store children as their hashes, we don't need to worry about making a
        // shallow copy. Does the compiler copy this the way we want it to?
        // TODO: a potential optimization would be to make this point to the node in the const ref graph that they passed us
        nodeMap[node.hash] = node;
    }

    template <typename FloatType>
    void Renderer<FloatType>::renderGraph(const SymbolicAudioGraph& graph, const RenderOptions options) {
        std::unordered_set<NodeId> visited;
        InstructionBatch instructions;

        // Wrap the roots of the graph in root-type nodes
        std::vector<SymbolicGraphNode> roots;
        roots.reserve(graph.roots.size());
        for (int i = 0; i < graph.roots.size(); ++i) {
            roots.push_back(
                SymbolicGraph::createNode(
                    "root",
                    {
                        {"channel", static_cast<js::Number>(i)},
                        {"fadeInMs", static_cast<js::Number>(options.fadeInMs)},
                        {"fadeOutMs", static_cast<js::Number>(options.fadeOutMs)},
                    },
                    {graph.roots[i]}
                )
            );
        }

        std::vector<NodeId> stack;
        for (const auto& root : roots) {
            // Since the root nodes are not in the symbolic graph's nodes, we must visit them directly
            // However, this diverges from js core because it visits the nodes in a pre-visit fashion where the entire
            // DFS for each root is computed sequentially, however in this case the two roots are visited directly,
            // then the DFS for the first root then the DFS for the second one.
            // I don't think this truly matters, but I should verify
            // TODO: Verify this
            visit(graph, instructions, root);
            for (const auto& child : root.children) {
                stack.push_back(child);
            }
        }

        while (!stack.empty()) {
            const NodeId hash = stack.back();
            stack.pop_back();

            if (visited.contains(hash)) { continue; }
            visited.insert(hash);

            try {
                const SymbolicGraphNode& node = graph.nodes.at(hash);

                visit(graph, instructions, node);

                for (const auto& child : node.children) {
                    const auto& childNode = graph.nodes.at(child);
                    stack.push_back(childNode.hash);
                }
            } catch (const std::out_of_range&) {
                // This would only happen if the caller gave an invalid audio graph which has child hashes
                // that don't exist
                // TODO: Maybe we should add methods to the SymbolicAudioGraph that would prevent this from
                // happening. I.e. addChild(parentHash, SymbolicGraphNode) and getNode(hash) returning optional
            }
        }

        instructions.activateRoots.push_back(
            makeActivateRootsInstruction(
                roots | std::views::transform([](auto root){return root.hash;})
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
