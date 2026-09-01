#include "RendererBenchmarkScenario.h"

#include <iostream>
#include <numeric>

#include "Benchmark.h"
#include "choc/text/choc_Files.h"

namespace benchmark {
    namespace {
        constexpr auto MICROSECONDS_TO_BUILD_GRAPH = "microsecondsToBuildGraph";
        constexpr auto MICROSECONDS_TO_RENDER_GRAPH = "microsecondsToRenderGraph";
    }

    template <typename FloatType>
    RendererBenchmarkScenario<FloatType>::RendererBenchmarkScenario(
        std::string name, GraphBuildFn buildGraph, GraphRenderFn renderGraph):
        mName{std::move(name)},
        mBuildGraph{std::move(buildGraph)},
        mRenderGraph{std::move(renderGraph)} {
    }

    namespace {
        void report(std::string const& label, std::vector<long long> const& diffs) {
            auto const sum = std::accumulate(diffs.begin(), diffs.end(), 0.0);
            auto const avg = sum / static_cast<double>(diffs.size());
            std::cout << label << " total: " << sum << "us, average: " << avg << "us" << std::endl;
        }
    }

    template <typename FloatType>
    void RendererBenchmarkScenario<FloatType>::runBenchmark() {
        std::vector<long long> buildAudioGraphDiffs;
        std::vector<long long> renderAudioGraphDiffs;

        for (size_t i = 0; i < 10'000; ++i) {
            auto [timeToBuildAudioGraph] = mBuildGraph(i);
            auto [timeToRenderAudioGraph] = mRenderGraph(i);
            buildAudioGraphDiffs.push_back(timeToBuildAudioGraph);
            renderAudioGraphDiffs.push_back(timeToRenderAudioGraph);
        }

        std::cout << "[Running " << mName << "]:" << std::endl;
        report("Build graph", buildAudioGraphDiffs);
        report("Render graph", renderAudioGraphDiffs);
        std::cout << "Done" << std::endl << std::endl;
    }

    template <typename FloatType>
    std::pair<GraphBuildFn, GraphRenderFn> makeNativeGraphFns(
        std::shared_ptr<elem::Runtime<FloatType>> runtime,
        std::function<elem::lib::NodeRepr(size_t)> nextGraph) {

        auto renderer = std::make_shared<elem::Renderer<FloatType>>(runtime);
        auto curGraph = std::make_shared<elem::lib::NodeRepr>();

        GraphBuildFn build = [curGraph, nextGraph = std::move(nextGraph)](size_t i) {
            const auto t0 = std::chrono::steady_clock::now();
            *curGraph = nextGraph(i);
            const auto t1 = std::chrono::steady_clock::now();

            return BuildAudioGraphStats{
                .timeToBuildAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
            };
        };

        GraphRenderFn render = [renderer, curGraph, runtime](size_t) {
            const auto t0 = std::chrono::steady_clock::now();
            renderer->renderGraph({*curGraph});
            const auto t1 = std::chrono::steady_clock::now();

            // TODO: Include pruned nodes in the stats
            runtime->gc();

            return RenderAudioGraphStats{
                .timeToRenderAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
            };
        };

        return {std::move(build), std::move(render)};
    }

    template <typename FloatType>
    std::pair<GraphBuildFn, GraphRenderFn> makeJSGraphFns(
        std::shared_ptr<elem::Runtime<FloatType>> runtime,
        std::string jsFileName) {

        auto ctx = std::make_shared<choc::javascript::Context>(choc::javascript::createQuickJSContext());

        ctx->registerFunction("__postNativeMessage__", [runtime](const choc::javascript::ArgumentList args) {
            runtime->applyInstructions(elem::js::parseJSON(args[0]->toString()));
            return choc::value::Value();
        });

        ctx->registerFunction("__log__", [](const choc::javascript::ArgumentList args) {
            for (size_t i = 0; i < args.numArgs; ++i) {
                std::cout << choc::json::toString(*args[i], true) << std::endl;
            }

            return choc::value::Value();
        });

        // Shim the js environment for console logging
        (void) ctx->evaluate(kConsoleShimScript);

        const auto inputFile = choc::file::loadFileAsString(std::move(jsFileName));
        (void) ctx->evaluate(inputFile);

        GraphBuildFn build = [ctx](size_t i) {
            auto response = ctx->invoke("buildNextAudioGraph", static_cast<int64_t>(i));
            if (!response.isObject()) throw std::invalid_argument("buildNextAudioGraph did not return an object");
            if (!response.hasObjectMember(MICROSECONDS_TO_BUILD_GRAPH)) throw std::invalid_argument("response did not have member: " + std::string(MICROSECONDS_TO_BUILD_GRAPH));
            if (!response[MICROSECONDS_TO_BUILD_GRAPH].isFloat64()) throw std::invalid_argument(std::string(MICROSECONDS_TO_BUILD_GRAPH) + " was not a float64");

            return BuildAudioGraphStats{
                .timeToBuildAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::duration<double, std::micro>(response[MICROSECONDS_TO_BUILD_GRAPH].getFloat64())).count()
            };
        };

        GraphRenderFn render = [ctx](size_t i) {
            auto response = ctx->invoke("renderNextAudioGraph", static_cast<int64_t>(i));
            if (!response.isObject()) throw std::invalid_argument("renderNextAudioGraph did not return an object");
            if (!response.hasObjectMember(MICROSECONDS_TO_RENDER_GRAPH)) throw std::invalid_argument("response did not have member: " + std::string(MICROSECONDS_TO_RENDER_GRAPH));
            if (!response[MICROSECONDS_TO_RENDER_GRAPH].isFloat64()) throw std::invalid_argument(std::string(MICROSECONDS_TO_RENDER_GRAPH) + " was not a float64");

            return RenderAudioGraphStats{
                .timeToRenderAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::duration<double, std::micro>(response[MICROSECONDS_TO_RENDER_GRAPH].getFloat64())).count()
            };
        };

        return {std::move(build), std::move(render)};
    }

    template class RendererBenchmarkScenario<float>;
    template class RendererBenchmarkScenario<double>;

    template std::pair<GraphBuildFn, GraphRenderFn> makeNativeGraphFns<float>(
        std::shared_ptr<elem::Runtime<float>>, std::function<elem::lib::NodeRepr(size_t)>);
    template std::pair<GraphBuildFn, GraphRenderFn> makeNativeGraphFns<double>(
        std::shared_ptr<elem::Runtime<double>>, std::function<elem::lib::NodeRepr(size_t)>);

    template std::pair<GraphBuildFn, GraphRenderFn> makeJSGraphFns<float>(
        std::shared_ptr<elem::Runtime<float>>, std::string);
    template std::pair<GraphBuildFn, GraphRenderFn> makeJSGraphFns<double>(
        std::shared_ptr<elem::Runtime<double>>, std::string);
}
