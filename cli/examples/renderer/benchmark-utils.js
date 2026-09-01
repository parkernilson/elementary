import { Renderer } from '@elemaudio/core';

function now() {
  return typeof performance !== 'undefined' ? performance.now() : Date.now();
}

// Wires up global buildNextAudioGraph/renderNextAudioGraph functions that the
// native benchmark driver invokes by name. nextGraph(i) should return the
// el.* graph to render for iteration i; the built graph is kept here between
// the build and render calls, since the driver calls them separately with no
// way to pass the graph between them.
//
// esbuild bundles each example into its own IIFE, so a plain top-level
// function declaration here wouldn't be reachable from the driver -- hence
// assigning to globalThis explicitly.
export function createBenchmarkScenario(name, nextGraph) {
  const core = new Renderer((batch) => {
    __postNativeMessage__(JSON.stringify(batch));
  });

  let pendingGraph = null;

  globalThis.buildNextAudioGraph = function buildNextAudioGraph(i) {
    const t0 = now();
    pendingGraph = nextGraph(i);
    const t1 = now();

    return { microsecondsToBuildAudioGraph: (t1 - t0) * 1000 };
  };

  globalThis.renderNextAudioGraph = function renderNextAudioGraph(i) {
    const t0 = now();
    core.render(pendingGraph);
    const t1 = now();

    return { microsecondsToRenderGraph: (t1 - t0) * 1000 };
  };
}
