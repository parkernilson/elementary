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

       static NodeId hashString(NodeId seed, std::string const& s) {
           NodeId r = seed;

           for (char c : s) {
               r = mixNumber(r, static_cast<NodeId>(static_cast<char>(c)));
           }

           return r;
       }

       static NodeId hashProps(NodeId seed, js::Object const& props) {
           auto const it = props.find("key");

           if (it != props.end() && it->second.isString()) {
               return hashString(seed, (js::String) it->second);
           }

           return hashString(seed, js::serialize(js::Value(props)));
       }

       static int finalizeHash(NodeId n) {
           return static_cast<int>(n & 0x7fffffffu);
       }

       static NodeId hashNode(std::string const& kind, js::Object const& props, std::vector<NodeId> const& children) {
           const NodeId r = hashString(kFnvOffsetBasis, kind);
           const NodeId r2 = hashProps(r, props);
           if (const auto& childHashes = children; !childHashes.empty()) {
               // TODO: Does this do what I think it does?
               return finalizeHash(std::accumulate(childHashes.begin(), childHashes.end(), r2, mixNumber));
           }
           return finalizeHash(r2);
       }

       // Full deep equality via serialization, rather than the JS implementation's
       // one-level shallowEqual.
       //
       // TODO: verify this deep-equality semantics matches the JS implementation's
       // shallowEqual closely enough in practice (e.g. for array/sequence props).
       // See docs/superpowers/specs/2026-08-13-native-renderer-reconciliation-design.md
       static bool valuesEqual(js::Value const& a, js::Value const& b) {
           return js::serialize(a) == js::serialize(b);
       }
   }
}
