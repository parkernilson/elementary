#include "Benchmark.h"
#include "RendererBenchmarkScenario.h"

#include "choc/text/choc_Files.h"

namespace benchmark {
    namespace {
        constexpr auto MICROSECONDS_TO_BUILD_GRAPH = "microsecondsToBuildAudioGraph";
        constexpr auto MICROSECONDS_TO_RENDER_GRAPH = "microsecondsToRenderGraph";
    }


    template <typename FloatType>
    RendererBenchmarkScenario<FloatType>::RendererBenchmarkScenario(std::string name):
        mName{std::move(name)},
        mRuntime{elem::Runtime<FloatType>(44100.0, 512)} {
    }

    template <typename FloatType>
    void RendererBenchmarkScenario<FloatType>::runBenchmark() {
        std::vector<long long> buildAudioGraphDiffs;
        std::vector<long long> renderAudioGraphDiffs;

        for (size_t i = 0; i < 10'000; ++i) {
            auto [timeToBuildAudioGraph] = buildNextAudioGraph(i);
            auto [timeToRenderAudioGraph] = renderNextAudioGraph(i);
            buildAudioGraphDiffs.push_back(timeToBuildAudioGraph);
            renderAudioGraphDiffs.push_back(timeToRenderAudioGraph);
        }
    }

    template <typename FloatType>
    JSRendererScenario<FloatType>::JSRendererScenario(std::string name, std::string jsFileName):
        RendererBenchmarkScenario<FloatType>(name),
        mJSCtx{choc::javascript::createQuickJSContext()} {

        mJSCtx.registerFunction("__postNativeMessage__", [&](const choc::javascript::ArgumentList args) {
            RendererBenchmarkScenario<FloatType>::mRuntime.applyInstructions(elem::js::parseJSON(args[0]->toString()));
            return choc::value::Value();
        });

        mJSCtx.registerFunction("__log__", [](const choc::javascript::ArgumentList args) {
            for (size_t i = 0; i < args.numArgs; ++i) {
                std::cout << choc::json::toString(*args[i], true) << std::endl;
            }

            return choc::value::Value();
        });

        // Shim the js environment for console logging
        (void) mJSCtx.evaluate(kConsoleShimScript);

        const auto inputFile = choc::file::loadFileAsString(std::move(jsFileName));
        auto rv = mJSCtx.evaluate(inputFile);
    }

    template <typename FloatType>
    BuildAudioGraphStats JSRendererScenario<FloatType>::buildNextAudioGraph(size_t i) {
        auto response = mJSCtx.invoke("buildNextAudioGraph", i);
        if (!response.isObject()) throw std::invalid_argument("buildNextAudioGraph did not return an object");
        if (!response.hasObjectMember(MICROSECONDS_TO_BUILD_GRAPH)) throw std::invalid_argument("response did not have member: " + std::string(MICROSECONDS_TO_BUILD_GRAPH));
        if (!response[MICROSECONDS_TO_BUILD_GRAPH].isFloat64()) throw std::invalid_argument(std::string(MICROSECONDS_TO_BUILD_GRAPH) + " was not a float64");
        auto timeToBuildAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::duration<double, std::micro>(response[MICROSECONDS_TO_BUILD_GRAPH].getFloat64())).count();

        return { timeToBuildAudioGraph };
    }

    template <typename FloatType>
    RenderAudioGraphStats JSRendererScenario<FloatType>::renderNextAudioGraph(size_t i) {
        auto response = mJSCtx.invoke("renderNextAudioGraph", i);
        if (!response.isObject()) throw std::invalid_argument("buildNextAudioGraph did not return an object");
        if (!response.hasObjectMember(MICROSECONDS_TO_RENDER_GRAPH)) throw std::invalid_argument("response did not have member: " + std::string(MICROSECONDS_TO_RENDER_GRAPH));
        if (!response[MICROSECONDS_TO_RENDER_GRAPH].getFloat64()) throw std::invalid_argument(std::string(MICROSECONDS_TO_RENDER_GRAPH) + " was not a float64");
        auto timeToRenderAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::duration<double, std::micro>(response[MICROSECONDS_TO_RENDER_GRAPH].getFloat64())).count();

        return { timeToRenderAudioGraph };
    }

    template <typename FloatType>
    NativeRendererScenario<FloatType>::NativeRendererScenario(std::string name):
        RendererBenchmarkScenario<FloatType>(std::move(name)), mRenderer{RendererBenchmarkScenario<FloatType>::mRuntime} {}

    template <typename FloatType>
    BuildAudioGraphStats NativeRendererScenario<FloatType>::buildNextAudioGraph(size_t i) {
        const auto t0 = std::chrono::steady_clock::now();
        mCurGraph = buildNextAudioGraph_(i);
        const auto t1 = std::chrono::steady_clock::now();
        return {
            .timeToBuildAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
        };
    }

    template <typename FloatType>
    RenderAudioGraphStats NativeRendererScenario<FloatType>::renderNextAudioGraph(size_t i) {
        // TODO: Implement
        const auto t0 = std::chrono::steady_clock::now();
        mRenderer->renderGraph(mCurGraph);
        const auto t1 = std::chrono::steady_clock::now();

        // TODO: Include pruned nodes in the stats
        RendererBenchmarkScenario<FloatType>::mRuntime.gc();

        return {
            .timeToRenderAudioGraph = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
        };
    }
}
