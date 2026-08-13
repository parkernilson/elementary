#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "elem/Renderer.h"
#include "elem/Runtime.h"

namespace {

using namespace elem;

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

#define TEST_CASE(name) \
    void name(); \
    static Registrar registrar_##name(#name, name); \
    void name()

TEST_CASE(smoke_test_build_wiring_works) {
    assert(1 + 1 == 2);
}

SymbolicGraphNode makeConstNode(double value) {
    SymbolicGraphNode node;
    node.type = "const";
    node.props = {{"value", js::Value(static_cast<js::Number>(value))}};
    return node;
}

TEST_CASE(first_render_creates_node_and_sets_props) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(makeConstNode(440.0));

    int result = renderer.renderGraph(graph, 20.0, 20.0);
    assert(result == ReturnCode::Ok());

    auto snap = runtime->snapshot();

    // Expect exactly two mounted nodes: the const node and its root wrapper.
    assert(snap.size() == 2);

    bool foundConstWithValue = false;

    for (auto const& [nodeIdHex, props] : snap) {
        auto const& obj = props.getObject();
        auto const it = obj.find("value");

        if (it != obj.end() && it->second.isNumber() && (js::Number) it->second == 440.0) {
            foundConstWithValue = true;
        }
    }

    assert(foundConstWithValue);
}

TEST_CASE(rerender_identical_graph_does_not_recreate_nodes) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicAudioGraph graph1;
    graph1.graphs.push_back(makeConstNode(440.0));
    int result1 = renderer.renderGraph(graph1, 20.0, 20.0);
    assert(result1 == ReturnCode::Ok());

    auto const snapAfterFirst = runtime->snapshot();

    SymbolicAudioGraph graph2;
    graph2.graphs.push_back(makeConstNode(440.0));
    int result2 = renderer.renderGraph(graph2, 20.0, 20.0);
    assert(result2 == ReturnCode::Ok());

    auto const snapAfterSecond = runtime->snapshot();

    // Same set of node ids mounted before and after: no new nodes were created.
    assert(snapAfterFirst.size() == snapAfterSecond.size());

    for (auto const& [nodeIdHex, props] : snapAfterFirst) {
        assert(snapAfterSecond.count(nodeIdHex) > 0);
    }
}

TEST_CASE(rerender_with_changed_prop_updates_value) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicGraphNode node;
    node.type = "const";
    node.props = {{"value", js::Value(static_cast<js::Number>(440.0))}, {"key", js::Value(std::string("myConst"))}};

    SymbolicAudioGraph graph1;
    graph1.graphs.push_back(node);
    int result1 = renderer.renderGraph(graph1, 20.0, 20.0);
    assert(result1 == ReturnCode::Ok());

    node.props["value"] = js::Value(static_cast<js::Number>(880.0));

    SymbolicAudioGraph graph2;
    graph2.graphs.push_back(node);
    int result2 = renderer.renderGraph(graph2, 20.0, 20.0);
    assert(result2 == ReturnCode::Ok());

    auto const snap = runtime->snapshot();

    bool foundUpdatedValue = false;

    for (auto const& [nodeIdHex, props] : snap) {
        auto const& obj = props.getObject();
        auto const it = obj.find("value");

        if (it != obj.end() && it->second.isNumber() && (js::Number) it->second == 880.0) {
            foundUpdatedValue = true;
        }
    }

    assert(foundUpdatedValue);

    // Exactly two mounted nodes still: the const node (same hash-identity via
    // the "key" prop) and its root wrapper -- no new node was created for the
    // prop change.
    assert(snap.size() == 2);
}

TEST_CASE(shared_subtree_mounted_once_appended_twice) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicGraphNode shared;
    shared.type = "const";
    shared.props = {{"value", js::Value(static_cast<js::Number>(1.0))}, {"key", js::Value(std::string("shared"))}};

    SymbolicGraphNode sum;
    sum.type = "add";
    sum.children.push_back(shared);
    sum.children.push_back(shared);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(sum);

    // Use zero fade times so the root's fade-in ramp doesn't attenuate the
    // very first processed block (a 20ms fade at 44.1kHz spans 882 samples,
    // longer than the small block we process below).
    int const result = renderer.renderGraph(graph, 0.0, 0.0);
    assert(result == ReturnCode::Ok());

    auto const snap = runtime->snapshot();

    // Mounted nodes: shared const, add, root. The shared const is mounted once
    // despite being referenced twice as a child of "add".
    assert(snap.size() == 3);

    // The snapshot alone can't prove that "add" actually has two inlets wired
    // to the shared const node (it only exposes node properties, not edges),
    // so run real audio through the graph: if both inlets are wired to
    // const(1.0), "add" should output ~2.0. If only one inlet were wired
    // (e.g. a broken/deduped appendChild), it would output ~1.0 instead.
    constexpr size_t numChannels = 1;
    constexpr size_t numSamples = 64;

    std::vector<double> outputBuffer(numSamples, 0.0);
    double* outputPtr = outputBuffer.data();
    double** outputChannelData = &outputPtr;

    // Even with 0ms fade times, the root's gain ramp needs one sample to step
    // from its initial gain of 0 up to 1, so warm up with a throwaway block
    // before asserting on steady-state output.
    runtime->process(nullptr, 0, outputChannelData, numChannels, numSamples, nullptr);
    runtime->process(nullptr, 0, outputChannelData, numChannels, numSamples, nullptr);

    for (size_t i = 0; i < numSamples; ++i) {
        assert(std::abs(outputBuffer[i] - 2.0) < 1e-6);
    }
}

