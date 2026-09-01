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

    // A build/render pair is the seam between the shared benchmark loop and a
    // particular scenario. The native and JS variants fill these in very
    // differently (the JS variant never hands back a NodeRepr at all, since
    // the graph is built and kept entirely on the JS side), so the scenario
    // itself stays agnostic to how the stats are produced.
    using GraphBuildFn = std::function<BuildAudioGraphStats(size_t)>;
    using GraphRenderFn = std::function<RenderAudioGraphStats(size_t)>;

    template <typename FloatType>
    class RendererBenchmarkScenario {
    public:
        RendererBenchmarkScenario(std::string name, GraphBuildFn buildGraph, GraphRenderFn renderGraph);

        void runBenchmark();
    private:
        std::string mName;
        GraphBuildFn mBuildGraph;
        GraphRenderFn mRenderGraph;
    };

    // Wraps a plain "give me the graph for iteration i" function with timing
    // and rendering against the given runtime. Individual native benchmarks
    // only need to supply nextGraph; this handles the rest, including
    // keeping the built NodeRepr alive between the build and render calls.
    template <typename FloatType>
    std::pair<GraphBuildFn, GraphRenderFn> makeNativeGraphFns(
        std::shared_ptr<elem::Runtime<FloatType>> runtime,
        std::function<elem::lib::NodeRepr(size_t)> nextGraph);

    // Drives a JS file that defines buildNextAudioGraph(i) and
    // renderNextAudioGraph(i), each returning an object reporting how long
    // the operation took on the JS side. The audio graph itself is never
    // returned to native code; JS applies it directly to the runtime.
    template <typename FloatType>
    std::pair<GraphBuildFn, GraphRenderFn> makeJSGraphFns(
        std::shared_ptr<elem::Runtime<FloatType>> runtime,
        std::string jsFileName);
}
