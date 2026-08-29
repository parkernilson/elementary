#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/AudioBufferResource.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/SymbolicGraph.h"

#include "GraphSnapshotTestUtils.h"
#include "elem/lib/Core.h"
#include "elem/lib/Math.h"
#include "elem/lib/Mc.h"
#include "elem/lib/Oscillators.h"

// Port of js/packages/core/__tests__/core.test.js and mc.test.js.
//
// lib.test.js's single test ("errors on graph construction") is not ported here: both of its
// assertions are compile-time-enforced in C++, not runtime-throwable conditions.
//   - el.seq({}, 1) checks a missing required argument; elem::lib::seq has fixed arity
//     (props, trigger, reset) enforced by the compiler.
//   - el.mul(1, 2, '4') checks a string where a number is expected; ElemNode is
//     std::variant<NodeRepr, js::Number> with no implicit conversion from string literals.
// Neither is expressible as a GTest runtime assertion.

TEST(NativeRendererSnapshotTests, DistinguishByProps) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    runtime->addSharedResource("test/path.wav", std::make_unique<elem::AudioBufferResource>(1, 512));
    elem::Renderer<float> renderer(runtime);

    auto renderVoice = [](std::string path, std::vector<elem::js::Number> seq) {
        return elem::SymbolicGraph::createNode("sample", elem::js::Object{{"path", path}}, {
            elem::SymbolicGraph::createNode("seq", elem::js::Object{{"seq", elem::js::Array(seq.begin(), seq.end())}}, {
                elem::lib::le(
                    elem::lib::phasor(elem::lib::constant(2.0)),
                    elem::lib::constant(0.5)
                )
            })
        });
    };

    const auto result = renderer.renderGraph({
        renderVoice("test/path.wav", {0, 0, 1}),
        renderVoice("test/path.wav", {0, 1, 0}),
    });

    elem::test::verifyGraphSnapshot(
        "DistinguishByProps",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 10);
    EXPECT_EQ(result.edgesAdded, 9);
    EXPECT_EQ(result.propsWritten, 12);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, MultiChannelBasics) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    // Run the same thing in two channels; we expect structural sharing except for the root nodes.
    const auto result = renderer.renderGraph({
        elem::lib::cycle(440.0),
        elem::lib::cycle(440.0),
    });

    elem::test::verifyGraphSnapshot(
        "MultiChannelBasics",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 7);
    EXPECT_EQ(result.edgesAdded, 6);
    EXPECT_EQ(result.propsWritten, 8);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, SimpleSharing) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({elem::lib::cycle(440.0)});

    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 5);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Second render inserts a tanh at the top; we should find the existing subtree and
    // share it, adding only the new tanh node and a new root (since the root's hash
    // depends on its child, which changed).
    const auto result2 = renderer.renderGraph({elem::lib::tanh(elem::lib::cycle(440.0))});

    elem::test::verifyGraphSnapshot(
        "SimpleSharing",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 2);
    EXPECT_EQ(result2.edgesAdded, 2);
    EXPECT_EQ(result2.propsWritten, 3);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

static elem::lib::NodeRepr renderKeyedVoice(std::string key, double freq) {
    return elem::lib::sin(elem::lib::mul({
        elem::lib::constant(2.0 * elem::lib::PI<float>),
        elem::lib::phasor(elem::lib::constant(freq, key)),
    }));
}

TEST(NativeRendererSnapshotTests, DistinguishedSubtreesByKey) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 440),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    elem::test::verifyGraphSnapshot(
        "DistinguishedSubtreesByKey",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 19);
    EXPECT_EQ(result.edgesAdded, 21);
    EXPECT_EQ(result.propsWritten, 12);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, StructuralEqualityWithValueChange) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 440),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    EXPECT_EQ(result1.nodesAdded, 19);
    EXPECT_EQ(result1.edgesAdded, 21);
    EXPECT_EQ(result1.propsWritten, 12);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Change one of the keyed values; we expect structural equality (no new nodes or
    // edges) since the node is found by its key, but the changed value is still written.
    const auto result2 = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 441),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    elem::test::verifyGraphSnapshot(
        "StructuralEqualityWithValueChange",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 0);
    EXPECT_EQ(result2.edgesAdded, 0);
    EXPECT_EQ(result2.propsWritten, 1);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

