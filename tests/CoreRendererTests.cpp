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
