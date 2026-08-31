#include "RendererBenchmark1.h"

#include "elem/lib/Oscillators.h"

namespace benchmark {
    template <typename FloatType>
    elem::lib::NodeRepr RendererBenchmark1<FloatType>::buildNextAudioGraph_(const size_t i) {
        return elem::lib::cycle(440.0 + i);
    }
}
