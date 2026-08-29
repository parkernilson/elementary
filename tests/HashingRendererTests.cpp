#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/SymbolicGraph.h"

#include "GraphSnapshotTestUtils.h"
#include "elem/lib/Core.h"
#include "elem/lib/Envelopes.h"
#include "elem/lib/Filters.h"
#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

// Port of js/packages/core/__tests__/hashing.test.js.
//
// The JS tests use a custom HashlessRenderer that hooks into the JS-only renderWithDelegate
// abstraction, replacing real hash values with sequential "mask ids" so the tests can assert
// on instruction *shape* independent of the hashing algorithm. The C++ Renderer has no
// delegate abstraction -- it always uses real NodeId hashes directly, with no equivalent hook
// point. That specific hash-independence property is NOT ported/verified here.
//
// What IS ported: the two graphs these tests build are rendered and snapshotted using the
// normal renderGraph + verifyGraphSnapshot pattern, exercising this more complex composed
// graph construction (adsr, lowpass, seq, blepsaw, blepsquare, train) end-to-end.

TEST(NativeRendererSnapshotTests, HashingCycleGraph) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({elem::lib::cycle(440.0)});

    elem::test::verifyGraphSnapshot(
        "HashingCycleGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, HashingComposedSynthVoiceGraph) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    auto synthVoice = [](elem::lib::ElemNode hz) {
        return elem::lib::mul({
            0.25,
            elem::lib::add({
                elem::lib::blepsaw(elem::lib::mul({hz, 1.001})),
                elem::lib::blepsquare(elem::lib::mul({hz, 0.994})),
                elem::lib::blepsquare(elem::lib::mul({hz, 0.501})),
                elem::lib::blepsaw(elem::lib::mul({hz, 0.496})),
            }),
        });
    };

    auto train = elem::lib::train(4.8);

    std::vector<double> arpSteps = {0, 4, 7, 11, 12, 11, 7, 4};
    std::vector<elem::js::Number> arp;
    for (double step : arpSteps) {
        arp.push_back(261.63 * 0.5 * std::pow(2.0, step / 12.0));
    }

    auto modulate = [](elem::lib::ElemNode x, elem::lib::ElemNode rate, elem::lib::ElemNode amt) {
        return elem::lib::add({x, elem::lib::mul({amt, elem::lib::cycle(rate)})});
    };

    auto env = elem::lib::adsr(0.01, 0.5, 0.0, 0.4, train);

    auto filt = [&](elem::lib::ElemNode x) {
        return elem::lib::lowpass(
            elem::lib::add({40.0, elem::lib::mul({modulate(1840.0, 0.05, 1800.0), env})}),
            1.0,
            x
        );
    };

    // Built directly (not via elem::lib::seq()) because elem::lib::seq()'s SeqProps.seq field
    // is Required<js::NumberArray>, which serializes to a js::Value holding the NumberArray
    // variant -- but the native SequenceNode::setProperty's "seq" handler requires
    // val.isArray() (the Array-of-Value variant), so elem::lib::seq()'s "seq" prop is rejected
    // at runtime with InvalidPropertyType. This is a pre-existing Core.h bug, out of scope here.
    auto seqNode = elem::SymbolicGraph::createNode(
        "seq",
        elem::js::Object{{"seq", elem::js::Array(arp.begin(), arp.end())}, {"hold", true}},
        elem::lib::resolve({train, 0.0})
    );

    auto out = elem::lib::mul({0.25, filt(synthVoice(seqNode))});

    const auto result = renderer.renderGraph({out, out});

    elem::test::verifyGraphSnapshot(
        "HashingComposedSynthVoiceGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}
