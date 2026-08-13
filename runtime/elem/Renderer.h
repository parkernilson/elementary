#pragma once
#include <algorithm>
#include <map>
#include <unordered_set>
#include <utility>
#include "JSON.h"
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

    template <typename FloatType>
    class Renderer {
    public:
        Renderer(std::shared_ptr<Runtime<FloatType>> runtime);

        // TODO: return statistics for benchmarking
        void renderGraph(SymbolicAudioGraph graph, double rootFadeInMs, double rootFadeOutMs);
    private:
        //==============================================================================
        // Instruction batching
        //
        // Accumulated per-call as traversal state (never stored as Renderer member
        // state) so that instructions can be grouped by type (all creates, then all
        // appends, then all sets) regardless of the order in which the traversal
        // visits independent branches of the tree.
        struct InstructionBatch {
            std::vector<js::Array> createNode;
            std::vector<js::Array> appendChild;
            std::vector<js::Array> setProperty;

            js::Array flatten(std::vector<int> const& rootHashes) const {
                js::Array out;
                out.insert(out.end(), createNode.begin(), createNode.end());
                out.insert(out.end(), appendChild.begin(), appendChild.end());
                out.insert(out.end(), setProperty.begin(), setProperty.end());
                out.push_back(Renderer::activateRoots(rootHashes));
                out.push_back(Renderer::commitUpdates());
                return out;
            }
        };

        static js::Array createNode(std::string type, int hash);
        static js::Array appendChild(int parentHash, int childHash, int childOutputChannel);
        static js::Array setProperty(int hash, std::string key, js::Value value);
        static js::Array activateRoots(std::vector<int> roots);
        static js::Array commitUpdates();

        //==============================================================================
        // Hashing
        //
        // Hashing is owned entirely by the Renderer and computed on demand during
        // reconciliation; SymbolicGraphNode never stores a hash. We use uint32_t here
        // (rather than following the JS implementation's float64 arithmetic) for
        // well-defined wraparound multiplication. Bit-for-bit parity with the JS
        // hash values is not required, only parity of semantics (structural
        // equality implies hash equality), since this Renderer talks to its own
        // independent nodeMap/Runtime, never to a JS-side node map.
        static constexpr uint32_t kFnvOffsetBasis = 0x811c9dc5;

        static uint32_t mixNumber(uint32_t seed, uint32_t n) {
            return (seed ^ n) * 0x01000193u;
        }

        static uint32_t hashString(uint32_t seed, std::string const& s) {
            uint32_t r = seed;

            for (char c : s) {
                r = mixNumber(r, static_cast<uint32_t>(static_cast<unsigned char>(c)));
            }

            return r;
        }

        static int finalizeHash(uint32_t n) {
            return static_cast<int>(n & 0x7fffffffu);
        }

        static uint32_t hashProps(uint32_t seed, std::unordered_map<std::string, js::Value> const& props) {
            auto const it = props.find("key");

            if (it != props.end() && it->second.isString()) {
                return hashString(seed, (js::String) it->second);
            }

            // Build a sorted Object so that iteration order (and therefore the
            // serialized string) is deterministic regardless of the incoming
            // unordered_map's bucket layout.
            js::Object sorted(props.begin(), props.end());
            return hashString(seed, js::serialize(js::Value(sorted)));
        }

        // Full deep equality via serialization, rather than the JS implementation's
        // one-level shallowEqual.
        //
        // TODO: verify this deep-equality semantics matches the JS implementation's
        // shallowEqual closely enough in practice (e.g. for array/sequence props).
        // See docs/superpowers/specs/2026-08-13-native-renderer-reconciliation-design.md
        static bool valuesEqual(js::Value const& a, js::Value const& b) {
            return js::serialize(a) == js::serialize(b);
        }

        //==============================================================================
        // Reconciliation
        //
        // A single post-order recursive pass: children are hashed (and mounted)
        // before their parent, because the parent's hash depends on its children's
        // hashes. Traversal state (visited, batch) is passed by reference rather
        // than stored on Renderer, so that renderGraph calls don't need to reset
        // any member state between calls.
        int visit(SymbolicGraphNode const& node, std::unordered_set<int>& visited, InstructionBatch& batch) {
            std::vector<std::pair<int, int>> childHashes;
            childHashes.reserve(node.children.size());

            for (auto const& child : node.children) {
                childHashes.push_back({visit(child, visited, batch), child.outputChannel});
            }

            uint32_t h = hashString(kFnvOffsetBasis, node.type);
            h = hashProps(h, node.props);

            for (auto const& [childHash, outputChannel] : childHashes) {
                h = mixNumber(h, mixNumber(static_cast<uint32_t>(childHash), static_cast<uint32_t>(outputChannel)));
            }

            int const hash = finalizeHash(h);

            if (visited.count(hash) > 0) {
                return hash;
            }

            visited.insert(hash);
            mount(node, hash, childHashes, batch);
            return hash;
        }

        void mount(
            SymbolicGraphNode const& node,
            int hash,
            std::vector<std::pair<int, int>> const& childHashes,
            InstructionBatch& batch)
        {
            auto const existingIt = nodeMap.find(hash);

            if (existingIt == nodeMap.end()) {
                batch.createNode.push_back(createNode(node.type, hash));

                for (auto const& [key, value] : node.props) {
                    batch.setProperty.push_back(setProperty(hash, key, value));
                }

                for (auto const& [childHash, outputChannel] : childHashes) {
                    batch.appendChild.push_back(appendChild(hash, childHash, outputChannel));
                }

                SymbolicGraphNodeShallow shallow;
                shallow.type = node.type;
                shallow.props = node.props;
                shallow.outputChannel = node.outputChannel;
                nodeMap.insert({hash, std::move(shallow)});
            } else {
                auto& existing = existingIt->second;

                for (auto const& [key, value] : node.props) {
                    auto const propIt = existing.props.find(key);
                    bool const shouldUpdate = propIt == existing.props.end() || !valuesEqual(propIt->second, value);

                    if (shouldUpdate) {
                        batch.setProperty.push_back(setProperty(hash, key, value));
                        existing.props[key] = value;
                    }
                }
            }
        }

        // Synthesizes the "root" wrapper node that every top-level graph is mounted
        // under, matching NodeRepr.create("root", {...}, [g]) in the JS reconciler.
        static SymbolicGraphNode wrapAsRoot(SymbolicGraphNode graph, int channel, double fadeInMs, double fadeOutMs) {
            SymbolicGraphNode root;
            root.type = "root";
            root.props = {
                {"channel", js::Value(static_cast<js::Number>(channel))},
                {"fadeInMs", js::Value(static_cast<js::Number>(fadeInMs))},
                {"fadeOutMs", js::Value(static_cast<js::Number>(fadeOutMs))},
            };
            root.children.push_back(std::move(graph));
            return root;
        }

        std::shared_ptr<Runtime<FloatType>> runtime;
        std::unordered_map<int, SymbolicGraphNodeShallow> nodeMap;
    };

    template <typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<Runtime<FloatType>> runtime) : runtime{std::move(runtime)} {}

    template <typename FloatType>
    void Renderer<FloatType>::renderGraph(SymbolicAudioGraph graph, double rootFadeInMs, double rootFadeOutMs) {
        InstructionBatch batch;
        std::unordered_set<int> visited;
        std::vector<int> rootHashes;
        rootHashes.reserve(graph.graphs.size());

        for (size_t i = 0; i < graph.graphs.size(); ++i) {
            SymbolicGraphNode root = wrapAsRoot(std::move(graph.graphs[i]), static_cast<int>(i), rootFadeInMs, rootFadeOutMs);
            rootHashes.push_back(visit(root, visited, batch));
        }

        runtime->applyInstructions(batch.flatten(rootHashes));
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::createNode(std::string type, int hash) {
        return {JsInstructionType::CREATE_NODE, static_cast<js::Number>(hash), js::Value(std::move(type))};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::appendChild(int parentHash, int childHash, int childOutputChannel) {
        return {JsInstructionType::APPEND_CHILD, static_cast<js::Number>(parentHash),
            static_cast<js::Number>(childHash), static_cast<js::Number>(childOutputChannel)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::setProperty(int hash, std::string key, js::Value value) {
        return {JsInstructionType::SET_PROPERTY, static_cast<js::Number>(hash),
            js::Value(std::move(key)), std::move(value)};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::activateRoots(std::vector<int> roots) {
        // TODO: Don't activate roots if they are already active (see js core renderer)
        return {JsInstructionType::ACTIVATE_ROOTS, js::Array(roots.begin(), roots.end())};
    }

    template <typename FloatType>
    js::Array Renderer<FloatType>::commitUpdates() {
        return {JsInstructionType::COMMIT_UPDATES};
    }
}
