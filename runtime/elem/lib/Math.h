#pragma once

#include "Core.h"
#include "elem/SymbolicGraph.h"

namespace elem::lib {
    template<typename T>
    inline constexpr T PI = static_cast<T>(3.14159265358979323846264338327950288);

    static NodeRepr sin(ElemNode x) {
        return SymbolicGraph::createNode("sin", {}, {resolve(std::move(x))});
    }

    static NodeRepr mul(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("mul", {}, resolve(std::move(xs)));
    }
}