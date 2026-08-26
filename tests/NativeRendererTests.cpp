#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/SymbolicGraph.h"

#include "SnapshotTestUtils.h"
#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

TEST(NativeRendererSnapshotTests, RendersBasicSineWave) {
    const auto runtime = std::make_shared<elem::Runtime<float> >(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({elem::lib::cycle(440.0)});

    elem::test::verifySnapshot(
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

    elem::test::verifySnapshot(
        "BasicSineWaveGraphNumericLiteral",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 6);
    EXPECT_EQ(result.edgesAdded, 5);
    EXPECT_EQ(result.propsWritten, 5);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

// TODO: Render a graph with multiple roots

// TODO: Adding a node to the top only adds 1 node, 1 appendChild, and 1 setProperty

// TODO: Changing props of a leaf node re-creates the whole tree

// TODO: Changing props of a leaf node with a key prop only adds 1 setProperty instruction per prop changed

// TODO: setter returned by createRef updates the props correctly (only sends one setProperty instruction per prop changed)
// - new value that doesn't exist in old props
// - new value that exists in old props but is different in new props (primitive and also Array/Object shallowEqual)
//   - need to make sure that we do a shallow compare, i.e. compare the elements of the Array/Object by value. I think
//     this is what c++ does automatically when comparing recursive sub structures

// TODO: Render a tree with Multi-Channel nodes (via unwrap)
/*
 * I think the way that multi-channel nodes work is that their hash is the same because outputChannel is not factored
 * into the hash (or if you give it a key then the key is the hash). But then, the parents of the mc node address diff
 * outputChannels, and the outputChannel is factored into the child hashes... (why does this matter? I think it means
 * that if a parent is changed to address a different outputChannel it would change the parent hash and cause it to be
 * re-created. That is probably why it matters).
 */

// TODO: Changing a node in the middle of the tree redraws only the parents of that node

// TODO: Adding a node to the middle of the tree redraws only the parents of the node

// TODO: Custom node tests
// TODO: Custom node is created successfully

// TODO: use gc() in tests (??? do we need to call gc() before each render to get proper snapshots?)
