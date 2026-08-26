#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/SymbolicGraph.h"

#include "SnapshotTestUtils.h"
#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

using namespace elem::lib;

// TODO: In all these tests we should also verify the statistics returned from the renderGraph method. We should be
// able to determine what they should be exactly per render.

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

// TODO: Adding a node to the top only adds 1 node, 1 appendChild, and 1 setProperty

// TODO: Changing a leaf node re-creates the whole tree

// TODO: Render a tree with Multi-Channel nodes
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

// TODO: Node with "key" prop is not re-created when prop is changed and parents are not recreated
// (All the correct props are updated with setProperty instructions)

// TODO: setter returned by createRef updates the props correctly (only sends one setProperty instruction)

// TODO: use gc() in tests (??? do we need to call gc() before each render to get proper snapshots?)

// TODO: NodeRef setter creates the proper updates
// - new value that doesn't exist in old props
// - new value that exists in old props but is different in new props (primitive and also Array/Object shallowEqual)
//   - need to make sure that we do a shallow compare, i.e. compare the elements of the Array/Object by value. I think
//     this is what c++ does automatically when comparing recursive sub structures

// TODO: after updating node with setProps (from NodeRef), the node is accurately reflected in the nodeMap
// (i.e. setProps should not only send the setProperty instructions + commitUpdates instructions to the
// runtime, but it should also update the props in the nodeMap for future render passes / updates)

// TODO: when any existing nodes are changed by setProperty instructions in a render pass, they should be updated
// in the nodeMap as well (see test case for NodeRef.setProps. This should apply to regular render passes too).

// TODO: node in nodeMap is unchanged after an unsuccessful setProps from nodeRef (i.e. unsuccessful commitChanges)

// TODO: nodeMap is unchanged after unsuccessful render pass (i.e. setProperty commands should not have updated
// nodes in nodeMap if commitChanges was unsuccessful)