#pragma once

#include "JSON.h"
#include <cstdint>
#include <numeric>

#include "Types.h"

//==============================================================================
// TODO: Make this comment better
        // Hashing
        //
        // Hashing is owned entirely by the Renderer and computed on demand during
        // reconciliation; SymbolicGraphNode never stores a hash. We use uint32_t here
        // (rather than following the JS implementation's float64 arithmetic) for
        // well-defined wraparound multiplication. Bit-for-bit parity with the JS
        // hash values is not required, only parity of semantics (structural
        // equality implies hash equality), since this Renderer talks to its own
        // independent nodeMap/Runtime, never to a JS-side node map.
namespace elem {
   namespace HashUtils {
       // TODO: Make sure all of this actually matches expected functionality
       // TODO: I think all of this should actually be type NodeID=int32_t???
       static constexpr NodeId kFnvOffsetBasis = 0x811c9dc5;

       static NodeId mixNumber(NodeId seed, NodeId n) {
           return (seed ^ n) * 0x01000193;
       }

       static NodeId hashString(const NodeId seed, std::string const& s) {
           NodeId r = seed;

           for (char c : s) {
               r = mixNumber(r, c);
           }

           return r;
       }

       static NodeId hashProps(NodeId seed, js::Object const& props) {
           if (auto const it = props.find("key"); it != props.end() && it->second.isString()) {
               return hashString(seed, it->second);
           }

           return hashString(seed, js::serialize(js::Value(props)));
       }

       static NodeId finalizeHash(const NodeId n) {
           return n & 0x7fffffff;
       }

       static NodeId hashNode(std::string const& kind, js::Object const& props, std::vector<NodeId> const& children) {
           NodeId r = hashString(kFnvOffsetBasis, kind);
           r = hashProps(r, props);
           for (const auto child : children) {
               r = mixNumber(r, child);
           }
           // TODO: Why do we use signed it but ensure it's always positive with finalHash?
           return finalizeHash(r);
       }
   }
}
