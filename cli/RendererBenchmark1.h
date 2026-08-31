#pragma once

#include "RendererBenchmarkScenario.h"
#include "elem/lib/NodeUtils.h"

namespace benchmark {
    template <typename FloatType>
    class RendererBenchmark1 : public NativeRendererScenario<FloatType> {
    private:
        elem::lib::NodeRepr buildNextAudioGraph_(size_t) override;
    };
}
