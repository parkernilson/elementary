#pragma once

#include <memory>
#include <vector>

#include "elem/SymbolicGraph.h"

namespace elem::lib {
    std::vector<std::shared_ptr<elem::SymbolicGraphNode>> buildHelloSineGraph();
}
