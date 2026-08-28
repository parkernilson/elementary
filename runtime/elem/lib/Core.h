#pragma once

#include <optional>
#include <type_traits>
#include <utility>

#include "elem/SymbolicGraph.h"
#include "elem/lib/Props.h"

namespace elem::lib {
    using ElemNode = std::variant<std::shared_ptr<SymbolicGraphNode>, js::Number>;
    // TODO: Should we name this SymbolicNode or Symbol or Signal or something?
    // NodeRepr is probably the right type because it follows the js core pattern
    // TODO: Maybe we should put NodeRepr in the elem namespace so we can use it more easily in other contexts...
    // like for example tests. Unless elem::lib::NodeRepr is the best namespace for it?
    // Also, is it better to just use std::shared_ptr<SymbolicGraphNode>? I think maybe it is because it is easier
    // to reason about.
    using NodeRepr = std::shared_ptr<SymbolicGraphNode>;

    static NodeRepr constant(const js::Number value, std::optional<std::string> key=std::nullopt) {
        js::Object props;
        props.insert({"value", value});
        if (key.has_value()) {
            props.insert({"key", std::move(*key)});
        }
        return SymbolicGraph::createNode("const", std::move(props), {});
    }

    /**
     * Some nodes can be represented by literal values. For example, constants can
     * be written as numerical values. The `resolve` method wraps theses literal
     * representations with the correct node.
     */
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

