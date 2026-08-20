#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/RuntimeInterface.h"
#include "elem/SymbolicGraph.h"

#include "SnapshotTestUtils.h"
#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

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

using namespace elem::lib;

// TODO: In all these tests we should also verify the statistics returned from the renderGraph method. We should be
// able to determine what they should be exactly per render.

TEST(NativeRendererTestMachinery, RenderGraphAppliesInstructionsToRuntime) {
    auto mockRuntime = std::make_shared<MockRuntime>();
    EXPECT_CALL(*mockRuntime, applyInstructions(::testing::_)).Times(1);

    elem::Renderer<double> renderer(mockRuntime);
    renderer.renderGraph({}, elem::RenderOptions{});
}

TEST(NativeRendererSnapshotTests, RendersBasicSineWave) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({cycle(440.0)});

    elem::test::verifySnapshot(
        "BasicSineWaveGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 6);
    EXPECT_EQ(result.edgesAdded, 5);
    EXPECT_EQ(result.propsWritten, 5);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}
