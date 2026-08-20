#pragma once

#include "elem/SymbolicGraph.h"

namespace elem::lib {
    using ElemNode = std::variant<std::shared_ptr<SymbolicGraphNode>, double>;
    // TODO: Should we name this SymbolicNode or Symbol or Signal or something?
    // NodeRepr is probably the right type because it follows the js core pattern
    // TODO: Maybe we should put NodeRepr in the elem namespace so we can use it more easily in other contexts...
    // like for example tests. Unless elem::lib::NodeRepr is the best namespace for it?
    // Also, is it better to just use std::shared_ptr<SymbolicGraphNode>? I think maybe it is because it is easier
    // to reason about.
    using NodeRepr = std::shared_ptr<SymbolicGraphNode>;

    static NodeRepr constant(const double value, std::optional<std::string> key=std::nullopt) {
        js::Object props;
        props.insert({"value", value});
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
                return constant(std::forward<decltype(r)>(r));
            } else if constexpr (std::is_same_v<T, std::shared_ptr<SymbolicGraphNode>>) {
                return std::forward<decltype(r)>(r);
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

    static NodeRepr rand(std::optional<double> seed = std::nullopt, std::optional<std::string> key = std::nullopt) {
        js::Object props;
        if (seed.has_value()) {
            props.insert({"seed", *seed});
        }
        if (key.has_value()) {
            props.insert({"key", std::move(*key)});
        }
        return SymbolicGraph::createNode("rand", std::move(props), {});
    }

    static NodeRepr meter(std::vector<ElemNode> children, std::optional<std::string> name = std::nullopt, std::optional<std::string> key = std::nullopt) {
        js::Object props;
        if (name.has_value()) {
            props.insert({"name", std::move(*name)});
        }
        if (key.has_value()) {
            props.insert({"key", std::move(*key)});
        }
        return SymbolicGraph::createNode("meter", std::move(props), resolveXs(std::move(children)));
    }
}
