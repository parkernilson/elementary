#pragma once

#include "Value.h"

namespace elem {
    template<typename FloatType>
    class RuntimeInterface {
    public:
        virtual ~RuntimeInterface() = default;

        virtual int applyInstructions(js::Array const &batch) = 0;
    };
}