    static std::vector<NodeRepr> resolve(std::vector<ElemNode> xs) {
        std::vector<std::shared_ptr<SymbolicGraphNode>> res;
        res.reserve(xs.size());
        for (auto& x : xs) {
            res.emplace_back(resolve(std::move(x)));
        }
        return res;
    }

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
        mode,        std::optional<std::string>,
        startOffset, std::optional<js::Number>,
        stopOffset,  std::optional<js::Number>
    )

    static NodeRepr sample(std::string path, SampleProps props, ElemNode trigger, ElemNode rate) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"path", std::move(path)});
        return SymbolicGraph::createNode("sample", std::move(jsProps),
            resolve({std::move(trigger), std::move(rate)}));
    }

    DEFINE_PROPS_STRUCT(
        TableProps,
        key,        std::optional<std::string>
    )

    static NodeRepr table(std::string path, TableProps props, ElemNode t) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"path", std::move(path)});
        return SymbolicGraph::createNode("table", std::move(jsProps),
            {resolve(std::move(t))});
    }

    DEFINE_PROPS_STRUCT(
        ConvolveProps,
        key,        std::optional<std::string>
    )

    static NodeRepr convolve(std::string path, ConvolveProps props, ElemNode x) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"path", std::move(path)});
        return SymbolicGraph::createNode("convolve", std::move(jsProps),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        SeqProps,
        key,        std::optional<std::string>,
        offset,     std::optional<js::Number>,
        hold,       std::optional<bool>,
        loop,       std::optional<bool>
    )

    static NodeRepr seq(js::NumberArray seq, SeqProps props, ElemNode trigger, ElemNode reset) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"seq", std::move(seq)});
        return SymbolicGraph::createNode("seq", std::move(jsProps),
            resolve({std::move(trigger), std::move(reset)}));
    }

    static NodeRepr seq2(js::NumberArray seq, SeqProps props, ElemNode trigger, ElemNode reset) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"seq", std::move(seq)});
        return SymbolicGraph::createNode("seq2", std::move(jsProps),
            resolve({std::move(trigger), std::move(reset)}));
    }

    // TODO: Reduce the boilerplate for sparseq and sparseq2 and make sure it is efficient
    DEFINE_PROPS_STRUCT(
        SparSeqStep,
        value,    Required<js::Number>,
        tickTime, Required<js::Number>
    )

    inline js::Array takeSparSeqVec(std::vector<SparSeqStep>& vec) {
        js::Array arr;
        arr.reserve(vec.size());
        for (auto& step : vec) {
            arr.push_back(step.takeJsObject());
        }
        return arr;
    }

    using SparSeqLoop = std::variant<bool, js::NumberArray>;
    DEFINE_PROPS_STRUCT(
        SparSeqProps,
        key,            std::optional<std::string>,
        offset,         std::optional<js::Number>,
        loop,           std::optional<SparSeqLoop>,
        interpolate,    std::optional<js::Number>,
        tickInterval,   std::optional<js::Number>
    );

    static NodeRepr sparseq(std::vector<SparSeqStep> seq, SparSeqProps props, ElemNode trigger, ElemNode reset) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"seq", takeSparSeqVec(seq)});
        return SymbolicGraph::createNode("sparseq", std::move(jsProps),
            resolve({std::move(trigger), std::move(reset)}));
    }

    DEFINE_PROPS_STRUCT(
        ValueTimeSeqStep,
        value,      Required<js::Number>,
        time,       Required<js::Number>
    )

    inline js::Array takeValueTimeSeq(std::vector<ValueTimeSeqStep>& vec) {
        js::Array arr;
        arr.reserve(vec.size());
        for (auto& step : vec) {
            arr.push_back(step.takeJsObject());
        }
        return arr;
    }

    DEFINE_PROPS_STRUCT(
        SparSeq2Props,
        key,            std::optional<std::string>
    )

    static NodeRepr sparseq2(std::vector<ValueTimeSeqStep> seq, SparSeq2Props props, ElemNode time) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"seq", takeValueTimeSeq(seq)});
        return SymbolicGraph::createNode("sparseq2", std::move(jsProps),
            {resolve(std::move(time))});
    }

    DEFINE_PROPS_STRUCT(
        SampleSeqProps,
        key,            std::optional<std::string>
    )

    static NodeRepr sampleseq(
        std::string path,
        js::Number duration,
        std::vector<ValueTimeSeqStep> seq,
        SampleSeqProps props,
        ElemNode time
    ) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"path", std::move(path)});
        jsProps.insert({"seq", takeValueTimeSeq(seq)});
        jsProps.insert({"duration", duration});
        return SymbolicGraph::createNode("sampleseq", std::move(jsProps),
            {resolve(std::move(time))});
    }

    DEFINE_PROPS_STRUCT(
        SampleSeq2Props,
        key,            std::optional<std::string>,
        stretch,        std::optional<js::Number>,
        shift,          std::optional<js::Number>
    )

    static NodeRepr sampleseq2(
        std::string path,
        js::Number duration,
        std::vector<ValueTimeSeqStep> seq,
        SampleSeq2Props props,
        ElemNode time
    ) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"path", std::move(path)});
        jsProps.insert({"seq", takeValueTimeSeq(seq)});
        jsProps.insert({"duration", duration});
        return SymbolicGraph::createNode("sampleseq2", std::move(jsProps),
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
        key,        std::optional<std::string>
    )

    static NodeRepr delay(js::Number size, DelayProps props, ElemNode len, ElemNode fb, ElemNode x) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"size", size});
        return SymbolicGraph::createNode("delay", std::move(jsProps),
            resolve({std::move(len), std::move(fb), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        SDelayProps,
        key,        std::optional<std::string>
    )

    static NodeRepr sdelay(js::Number size, SDelayProps props, ElemNode x) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"size", size});
        return SymbolicGraph::createNode("sdelay", std::move(jsProps),
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
        key,        std::optional<std::string>
    )

    static NodeRepr tapIn(std::string name, TapProps props) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"name", std::move(name)});
        return SymbolicGraph::createNode("tapIn", std::move(jsProps), {});
    }

    static NodeRepr tapOut(std::string name, TapProps props, ElemNode x) {
        js::Object jsProps = props.takeJsObject();
        jsProps.insert({"name", std::move(name)});
        return SymbolicGraph::createNode("tapOut", std::move(jsProps), {resolve(std::move(x))});
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
