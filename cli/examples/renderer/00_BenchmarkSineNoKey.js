import { el } from '@elemaudio/core';

import { createBenchmarkScenario } from './benchmark-utils.js';

createBenchmarkScenario('RendererBenchmark1', (i) => el.cycle(440 + i));
