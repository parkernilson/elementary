#pragma once

#include <optional>
#include <type_traits>
#include <utility>

#include "elem/SymbolicGraph.h"
#include "elem/lib/NodeUtils.h"
#include "elem/lib/Props.h"

namespace elem::lib {

    static NodeRepr sr() {
        return SymbolicGraph::createNode("sr", {}, {});
    }

    static NodeRepr time() {
        return SymbolicGraph::createNode("time", {}, {});
    }

    static NodeRepr counter(ElemNode gate) {
        return SymbolicGraph::createNode("counter", {}, {resolve(std::move(gate))});
    }

    static NodeRepr accum(ElemNode xn, ElemNode reset) {
        return SymbolicGraph::createNode("accum", {},
            resolve({std::move(xn), std::move(reset)}));
    }

    static NodeRepr phasor(ElemNode rate) {
        return SymbolicGraph::createNode("phasor", {}, {resolve(std::move(rate))});
    }

    static NodeRepr syncphasor(ElemNode rate, ElemNode reset) {
        return SymbolicGraph::createNode("sphasor", {}, {resolve(std::move(rate)), resolve(std::move(reset))});
    }

    static NodeRepr latch(ElemNode t, ElemNode x) {
        return SymbolicGraph::createNode("latch", {},
            resolve({std::move(t), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        MaxHoldProps,
        key,  std::optional<std::string>,
        hold, std::optional<double>
    )

    static NodeRepr maxhold(MaxHoldProps props, ElemNode x, ElemNode reset) {
        return SymbolicGraph::createNode("maxhold", props.takeJsObject(),
            resolve({std::move(x), std::move(reset)}));
    }

    DEFINE_PROPS_STRUCT(
        OnceProps,
        key, std::optional<std::string>,
        arm, std::optional<bool>
    )

    static NodeRepr once(OnceProps props, ElemNode x) {
        return SymbolicGraph::createNode("once", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        RandProps,
        key,  std::optional<std::string>,
        seed, std::optional<js::Number>
    )

    static NodeRepr rand(RandProps props={}) {
        return SymbolicGraph::createNode("rand", props.takeJsObject(), {});
    }

    DEFINE_PROPS_STRUCT(
        MetroProps,
        key,      std::optional<std::string>,
        name,     std::optional<std::string>,
        interval, std::optional<js::Number>
    )

    static NodeRepr metro(MetroProps props) {
        return SymbolicGraph::createNode("metro", props.takeJsObject(), {});
    }

    // TODO: Can mode be a variant?
    DEFINE_PROPS_STRUCT(
        SampleProps,
        key,         std::optional<std::string>,
        path,        Required<std::string>,
        mode,        std::optional<std::string>,
        startOffset, std::optional<js::Number>,
        stopOffset,  std::optional<js::Number>
    )

    static NodeRepr sample(SampleProps props, ElemNode trigger, ElemNode rate) {
        return SymbolicGraph::createNode("sample", props.takeJsObject(),
            resolve({std::move(trigger), std::move(rate)}));
    }

    DEFINE_PROPS_STRUCT(
        TableProps,
        key,        std::optional<std::string>,
        path,       Required<std::string>
    )

    static NodeRepr table(TableProps props, ElemNode t) {
        return SymbolicGraph::createNode("table", props.takeJsObject(),
            {resolve(std::move(t))});
    }

    DEFINE_PROPS_STRUCT(
        ConvolveProps,
        key,        std::optional<std::string>,
        path,       Required<std::string>
    )

    static NodeRepr convolve(ConvolveProps props, ElemNode x) {
        return SymbolicGraph::createNode("convolve", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        SeqProps,
        key,        std::optional<std::string>,
        seq,        Required<js::NumberArray>,
        offset,     std::optional<js::Number>,
        hold,       std::optional<bool>,
        loop,       std::optional<bool>
    )

    static NodeRepr seq(SeqProps props, ElemNode trigger, ElemNode reset) {
        return SymbolicGraph::createNode("seq", props.takeJsObject(),
            resolve({std::move(trigger), std::move(reset)}));
    }

    static NodeRepr seq2(SeqProps props, ElemNode trigger, ElemNode reset) {
        return SymbolicGraph::createNode("seq2", props.takeJsObject(),
            resolve({std::move(trigger), std::move(reset)}));
    }

    DEFINE_PROPS_STRUCT(
        SparSeqStep,
        value,    Required<js::Number>,
        tickTime, Required<js::Number>
    )

    using SparSeqLoop = std::variant<bool, js::NumberArray>;
    DEFINE_PROPS_STRUCT(
        SparSeqProps,
        key,            std::optional<std::string>,
        seq,            Required<std::vector<SparSeqStep>>,
        offset,         std::optional<js::Number>,
        loop,           std::optional<SparSeqLoop>,
        interpolate,    std::optional<js::Number>,
        tickInterval,   std::optional<js::Number>
    );

    static NodeRepr sparseq(SparSeqProps props, ElemNode trigger, ElemNode reset) {
        return SymbolicGraph::createNode("sparseq", props.takeJsObject(),
            resolve({std::move(trigger), std::move(reset)}));
    }

    DEFINE_PROPS_STRUCT(
        ValueTimeSeqStep,
        value,      Required<js::Number>,
        time,       Required<js::Number>
    )

    DEFINE_PROPS_STRUCT(
        SparSeq2Props,
        key,            std::optional<std::string>,
        seq,            Required<std::vector<ValueTimeSeqStep>>
    )

    static NodeRepr sparseq2(SparSeq2Props props, ElemNode time) {
        return SymbolicGraph::createNode("sparseq2", props.takeJsObject(),
            {resolve(std::move(time))});
    }

    DEFINE_PROPS_STRUCT(
        SampleSeqProps,
        key,            std::optional<std::string>,
        path,           Required<std::string>,
        seq,            Required<std::vector<ValueTimeSeqStep>>,
        duration,       Required<js::Number>
    )

    static NodeRepr sampleseq(
        SampleSeqProps props,
        ElemNode time
    ) {
        return SymbolicGraph::createNode("sampleseq", props.takeJsObject(),
            {resolve(std::move(time))});
    }

    DEFINE_PROPS_STRUCT(
        SampleSeq2Props,
        key,            std::optional<std::string>,
        path,           Required<std::string>,
        seq,            Required<std::vector<ValueTimeSeqStep>>,
        duration,       Required<js::Number>,
        stretch,        std::optional<js::Number>,
        shift,          std::optional<js::Number>
    )

    static NodeRepr sampleseq2(
        SampleSeq2Props props,
        ElemNode time
    ) {
        return SymbolicGraph::createNode("sampleseq2", props.takeJsObject(),
            {resolve(std::move(time))});
    }

    static NodeRepr pole(ElemNode p, ElemNode x) {
        return SymbolicGraph::createNode("pole", {},
            resolve({std::move(p), std::move(x)}));
    }

    static NodeRepr env(ElemNode atkPole, ElemNode relPole, ElemNode x) {
        return SymbolicGraph::createNode("env", {},
            resolve({std::move(atkPole), std::move(relPole), std::move(x)}));
    }

    static NodeRepr z(ElemNode x) {
        return SymbolicGraph::createNode("z", {}, {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        DelayProps,
        key,        std::optional<std::string>,
        size,       Required<js::Number>
    )

    static NodeRepr delay(DelayProps props, ElemNode len, ElemNode fb, ElemNode x) {
        return SymbolicGraph::createNode("delay", props.takeJsObject(),
            resolve({std::move(len), std::move(fb), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        SDelayProps,
        key,        std::optional<std::string>,
        size,       Required<js::Number>
    )

    static NodeRepr sdelay(SDelayProps props, ElemNode x) {
        return SymbolicGraph::createNode("sdelay", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    static NodeRepr prewarp(ElemNode fc) {
        return SymbolicGraph::createNode("prewarp", {}, {resolve(std::move(fc))});
    }

    // TODO: Maybe we could make mode into an enum or variant to enumerate its possible values

    DEFINE_PROPS_STRUCT(
        MM1PProps,
        key,        std::optional<std::string>,
        mode,       std::optional<std::string>
    )

    static NodeRepr mm1p(MM1PProps props, ElemNode fc, ElemNode x) {
        return SymbolicGraph::createNode("mm1p", props.takeJsObject(),
            resolve({std::move(fc), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        SVFProps,
        key,        std::optional<std::string>,
        mode,       std::optional<std::string>
    )

    static NodeRepr svf(SVFProps props, ElemNode fc, ElemNode q, ElemNode x) {
        return SymbolicGraph::createNode("svf", props.takeJsObject(),
            resolve({std::move(fc), std::move(q), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        SVFShelfProps,
        key,        std::optional<std::string>,
        mode,       std::optional<std::string>
    )

    static NodeRepr svfshelf(SVFShelfProps props, ElemNode fc, ElemNode q, ElemNode gainDecibels, ElemNode x) {
        return SymbolicGraph::createNode("svfshelf", props.takeJsObject(),
            resolve({std::move(fc), std::move(q), std::move(gainDecibels), std::move(x)}));
    }

    static NodeRepr biquad(
        ElemNode b0,
        ElemNode b1,
        ElemNode b2,
        ElemNode a1,
        ElemNode a2,
        ElemNode x
    ) {
        return SymbolicGraph::createNode("biquad", {},
                                         resolve({
                                             std::move(b0), std::move(b1), std::move(b2),
                                             std::move(a1), std::move(a2), std::move(x)
                                         }));
    }

    DEFINE_PROPS_STRUCT(
        TapProps,
        key,        std::optional<std::string>,
        name,       Required<std::string>
    )

    static NodeRepr tapIn(TapProps props) {
        return SymbolicGraph::createNode("tapIn", props.takeJsObject(), {});
    }

    static NodeRepr tapOut(TapProps props, ElemNode x) {
        return SymbolicGraph::createNode("tapOut", props.takeJsObject(), {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        MeterProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>
    )

    static NodeRepr meter(MeterProps props, ElemNode x) {
        return SymbolicGraph::createNode("meter", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        SnapshotProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>
    )

    static NodeRepr snapshot(SnapshotProps props, ElemNode trigger, ElemNode x) {
        return SymbolicGraph::createNode("snapshot", props.takeJsObject(),
            resolve({std::move(trigger), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        ScopeProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>,
        size,       std::optional<js::Number>,
        channels,   std::optional<js::Number>
    )

    static NodeRepr scope(ScopeProps props, std::vector<ElemNode> children) {
        return SymbolicGraph::createNode("scope", props.takeJsObject(),
            resolve(std::move(children)));
    }

    DEFINE_PROPS_STRUCT(
        FFTProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>,
        size,       std::optional<js::Number>
    )

    static NodeRepr fft(FFTProps props, ElemNode x) {
        return SymbolicGraph::createNode("fft", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        CaptureProps,
        key,        std::optional<std::string>
    )

    static NodeRepr capture(CaptureProps props, ElemNode g, ElemNode x) {
        return SymbolicGraph::createNode("capture", props.takeJsObject(),
            resolve({std::move(g), std::move(x)}));
    }
}