// Testing here to ensure that root activation/deactivation works as expected across
// renders, and that nodes are not garbage collected just because they became inactive.
TEST(NativeRendererSnapshotTests, SwitchAndSwitchBack) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({renderKeyedVoice("hi", 440)});
    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 6);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    const auto result2 = renderer.renderGraph({renderKeyedVoice("bye", 880)});
    EXPECT_EQ(result2.nodesAdded, 5);
    EXPECT_EQ(result2.edgesAdded, 5);
    EXPECT_EQ(result2.propsWritten, 5);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());

    // Third render switches back to A. We expect this to be a full no-op: A's subtree
    // was never garbage collected, so nothing new needs to be created or written.
    const auto result3 = renderer.renderGraph({renderKeyedVoice("hi", 440)});

    elem::test::verifyGraphSnapshot(
        "SwitchAndSwitchBack",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result3.nodesAdded, 0);
    EXPECT_EQ(result3.edgesAdded, 0);
    EXPECT_EQ(result3.propsWritten, 0);
    EXPECT_EQ(result3.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, RefSetterUpdatesPropsWithoutRecreatingTree) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    // Sine tone with a frequency set by ref.
    auto ref = renderer.createRef("const", elem::js::Object{{"value", 440.0}}, {});

    const auto result1 = renderer.renderGraph({
        elem::lib::sin(elem::lib::mul({
            elem::lib::constant(2.0 * elem::lib::PI<float>),
            elem::lib::phasor(elem::lib::ElemNode(ref.node)),
        }))
    });

    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 5);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Using our ref setter: we expect a single prop update, no structural change.
    const auto result2 = ref.setter(elem::js::Object{{"value", 550.0}});

    elem::test::verifyGraphSnapshot(
        "RefSetterUpdatesPropsWithoutRecreatingTree",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 0);
    EXPECT_EQ(result2.edgesAdded, 0);
    EXPECT_EQ(result2.propsWritten, 1);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, McHashingReflectsOutputChannelFromChildNodes) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    runtime->addSharedResource("/v/path", std::make_unique<elem::AudioBufferResource>(2, 512));
    elem::Renderer<float> renderer(runtime);

    auto channels = elem::lib::sampleseq2(
        elem::lib::MCSampleSeq2Props{
            .path = std::string("/v/path"),
            .seq = {{.value = 1.0, .time = 0.0}},
            .duration = 2.0,
        },
        2.0,
        1.0
    );

    std::vector<elem::lib::ElemNode> muls;
    for (auto& channel : channels) {
        muls.push_back(elem::lib::mul({0.5, elem::lib::ElemNode(channel)}));
    }

    const auto result = renderer.renderGraph({elem::lib::add(std::move(muls))});

    const auto snapshotJson = elem::js::serialize(elem::js::Value(runtime->snapshot()));
    elem::test::verifyGraphSnapshot("McHashingReflectsOutputChannelFromChildNodes", snapshotJson);

    EXPECT_EQ(result.nodesAdded, 7);
    EXPECT_EQ(result.edgesAdded, 8);
    EXPECT_EQ(result.propsWritten, 8);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());

    // Demonstrates that both `mul` nodes above get visited/created independently during
    // traversal (they have different hashes because they address different output channels
    // of the same mc.sampleseq2 child), so the connection to the second output channel
    // survives in the rendered graph.
    const auto snapshot = nlohmann::json::parse(snapshotJson);
    bool foundChannelOneInlet = false;
    for (auto const& [nodeId, node] : snapshot.items()) {
        for (auto const& inlet : node.value("inlets", nlohmann::json::array())) {
            if (static_cast<int>(inlet.value("outletChannel", 0.0)) == 1) {
                foundChannelOneInlet = true;
            }
        }
    }
    EXPECT_TRUE(foundChannelOneInlet);
}
