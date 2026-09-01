#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
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

    template <typename FloatType>
    using NativeScenarioFactory = std::function<std::pair<GraphBuildFn, GraphRenderFn>(
        std::shared_ptr<elem::Runtime<FloatType>>)>;

    // Names every native renderer scenario so the CLI can look one up by name
    // (or list them) without a hardcoded switch statement at the call site.
    template <typename FloatType>
    const std::map<std::string, NativeScenarioFactory<FloatType>>& nativeRendererScenarios();
}
