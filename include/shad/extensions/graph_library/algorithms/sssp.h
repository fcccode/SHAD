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

#ifndef INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ALGORITHMS_SSSP_H_
#define INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ALGORITHMS_SSSP_H_

#include <atomic>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "shad/data_structures/array.h"
#include "shad/data_structures/set.h"
#include "shad/extensions/graph_library/edge_index.h"
#include "shad/runtime/runtime.h"
#include "shad/util/measure.h"

/// @brief Length of the shortest path between two vertices.
///
/// @tparam GraphT Graph Type.
/// @tparam VertexT VertexType.
/// @param gid ObjectID of the graph.
/// @param src The source vertex.
/// @param dest The destination vertex.
/// @return The length of the shortest path between two vertices if any;
///         returns numeric limits'infinity if no path is found.
///
template <typename GraphT, typename VertexT>
size_t sssp_length(typename GraphT::ObjectID gid, VertexT src, VertexT dest);

template <typename GraphT, typename VertexT>
void __sssp_neigh_iter(shad::rt::Handle &handle, const VertexT &src,
                       const VertexT &dest,
                       typename shad::Set<VertexT>::ObjectID &qnextID,
                       // visited will be embedded in the graph
                       shad::Array<bool>::ObjectID &visitedID,
                       shad::Array<bool>::ObjectID &foundID, VertexT &target) {
  auto visitedPtr = shad::Array<bool>::GetPtr(visitedID);
  if (visitedPtr->At(dest) == true) return;
  if (dest == target) {
    bool sol_found = true;
    auto foundPtr = shad::Array<bool>::GetPtr(foundID);
    foundPtr->InsertAt(0, sol_found);
    return;
  }
  auto qnextPtr = shad::Set<VertexT>::GetPtr(qnextID);
  qnextPtr->Insert(dest);
  bool dest_visited = true;
  visitedPtr->InsertAt(dest, dest_visited);
}

template <typename GraphT, typename VertexT>
void __sssp_iteration(shad::rt::Handle &handle, const size_t &curr_vertex,
                      typename GraphT::ObjectID &gid,
                      typename shad::Set<VertexT>::ObjectID &qnextID,
                      shad::Array<bool>::ObjectID &visitedID,
                      shad::Array<bool>::ObjectID &foundID, size_t &target) {
  auto graphPtr = GraphT::GetPtr(gid);
  graphPtr->AsyncForEachNeighbor(handle, curr_vertex,
                                 __sssp_neigh_iter<GraphT, VertexT>, qnextID,
                                 visitedID, foundID, target);
}

template <typename GraphT, typename VertexT>
size_t __sssp_length(  // GraphT::SharedPtr gPtr,
    typename GraphT::ObjectID gid, size_t num_vertices,
    typename shad::Set<VertexT>::SharedPtr to_visit_0,
    typename shad::Set<VertexT>::SharedPtr to_visit_1,
    shad::Array<bool>::SharedPtr visitedPtr,
    shad::Array<bool>::SharedPtr foundPtr, VertexT src, VertexT dest) {
  if (src == dest) return 0;
  size_t level = 0;
  shad::Set<size_t>::ShadSetPtr qPtr, nextqPtr;
  qPtr = to_visit_0;
  nextqPtr = to_visit_1;

  qPtr->Insert(src);
  bool v = true;
  visitedPtr->InsertAt(src, v);
  auto visitedID = visitedPtr->GetGlobalID();
  auto foundID = foundPtr->GetGlobalID();
  shad::rt::Handle handle;
  while (qPtr->Size()) {
    auto nextID = nextqPtr->GetGlobalID();
    qPtr->AsyncForEachElement(handle, __sssp_iteration<GraphT, VertexT>, gid,
                              nextID, visitedID, foundID, dest);
    shad::rt::waitForCompletion(handle);
    // check solution
    ++level;
    if (foundPtr->At(0)) return level;
    // prepare for next round
    qPtr->Reset(num_vertices / 2);
    qPtr.swap(nextqPtr);
  }
  return std::numeric_limits<size_t>::infinity();
}

template <typename GraphT, typename VertexT>
size_t sssp_length(typename GraphT::ObjectID gid, VertexT src, VertexT dest) {
  auto gPtr = GraphT::GetPtr(gid);
  size_t num_vertices = gPtr->Size();
  auto q0Ptr = shad::Set<VertexT>::Create(num_vertices / 2);
  auto q1Ptr = shad::Set<VertexT>::Create(num_vertices / 2);
  auto visited = shad::Array<bool>::Create(num_vertices, false);
  auto found = shad::Array<bool>::Create(1, false);
  return __sssp_length<GraphT, VertexT>(gid, num_vertices, q0Ptr, q1Ptr,
                                        visited, found, src, dest);
  shad::Set<VertexT>::Destroy(q0Ptr->GetGlobalID());
  shad::Set<VertexT>::Destroy(q1Ptr->GetGlobalID());
  shad::Array<bool>::Destroy(visited->GetGlobalID());
  shad::Array<bool>::Destroy(found->GetGlobalID());
}

#endif  // INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ALGORITHMS_SSSP_H_
