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

    template std::pair<GraphBuildFn, GraphRenderFn> makeRendererBenchmark1<float>(std::shared_ptr<elem::Runtime<float>>);
    template std::pair<GraphBuildFn, GraphRenderFn> makeRendererBenchmark1<double>(std::shared_ptr<elem::Runtime<double>>);
}
