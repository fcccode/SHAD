//===------------------------------------------------------------*- C++ -*-===//
//
//                                     SHAD
//
//      The Scalable High-performance Algorithms and Data Structure Library
//
//===----------------------------------------------------------------------===//
//
// Copyright 2018 Battelle Memorial Institute
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ATTRIBUTED_EDGE_INDEX_H_
#define INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ATTRIBUTED_EDGE_INDEX_H_

#include <algorithm>
#include <atomic>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>
#include "shad/data_structures/compare_and_hash_utils.h"
#include "shad/data_structures/local_hashmap.h"
#include "shad/data_structures/local_set.h"
#include "shad/extensions/graph_library/edge_index.h"
#include "shad/extensions/graph_library/local_edge_index.h"
#include "shad/runtime/runtime.h"

namespace shad {

template <typename SrcAttrT, typename DestT>
class AttrEdgesPair {
 public:
  SrcAttrT attributes_;
  LocalSet<DestT> neighbors_;
  size_t Size() { return neighbors_.Size(); }
  void Erase(const DestT &dest) { neighbors_.Erase(dest); }
  void AsyncErase(rt::Handle &handle, const DestT &dest) {
    neighbors_.Erase(handle, dest);
  }

  void Insert(const DestT &dest) { neighbors_.Insert(dest); }
  void AsyncInsert(rt::Handle &handle, const DestT &dest) {
    neighbors_.AsyncInsert(handle, dest);
  }

  template <typename ApplyFunT, typename... Args>
  void ForEachNeighbor(ApplyFunT &&function, Args &... args) {
    neighbors_.ForEachNeighbor(function, args...);
  }

  template <typename ApplyFunT, typename... Args>
  void AsyncForEachNeighbor(rt::Handle &handle, ApplyFunT &&function,
                            Args &... args) {
    neighbors_.AsyncForEachNeighbor(handle, function, args...);
  }
};

template <typename SrcT, typename DestT, typename SrcAttrT,
          typename NeighborsStorageT = AttrEdgesPair<SrcAttrT, DestT>>
class AttributedEdgeIndexStorage {
 public:
  using SrcAttributesT = SrcAttrT;
  explicit AttributedEdgeIndexStorage(const size_t numVertices)
      : edgeList_(std::max(numVertices / constants::kDefaultNumEntriesPerBucket,
                           1lu)) {}
  static constexpr size_t kEdgeListChunkSize_ = 3072 / sizeof(DestT);
  struct LocalEdgeListChunk {
    LocalEdgeListChunk(size_t _numDest, bool _ow, DestT *_dest)
        : numDest(_numDest), overwrite(_ow) {
      memcpy(destinations.data(), _dest,
             std::min(numDest, destinations.size()) * sizeof(DestT));
    }
    size_t numDest;
    size_t chunkSize;
    bool overwrite;
    std::array<DestT, kEdgeListChunkSize_> destinations;
  };

  struct FlatEdgeList {
    const DestT *values;
    size_t numValues;
    bool overwrite;
  };

  struct ElementInserter {
    void operator()(NeighborsStorageT *const lhs, const NeighborsStorageT &) {}
    static void Insert(NeighborsStorageT *const lhs, const DestT value) {
      lhs->neighbors_.Insert(value);
    }
    static void Insert(NeighborsStorageT *const lhs,
                       const FlatEdgeList values) {
      if (values.overwrite) lhs->neighbors_.Reset(values.numValues);
      for (size_t i = 0; i < values.numValues; i++) {
        lhs->neighbors_.Insert(values.values[i]);
      }
    }

    static void Insert(NeighborsStorageT *const lhs,
                       const LocalEdgeListChunk &chunk) {
      if (chunk.overwrite) lhs->neighbors_.Reset(chunk.numDest);
      size_t chunkSize = std::min(chunk.numDest, chunk.destinations.size());
      for (size_t i = 0; i < chunkSize; i++) {
        lhs->neighbors_.Insert(chunk.destinations[i]);
      }
    }
  };

  SrcAttributesT *GetVertexAttributes(const SrcT &src) {
    SrcAttributesT *res = nullptr;
    NeighborsStorageT *entry = edgeList_.Lookup(src);
    if (entry != nullptr) {
      return entry->attributes_;
    }
    return res;
  }

  bool GetVertexAttributes(const SrcT &src, SrcAttributesT *attr) {
    NeighborsStorageT *entry = edgeList_.Lookup(src);
    if (entry != nullptr) {
      *attr = entry->attributes_;
      return true;
    }
    return false;
  }

  template <typename ApplyFunT, typename... Args>
  void VertexAttributesApply(const SrcT &src, ApplyFunT &&function,
                             Args &... args) {
    NeighborsStorageT *entry = edgeList_.Lookup(src);
    if (entry != nullptr) {
      function(src, entry->attributes_, args...);
    }
  }

  using NeighborListStorageT = NeighborsStorageT;
  using EdgeListStorageT =
      LocalHashmap<SrcT, NeighborsStorageT, IDCmp<SrcT>, ElementInserter>;
  EdgeListStorageT edgeList_;

  template <typename ApplyFunT, typename... Args, std::size_t... is>
  static void CallVertexAttributesApplyFun(
      AttributedEdgeIndexStorage<SrcT, DestT, SrcAttrT, NeighborListStorageT>
          *stPtr,
      const SrcT &src, ApplyFunT function, std::tuple<Args...> &args,
      std::index_sequence<is...>) {
    NeighborsStorageT *entry = stPtr->edgeList_.Lookup(src);
    if (entry != nullptr) {
      function(src, entry->attributes_, std::get<is>(args)...);
    }
  }
};

template <typename SrcT, typename SrcAttrT, typename DestT>
using LocalAttributedEdgeIndex =
    LocalEdgeIndex<SrcT, DestT,
                   AttributedEdgeIndexStorage<SrcT, DestT, SrcAttrT>>;

template <typename SrcT, typename SrcAttrT, typename DestT>
using AttributedEdgeIndex =
    EdgeIndex<SrcT, DestT, AttributedEdgeIndexStorage<SrcT, DestT, SrcAttrT>>;

}  // namespace shad

#endif  // INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ATTRIBUTED_EDGE_INDEX_H_
