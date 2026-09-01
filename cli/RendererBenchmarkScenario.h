#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "choc/javascript/choc_javascript.h"
#include <elem/Renderer.h>
#include <elem/Runtime.h>
#include <elem/lib/NodeUtils.h>

namespace benchmark {
    struct BuildAudioGraphStats {
        long long timeToBuildAudioGraph;
    };

    struct RenderAudioGraphStats {
        long long timeToRenderAudioGraph;
    };

    using GraphBuildFn = std::function<BuildAudioGraphStats(size_t)>;
    using GraphRenderFn = std::function<RenderAudioGraphStats(size_t)>;

    /**
     * A benchmarking scenario used to test speed and efficiency of declarative audio
     * graph rendering (i.e. how long it takes to build a symbolic audio graph and then how
     * long it takes to generate and apply the instructions to the runtime round-trip)
     *
     * This can be used to benchmark both JS and Native renderers with the same rendering loop.
     */
    class RendererBenchmarkScenario {
    public:
        RendererBenchmarkScenario(std::string name, GraphBuildFn buildGraph, GraphRenderFn renderGraph);

        void runBenchmark() const;
    private:
        std::string mName;
        GraphBuildFn mBuildGraph;
        GraphRenderFn mRenderGraph;
    };

    // Renderer<FloatType> only uses FloatType to hold a shared_ptr<Runtime<FloatType>>;
    // none of the graph-building/instruction logic we exercise here (applyInstructions,
    // gc, findNode) depends on it. Since these benchmarks never touch Runtime::process
    // (the part that actually cares about FloatType), we fix it to float rather than
    // templating this code for no benefit.
    std::pair<GraphBuildFn, GraphRenderFn> makeNativeGraphFns(
        const std::shared_ptr<elem::Runtime<float>>& runtime,
        std::function<elem::lib::NodeRepr(size_t)> nextGraph);

    std::pair<GraphBuildFn, GraphRenderFn> makeJSGraphFns(
        const std::shared_ptr<elem::Runtime<float>>& runtime,
        const std::string& jsFileName);
}
