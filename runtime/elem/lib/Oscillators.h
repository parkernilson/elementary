#pragma once

#include "elem/lib/Core.h"
#include "elem/lib/Math.h"

namespace elem::lib {
    static NodeRepr cycle(ElemNode rate) {
        return sin(mul({resolve(2.0 * PI<float>), phasor(resolve(std::move(rate)))}));
    }
}