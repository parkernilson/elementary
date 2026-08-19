#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/RuntimeInterface.h"
#include "elem/SymbolicGraph.h"

#include "SnapshotTestUtils.h"

/**
 * This test target is included to give CMake a compiled target for IDE intellisense and checking
 * compilability of headers related to the Native Renderer (since it is not included in the other
 * compiled targets.
 */

namespace {
    class MockRuntime : public elem::RuntimeInterface<double> {
    public:
        MOCK_METHOD(int, applyInstructions, (elem::js::Array const &batch), (override));
    };
}

TEST(NativeRendererTestMachinery, RenderGraphAppliesInstructionsToRuntime) {
    auto mockRuntime = std::make_shared<MockRuntime>();
    EXPECT_CALL(*mockRuntime, applyInstructions(::testing::_)).Times(1);

    elem::Renderer<double> renderer(mockRuntime);
    renderer.renderGraph({}, elem::RenderOptions{});
}

TEST(NativeRendererSnapshotTests, ConstNodeGraph) {
    auto runtime = std::make_shared<elem::Runtime<double>>(44100.0, 512);
    elem::Renderer<double> renderer(runtime);

    std::vector<elem::SymbolicGraphNode> graph;
    graph.push_back(elem::SymbolicGraph::createNode("const", {{"value", 42.0}}, {}));

    renderer.renderGraph(std::move(graph), elem::RenderOptions{});

    elem::test::verifySnapshot(
        "ConstNodeGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );
}

TEST(NativeRendererSnapshotTests, SnapshotIncludesInletAndOutletConnections) {
    auto runtime = std::make_shared<elem::Runtime<double>>(44100.0, 512);
    elem::Renderer<double> renderer(runtime);

    std::vector<elem::SymbolicGraphNode> graph;
    graph.push_back(elem::SymbolicGraph::createNode("const", {{"value", 42.0}}, {}));

    renderer.renderGraph(std::move(graph), elem::RenderOptions{});

    auto const serialized = elem::js::serialize(elem::js::Value(runtime->snapshot()));

    EXPECT_THAT(serialized, ::testing::HasSubstr("\"inlets\""));
    EXPECT_THAT(serialized, ::testing::HasSubstr("\"outlets\""));
}

TEST(NativeRendererSnapshotTests, SnapshotIncludesNodeKind) {
    auto runtime = std::make_shared<elem::Runtime<double>>(44100.0, 512);
    elem::Renderer<double> renderer(runtime);

    std::vector<elem::SymbolicGraphNode> graph;
    graph.push_back(elem::SymbolicGraph::createNode("const", {{"value", 42.0}}, {}));

    renderer.renderGraph(std::move(graph), elem::RenderOptions{});

    auto const serialized = elem::js::serialize(elem::js::Value(runtime->snapshot()));

    EXPECT_THAT(serialized, ::testing::HasSubstr("\"kind\": \"const\""));
    EXPECT_THAT(serialized, ::testing::HasSubstr("\"kind\": \"root\""));
}


