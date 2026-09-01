#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "elem/SymbolicGraph.h"

namespace elem::lib {
    struct GraphInfo {
        std::string name;
        std::string description;
        std::function<std::vector<std::shared_ptr<elem::SymbolicGraphNode>>()> build;
    };

    std::vector<GraphInfo> const& getGraphRegistry();
}
