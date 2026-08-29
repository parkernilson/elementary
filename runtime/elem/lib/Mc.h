#pragma once

#include "../SymbolicGraph.h"
#include "Core.h"
#include "NodeUtils.h"
#include "Props.h"

namespace elem::lib {
    DEFINE_PROPS_STRUCT(
        MCSampleProps,
        key,          std::optional<std::string>,
        path,         Required<std::string>,
        mode,         std::optional<std::string>,
        startOffset,  std::optional<js::Number>,
        stopOffset,   std::optional<js::Number>,
        playbackRate, std::optional<js::Number>
    )

    static std::vector<NodeRepr> sample(MCSampleProps props, js::Number channels, ElemNode gate) {
        auto node = SymbolicGraph::createNode("mc.sample", props.takeJsObject(),
            {resolve(std::move(gate))});

        return unpack(node, static_cast<int>(channels));
    }

    DEFINE_PROPS_STRUCT(
        MCSampleSeqProps,
        key,      std::optional<std::string>,
        path,     Required<std::string>,
        seq,      Required<std::vector<ValueTimeSeqStep>>,
        duration, Required<js::Number>
    )

    static std::vector<NodeRepr> sampleseq(MCSampleSeqProps props, js::Number channels, ElemNode time) {
        auto node = SymbolicGraph::createNode("mc.sampleseq", props.takeJsObject(),
            {resolve(std::move(time))});

        return unpack(node, static_cast<int>(channels));
    }

    DEFINE_PROPS_STRUCT(
        MCSampleSeq2Props,
        key,      std::optional<std::string>,
        path,     Required<std::string>,
        seq,      Required<std::vector<ValueTimeSeqStep>>,
        duration, Required<js::Number>,
        stretch,  std::optional<js::Number>,
        shift,    std::optional<js::Number>
    )

    static std::vector<NodeRepr> sampleseq2(MCSampleSeq2Props props, js::Number channels, ElemNode time) {
        auto node = SymbolicGraph::createNode("mc.sampleseq2", props.takeJsObject(),
            {resolve(std::move(time))});

        return unpack(node, static_cast<int>(channels));
    }

    DEFINE_PROPS_STRUCT(
        MCTableProps,
        key,  std::optional<std::string>,
        path, Required<std::string>
    )

    static std::vector<NodeRepr> table(MCTableProps props, js::Number channels, ElemNode t) {
        auto node = SymbolicGraph::createNode("mc.table", props.takeJsObject(),
            {resolve(std::move(t))});

        return unpack(node, static_cast<int>(channels));
    }

    DEFINE_PROPS_STRUCT(
        MCCaptureProps,
        name, std::optional<std::string>
    )

    static std::vector<NodeRepr> capture(MCCaptureProps props, js::Number channels, ElemNode g, std::vector<ElemNode> args) {
        std::vector<ElemNode> children;
        children.reserve(args.size() + 1);
        children.push_back(std::move(g));
        children.insert(children.end(), std::make_move_iterator(args.begin()), std::make_move_iterator(args.end()));

        auto node = SymbolicGraph::createNode("mc.capture", props.takeJsObject(),
            resolve(std::move(children)));

        return unpack(node, static_cast<int>(channels));
    }
}
