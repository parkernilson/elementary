#pragma once

#include <memory>
#include <utility>

#include "RendererBenchmarkScenario.h"
#include <elem/Runtime.h>

namespace benchmark {
    // Cycles a single oscillator whose frequency shifts with the iteration
    // index. All graph-building state lives in the closures returned here;
    // no separate class is needed to carry it between build and render.
    template <typename FloatType>
    std::pair<GraphBuildFn, GraphRenderFn> makeRendererBenchmark1(
        std::shared_ptr<elem::Runtime<FloatType>> runtime);
}
