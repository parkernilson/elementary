#pragma once

#include "elem/lib/Core.h"
#include "elem/lib/Math.h"

namespace elem::lib {
    static NodeRepr cycle(ElemNode rate) {
        return sin(mul({constant(PI<double>), phasor(resolve(std::move(rate)))}));
    }
}