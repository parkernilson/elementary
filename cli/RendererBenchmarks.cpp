#include "RendererBenchmarks.h"

#include "elem/lib/Oscillators.h"

namespace benchmark {
    template <typename FloatType>
    std::pair<GraphBuildFn, GraphRenderFn> makeRendererBenchmark1(
        std::shared_ptr<elem::Runtime<FloatType>> runtime) {

        return makeNativeGraphFns<FloatType>(std::move(runtime), [](const size_t i) {
            return elem::lib::cycle(440.0 + i);
        });
    }

    template <typename FloatType>
    const std::map<std::string, NativeScenarioFactory<FloatType>>& nativeRendererScenarios() {
        static const std::map<std::string, NativeScenarioFactory<FloatType>> scenarios = {
            {"benchmark1", &makeRendererBenchmark1<FloatType>},
        };
        return scenarios;
    }

    template std::pair<GraphBuildFn, GraphRenderFn> makeRendererBenchmark1<float>(std::shared_ptr<elem::Runtime<float>>);
    template std::pair<GraphBuildFn, GraphRenderFn> makeRendererBenchmark1<double>(std::shared_ptr<elem::Runtime<double>>);

    template const std::map<std::string, NativeScenarioFactory<float>>& nativeRendererScenarios<float>();
    template const std::map<std::string, NativeScenarioFactory<double>>& nativeRendererScenarios<double>();
}