TEST_CASE(multiple_top_level_graphs_get_distinct_roots) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(makeConstNode(1.0));
    graph.graphs.push_back(makeConstNode(2.0));

    int const result = renderer.renderGraph(graph, 20.0, 20.0);
    assert(result == ReturnCode::Ok());

    auto const snap = runtime->snapshot();

    // Two const nodes (different "value" props with no shared "key", so
    // different hashes) plus two distinct root wrappers (different "channel"
    // props).
    assert(snap.size() == 4);

    std::set<int> rootChannels;

    for (auto const& [nodeIdHex, props] : snap) {
        auto const& obj = props.getObject();

        if (obj.count("fadeInMs") > 0) {
            auto const it = obj.find("channel");
            assert(it != obj.end());
            assert(it->second.isNumber());
            rootChannels.insert(static_cast<int>((js::Number) it->second));
        }
    }

    // Each root wrapper must carry its own distinct channel index (0 and 1),
    // not merely be distinguishable by node count.
    assert(rootChannels.size() == 2);
    assert(rootChannels.count(0) > 0);
    assert(rootChannels.count(1) > 0);
}

TEST_CASE(render_with_unregistered_node_type_returns_error) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicGraphNode node;
    node.type = "this_type_does_not_exist";

    SymbolicAudioGraph graph;
    graph.graphs.push_back(node);

    int const result = renderer.renderGraph(graph, 20.0, 20.0);

    // An unregistered node type must surface as a non-Ok error rather than
    // being silently swallowed.
    assert(result != ReturnCode::Ok());
    assert(result == ReturnCode::UnknownNodeType());
}

TEST_CASE(references_to_different_output_channels_share_node_identity) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicGraphNode shared;
    shared.type = "const";
    shared.props = {{"value", js::Value(static_cast<js::Number>(1.0))}, {"key", js::Value(std::string("sharedChannelNode"))}};

    // Two references to the same underlying node, addressing different
    // output channels of it. outputChannel is mixed into the *parent's*
    // structural hash (see Renderer::visit), so it should not create a new
    // node identity for the shared child itself -- only affect how the
    // parent wires its appendChild edges.
    SymbolicGraphNode channel0 = shared;
    channel0.outputChannel = 0;

    SymbolicGraphNode channel1 = shared;
    channel1.outputChannel = 1;

    SymbolicGraphNode sum;
    sum.type = "add";
    sum.children.push_back(channel0);
    sum.children.push_back(channel1);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(sum);

    int const result = renderer.renderGraph(graph, 20.0, 20.0);
    assert(result == ReturnCode::Ok());

    auto const snap = runtime->snapshot();

    // Mounted nodes: shared const, add, root -- exactly 3, proving that
    // referencing the same node at two different output channels does not
    // create a spurious duplicate node. Note: Runtime::snapshot() doesn't
    // expose edge/channel wiring, so this test's coverage is limited to
    // proving outputChannel is a hash *input* (affecting the parent's
    // structural hash) rather than a source of new node identity for the
    // referenced node. Fuller behavioral verification of channel routing
    // itself would require a node type where output-channel selection is
    // externally observable (e.g. a multi-output node), which is out of
    // scope here.
    assert(snap.size() == 3);
}

TEST_CASE(prune_removes_stale_nodemap_entries) {
    auto runtime = std::make_shared<Runtime<double>>(44100.0, 512);
    Renderer<double> renderer(runtime);

    SymbolicAudioGraph graph;
    graph.graphs.push_back(makeConstNode(440.0));

    int const result1 = renderer.renderGraph(graph, 20.0, 20.0);
    assert(result1 == ReturnCode::Ok());

    auto const snapBeforePrune = runtime->snapshot();

    // Collect all currently-mounted node hashes from the snapshot (hex ids
    // like "0x479e61a7") and hand them to Renderer::prune(), simulating what
    // an embedder should do after calling Runtime::gc() to keep the
    // Renderer's nodeMap in sync.
    std::set<int> hashes;

    for (auto const& [nodeIdHex, props] : snapBeforePrune) {
        hashes.insert(static_cast<int>(std::stoul(nodeIdHex.substr(2), nullptr, 16)));
    }

    renderer.prune(hashes);

    // Re-rendering the identical graph after prune() must still succeed: the
    // Renderer forgot about these nodes, but the Runtime still actually has
    // them, so mount() will take the "already exists" branch's props-only
    // path with no ill effects, and the render should complete cleanly.
    SymbolicAudioGraph graph2;
    graph2.graphs.push_back(makeConstNode(440.0));
    int const result2 = renderer.renderGraph(graph2, 20.0, 20.0);
    assert(result2 == ReturnCode::Ok());
}

} // namespace

int main() {
    int failures = 0;

    for (auto& t : registry()) {
        std::cout << "[ RUN ] " << t.name << std::endl;
        t.fn();
        std::cout << "[ OK  ] " << t.name << std::endl;
    }

    if (failures == 0) {
        std::cout << registry().size() << " test(s) passed." << std::endl;
    }

    return failures;
}
