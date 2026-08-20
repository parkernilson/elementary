#pragma once

#include "elem/SymbolicGraph.h"

namespace elem::lib {
    using ElemNode = std::variant<std::shared_ptr<SymbolicGraphNode>, double>;
    // TODO: Should we name this SymbolicNode or Symbol or Signal or something?
    // NodeRepr is probably the right type because it follows the js core pattern
    using NodeRepr = std::shared_ptr<SymbolicGraphNode>;

    static NodeRepr constant(const double value, std::optional<std::string> key=std::nullopt) {
        js::Object props;
        props.insert({"value", std::move(value)});
        if (key.has_value()) {
            props.insert({"key", std::move(*key)});
        }
        return SymbolicGraph::createNode("const", std::move(props), {});
    }

    // TODO: Comment me
    static NodeRepr resolve(ElemNode repr) {
         return std::visit([](auto&& r) {
            using T = std::decay_t<decltype(r)>;
            if constexpr (std::is_same_v<T, double>) {
                return constant(std::move(r));
            } else if constexpr (std::is_same_v<T, std::shared_ptr<SymbolicGraphNode>>) {
                return std::move(r);
            }
        }, repr);
    }

    static std::vector<NodeRepr> resolveXs(std::vector<ElemNode> xs) {
        std::vector<std::shared_ptr<SymbolicGraphNode>> res;
        res.reserve(xs.size());
        for (auto& x : xs) {
            res.emplace_back(resolve(std::move(x)));
        }
        return res;
    }

    static NodeRepr phasor(ElemNode rate) {
        return SymbolicGraph::createNode("phasor", {}, {resolve(std::move(rate))});
    }
}
