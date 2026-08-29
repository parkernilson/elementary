#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/SymbolicGraph.h"

#include "GraphSnapshotTestUtils.h"
#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

TEST(NativeRendererSnapshotTests, RendersBasicSineWave) {
    const auto runtime = std::make_shared<elem::Runtime<float> >(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({elem::lib::cycle(440.0)});

    elem::test::verifyGraphSnapshot(
        "BasicSineWaveGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 6);
    EXPECT_EQ(result.edgesAdded, 5);
    EXPECT_EQ(result.propsWritten, 5);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, NumericLiteralIsResolvedToConstantNode) {
    const auto runtime = std::make_shared<elem::Runtime<float> >(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({
        elem::lib::sin(
            elem::lib::mul({
                2.0 * elem::lib::PI<float>,
                elem::lib::phasor(440.0)
            })
        )
    });

    elem::test::verifyGraphSnapshot(
        "BasicSineWaveGraphNumericLiteral",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 6);
    EXPECT_EQ(result.edgesAdded, 5);
    EXPECT_EQ(result.propsWritten, 5);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

// TODO: Changing props of a leaf node re-creates the whole tree

// TODO: Changing a node in the middle of the tree redraws only the parents of that node

// TODO: Adding a node to the middle of the tree redraws only the parents of the node

// TODO: Custom node tests
// TODO: Custom node is created successfully

// TODO: use gc() in tests (??? do we need to call gc() before each render to get proper snapshots?)
