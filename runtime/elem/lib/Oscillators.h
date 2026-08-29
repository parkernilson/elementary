#pragma once

#include "Core.h"
#include "Math.h"
#include "NodeUtils.h"

namespace elem::lib {
    static NodeRepr train(ElemNode rate) {
        return le(phasor(std::move(rate)), 0.5);
    }

    static NodeRepr cycle(ElemNode rate) {
        return sin(mul({resolve(2.0 * PI<float>), phasor(resolve(std::move(rate)))}));
    }

    static NodeRepr saw(ElemNode rate) {
        return sub({mul({2.0, phasor(std::move(rate))}), 1.0});
    }

    static NodeRepr square(ElemNode rate) {
        return sub({mul({2.0, train(std::move(rate))}), 1.0});
    }

    static NodeRepr triangle(ElemNode rate) {
        return mul({2.0, sub({0.5, abs(saw(std::move(rate)))})});
    }

    static NodeRepr blepsaw(ElemNode rate) {
        return SymbolicGraph::createNode("blepsaw", {}, {resolve(std::move(rate))});
    }

    static NodeRepr blepsquare(ElemNode rate) {
        return SymbolicGraph::createNode("blepsquare", {}, {resolve(std::move(rate))});
    }

    static NodeRepr bleptriangle(ElemNode rate) {
        return SymbolicGraph::createNode("bleptriangle", {}, {resolve(std::move(rate))});
    }

    static NodeRepr noise(RandProps props = {}) {
        return sub({mul({2.0, rand(std::move(props))}), 1.0});
    }
}