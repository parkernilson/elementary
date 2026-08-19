#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/RuntimeInterface.h"

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
