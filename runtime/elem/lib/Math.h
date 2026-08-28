#pragma once

#include "Core.h"
#include "elem/SymbolicGraph.h"
#include "elem/lib/NodeUtils.h"
#include "elem/lib/Props.h"

namespace elem::lib {
    template<typename T>
    inline constexpr T PI = static_cast<T>(3.14159265358979323846264338327950288);

    // TODO: Maybe we should add an int type to js::Int or something so that we can
    // make things like channel more type safe?

    DEFINE_PROPS_STRUCT(
        IdentityProps,
        key,            std::optional<std::string>,
        channel,        std::optional<js::Number>
    )

    // Unary nodes
    /**
     * TODO: Add this clarification to the docs as well, and maybe js core
     * Identity takes a set of inputs and allows you to choose one to pass through
     * via the `channel` prop. If no inputs are provided, i.e. the node is a leaf
     * node, then the inputs are provided by the host (inputs passed to the root
     * process call)
     */
    static NodeRepr identity(IdentityProps props, std::optional<std::vector<ElemNode>> children=std::nullopt) {
        if (children.has_value()) {
            return SymbolicGraph::createNode("in", props.takeJsObject(),
                resolve(std::move(*children)));
        }
        return SymbolicGraph::createNode("in", props.takeJsObject(), {});
    }

    static NodeRepr identity(IdentityProps props, ElemNode x) {
        return SymbolicGraph::createNode("in", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    static NodeRepr in(IdentityProps props, std::optional<std::vector<ElemNode>> children=std::nullopt) {
        return identity(std::move(props), std::move(children));
    }

    static NodeRepr in(IdentityProps props, ElemNode x) {
        return identity(std::move(props), std::move(x));
    }

    static NodeRepr sin(ElemNode x) {
        return SymbolicGraph::createNode("sin", {}, {resolve(std::move(x))});
    }

    static NodeRepr cos(ElemNode x) {
        return SymbolicGraph::createNode("cos", {}, {resolve(std::move(x))});
    }

    static NodeRepr tan(ElemNode x) {
        return SymbolicGraph::createNode("tan", {}, {resolve(std::move(x))});
    }

    static NodeRepr tanh(ElemNode x) {
        return SymbolicGraph::createNode("tanh", {}, {resolve(std::move(x))});
    }

    static NodeRepr asinh(ElemNode x) {
        return SymbolicGraph::createNode("asinh", {}, {resolve(std::move(x))});
    }

    static NodeRepr ln(ElemNode x) {
        return SymbolicGraph::createNode("ln", {}, {resolve(std::move(x))});
    }

    static NodeRepr log(ElemNode x) {
        return SymbolicGraph::createNode("log", {}, {resolve(std::move(x))});
    }

    static NodeRepr log2(ElemNode x) {
        return SymbolicGraph::createNode("log2", {}, {resolve(std::move(x))});
    }

    static NodeRepr ceil(ElemNode x) {
        return SymbolicGraph::createNode("ceil", {}, {resolve(std::move(x))});
    }

    static NodeRepr floor(ElemNode x) {
        return SymbolicGraph::createNode("floor", {}, {resolve(std::move(x))});
    }

    static NodeRepr round(ElemNode x) {
        return SymbolicGraph::createNode("round", {}, {resolve(std::move(x))});
    }

    static NodeRepr sqrt(ElemNode x) {
        return SymbolicGraph::createNode("sqrt", {}, {resolve(std::move(x))});
    }

    static NodeRepr exp(ElemNode x) {
        return SymbolicGraph::createNode("exp", {}, {resolve(std::move(x))});
    }

    static NodeRepr abs(ElemNode x) {
        return SymbolicGraph::createNode("abs", {}, {resolve(std::move(x))});
    }

    // Binary nodes
    static NodeRepr le(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("le", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeRepr leq(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("leq", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeRepr ge(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("ge", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeRepr geq(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("geq", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeRepr pow(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("pow", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeRepr eq(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("eq", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeRepr and_(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("and", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeRepr or_(ElemNode a, ElemNode b) {
        return SymbolicGraph::createNode("or", {}, resolve({std::move(a), std::move(b)}));
    }

    // Binary reducing nodes
    static NodeRepr add(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("add", {}, resolve(std::move(xs)));
    }

    static NodeRepr sub(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("sub", {}, resolve(std::move(xs)));
    }

    static NodeRepr mul(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("mul", {}, resolve(std::move(xs)));
    }

    static NodeRepr div(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("div", {}, resolve(std::move(xs)));
    }

    static NodeRepr mod(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("mod", {}, resolve(std::move(xs)));
    }

    static NodeRepr min(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("min", {}, resolve(std::move(xs)));
    }

    static NodeRepr max(std::vector<ElemNode> xs) {
        return SymbolicGraph::createNode("max", {}, resolve(std::move(xs)));
    }
}