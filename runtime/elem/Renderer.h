#pragma once
#include <algorithm>
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

        // TODO: Implement the graph reconciliation
        // TODO: return statistics for benchmarking
        void renderGraph(SymbolicAudioGraph graph);
    private:
        static js::Array createNode(std::string type, int hash);
        static js::Array appendChild(int parentHash, int childHash, int childOutputChannel);
        static js::Array setProperty(int hash, std::string key, js::Value value);
        static js::Array activateRoots(std::vector<int> roots);
        static js::Array commitUpdates();

        std::shared_ptr<Runtime<FloatType>> runtime;
        std::unordered_map<int, SymbolicGraphNodeShallow> nodeMap;
    };

    template <typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<Runtime<FloatType>> runtime) : runtime{std::move(runtime)} {}

    template <typename FloatType>
    void Renderer<FloatType>::renderGraph(SymbolicAudioGraph graph) {
        // TODO: Implement
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
