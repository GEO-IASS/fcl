/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011-2014, Willow Garage, Inc.
 *  Copyright (c) 2014-2016, Open Source Robotics Foundation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Open Source Robotics Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** @author Jia Pan */

#ifndef FCL_BV_SPLITTER_H
#define FCL_BV_SPLITTER_H

#include "fcl/math/triangle.h"
#include "fcl/object/geometry/bvh/BVH_internal.h"
#include "fcl/math/bv/kIOS.h"
#include "fcl/math/bv/OBBRSS.h"
#include <vector>
#include <iostream>

namespace fcl
{

namespace detail
{

/// @brief Base interface for BV splitting algorithm
template <typename BV>
class BVSplitterBase
{
public:

  using S = typename BV::S;

  /// @brief Set the geometry data needed by the split rule
  virtual void set(
      Vector3<S>* vertices_,
      Triangle* tri_indices_,
      BVHModelType type_) = 0;

  /// @brief Compute the split rule according to a subset of geometry and the
  /// corresponding BV node
  virtual void computeRule(
      const BV& bv, unsigned int* primitive_indices, int num_primitives) = 0;

  /// @brief Apply the split rule on a given point
  virtual bool apply(const Vector3<S>& q) const = 0;

  /// @brief Clear the geometry data set before
  virtual void clear() = 0;
};

/// @brief Three types of split algorithms are provided in FCL as default
enum SplitMethodType
{
  SPLIT_METHOD_MEAN,
  SPLIT_METHOD_MEDIAN,
  SPLIT_METHOD_BV_CENTER
};

/// @brief A class describing the split rule that splits each BV node
template <typename BV>
class BVSplitter : public BVSplitterBase<BV>
{
public:

  using S = typename BV::S;

  BVSplitter(SplitMethodType method);

  /// @brief Default deconstructor
  virtual ~BVSplitter();

  /// @brief Set the geometry data needed by the split rule
  void set(
      Vector3<S>* vertices_, Triangle* tri_indices_, BVHModelType type_);

  /// @brief Compute the split rule according to a subset of geometry and the
  /// corresponding BV node
  void computeRule(
      const BV& bv, unsigned int* primitive_indices, int num_primitives);

  /// @brief Apply the split rule on a given point
  bool apply(const Vector3<S>& q) const;

  /// @brief Clear the geometry data set before
  void clear();

private:

  /// @brief The axis based on which the split decision is made. For most BV,
  /// the axis is aligned with one of the world coordinate, so only split_axis
  /// is needed. For oriented node, we can use a vector to make a better split
  /// decision.
  int split_axis;
  Vector3<S> split_vector;

  /// @brief The split threshold, different primitives are splitted according
  /// whether their projection on the split_axis is larger or smaller than the
  /// threshold
  S split_value;

  /// @brief The mesh vertices or points handled by the splitter
  Vector3<S>* vertices;

  /// @brief The triangles handled by the splitter
  Triangle* tri_indices;

  /// @brief Whether the geometry is mesh or point cloud
  BVHModelType type;

  /// @brief The split algorithm used
  SplitMethodType split_method;

  /// @brief Split algorithm 1: Split the node from center
  void computeRule_bvcenter(
      const BV& bv, unsigned int* primitive_indices, int num_primitives);

  /// @brief Split algorithm 2: Split the node according to the mean of the data
  ///  contained
  void computeRule_mean(
      const BV& bv, unsigned int* primitive_indices, int num_primitives);

  /// @brief Split algorithm 3: Split the node according to the median of the
  /// data contained
  void computeRule_median(
      const BV& bv, unsigned int* primitive_indices, int num_primitives);

  template <typename, typename>
  friend struct ApplyImpl;

  template <typename, typename>
  friend struct ComputeRuleCenterImpl;

  template <typename, typename>
  friend struct ComputeRuleMeanImpl;

  template <typename, typename>
  friend struct ComputeRuleMedianImpl;
};

template <typename S, typename BV>
void computeSplitVector(const BV& bv, Vector3<S>& split_vector);

template <typename S, typename BV>
void computeSplitValue_bvcenter(const BV& bv, S& split_value);

template <typename S, typename BV>
void computeSplitValue_mean(
    const BV& bv,
    Vector3<S>* vertices,
    Triangle* triangles,
    unsigned int* primitive_indices,
    int num_primitives,
    BVHModelType type,
    const Vector3<S>& split_vector,
    S& split_value);

template <typename S, typename BV>
void computeSplitValue_median(
    const BV& bv,
    Vector3<S>* vertices,
    Triangle* triangles,
    unsigned int* primitive_indices,
    int num_primitives,
    BVHModelType type,
    const Vector3<S>& split_vector,
    S& split_value);

//============================================================================//
//                                                                            //
//                              Implementations                               //
//                                                                            //
//============================================================================//

//==============================================================================
template <typename BV>
BVSplitter<BV>::BVSplitter(SplitMethodType method)
  : split_method(method)
{
  // Do nothing
}

//==============================================================================
template <typename BV>
BVSplitter<BV>::~BVSplitter()
{
  // Do nothing
}

//==============================================================================
template <typename BV>
void BVSplitter<BV>::set(
    Vector3<S>* vertices_, Triangle* tri_indices_, BVHModelType type_)
{
  vertices = vertices_;
  tri_indices = tri_indices_;
  type = type_;
}

//==============================================================================
template <typename BV>
void BVSplitter<BV>::computeRule(
    const BV& bv, unsigned int* primitive_indices, int num_primitives)
{
  switch(split_method)
  {
  case SPLIT_METHOD_MEAN:
    computeRule_mean(bv, primitive_indices, num_primitives);
    break;
  case SPLIT_METHOD_MEDIAN:
    computeRule_median(bv, primitive_indices, num_primitives);
    break;
  case SPLIT_METHOD_BV_CENTER:
    computeRule_bvcenter(bv, primitive_indices, num_primitives);
    break;
  default:
    std::cerr << "Split method not supported" << std::endl;
  }
}

//==============================================================================
template <typename S, typename BV>
struct ApplyImpl
{
  static bool run(
      const BVSplitter<BV>& splitter, const Vector3<S>& q)
  {
    return q[splitter.split_axis] > splitter.split_value;
  }
};

//==============================================================================
template <typename BV>
bool BVSplitter<BV>::apply(const Vector3<S>& q) const
{
  return ApplyImpl<S, BV>::run(*this, q);
}

//==============================================================================
template <typename S, typename BV>
struct ComputeRuleCenterImpl
{
  static void run(
      BVSplitter<BV>& splitter,
      const BV& bv,
      unsigned int* /*primitive_indices*/,
      int /*num_primitives*/)
  {
    Vector3<S> center = bv.center();
    int axis = 2;

    if(bv.width() >= bv.height() && bv.width() >= bv.depth())
      axis = 0;
    else if(bv.height() >= bv.width() && bv.height() >= bv.depth())
      axis = 1;

    splitter.split_axis = axis;
    splitter.split_value = center[axis];
  }
};

//==============================================================================
template <typename BV>
void BVSplitter<BV>::computeRule_bvcenter(
    const BV& bv, unsigned int* primitive_indices, int num_primitives)
{
  ComputeRuleCenterImpl<S, BV>::run(
        *this, bv, primitive_indices, num_primitives);
}

//==============================================================================
template <typename S, typename BV>
struct ComputeRuleMeanImpl
{
  static void run(
      BVSplitter<BV>& splitter,
      const BV& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    int axis = 2;

    if(bv.width() >= bv.height() && bv.width() >= bv.depth())
      axis = 0;
    else if(bv.height() >= bv.width() && bv.height() >= bv.depth())
      axis = 1;

    splitter.split_axis = axis;
    S sum = 0;

    if(splitter.type == BVH_MODEL_TRIANGLES)
    {
      for(int i = 0; i < num_primitives; ++i)
      {
        const Triangle& t = splitter.tri_indices[primitive_indices[i]];
        sum += splitter.vertices[t[0]][splitter.split_axis]
             + splitter.vertices[t[1]][splitter.split_axis]
             + splitter.vertices[t[2]][splitter.split_axis];
      }

      sum /= 3;
    }
    else if(splitter.type == BVH_MODEL_POINTCLOUD)
    {
      for(int i = 0; i < num_primitives; ++i)
      {
        sum += splitter.vertices[primitive_indices[i]][splitter.split_axis];
      }
    }

    splitter.split_value = sum / num_primitives;
  }
};

//==============================================================================
template <typename BV>
void BVSplitter<BV>::computeRule_mean(
    const BV& bv, unsigned int* primitive_indices, int num_primitives)
{
  ComputeRuleMeanImpl<S, BV>::run(
        *this, bv, primitive_indices, num_primitives);
}

//==============================================================================
template <typename S, typename BV>
struct ComputeRuleMedianImpl
{
  static void run(
      BVSplitter<BV>& splitter,
      const BV& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    int axis = 2;

    if(bv.width() >= bv.height() && bv.width() >= bv.depth())
      axis = 0;
    else if(bv.height() >= bv.width() && bv.height() >= bv.depth())
      axis = 1;

    splitter.split_axis = axis;
    std::vector<S> proj(num_primitives);

    if(splitter.type == BVH_MODEL_TRIANGLES)
    {
      for(int i = 0; i < num_primitives; ++i)
      {
        const Triangle& t = splitter.tri_indices[primitive_indices[i]];
        proj[i] = (splitter.vertices[t[0]][splitter.split_axis]
            + splitter.vertices[t[1]][splitter.split_axis]
            + splitter.vertices[t[2]][splitter.split_axis]) / 3;
      }
    }
    else if(splitter.type == BVH_MODEL_POINTCLOUD)
    {
      for(int i = 0; i < num_primitives; ++i)
        proj[i] = splitter.vertices[primitive_indices[i]][splitter.split_axis];
    }

    std::sort(proj.begin(), proj.end());

    if(num_primitives % 2 == 1)
    {
      splitter.split_value = proj[(num_primitives - 1) / 2];
    }
    else
    {
      splitter.split_value = (proj[num_primitives / 2] + proj[num_primitives / 2 - 1]) / 2;
    }
  }
};

//==============================================================================
template <typename BV>
void BVSplitter<BV>::computeRule_median(
    const BV& bv, unsigned int* primitive_indices, int num_primitives)
{
  ComputeRuleMedianImpl<S, BV>::run(
        *this, bv, primitive_indices, num_primitives);
}

//==============================================================================
template <typename S>
struct ComputeRuleCenterImpl<S, OBB<S>>
{
  static void run(
      BVSplitter<OBB<S>>& splitter,
      const OBB<S>& bv,
      unsigned int* /*primitive_indices*/,
      int /*num_primitives*/)
  {
    computeSplitVector<S, OBB<S>>(bv, splitter.split_vector);
    computeSplitValue_bvcenter<S, OBB<S>>(bv, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMeanImpl<S, OBB<S>>
{
  static void run(
      BVSplitter<OBB<S>>& splitter,
      const OBB<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, OBB<S>>(bv, splitter.split_vector);
    computeSplitValue_mean<S, OBB<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMedianImpl<S, OBB<S>>
{
  static void run(
      BVSplitter<OBB<S>>& splitter,
      const OBB<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, OBB<S>>(bv, splitter.split_vector);
    computeSplitValue_median<S, OBB<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleCenterImpl<S, RSS<S>>
{
  static void run(
      BVSplitter<RSS<S>>& splitter,
      const RSS<S>& bv,
      unsigned int* /*primitive_indices*/,
      int /*num_primitives*/)
  {
    computeSplitVector<S, RSS<S>>(bv, splitter.split_vector);
    computeSplitValue_bvcenter<S, RSS<S>>(bv, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMeanImpl<S, RSS<S>>
{
  static void run(
      BVSplitter<RSS<S>>& splitter,
      const RSS<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, RSS<S>>(bv, splitter.split_vector);
    computeSplitValue_mean<S, RSS<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMedianImpl<S, RSS<S>>
{
  static void run(
      BVSplitter<RSS<S>>& splitter,
      const RSS<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, RSS<S>>(bv, splitter.split_vector);
    computeSplitValue_median<S, RSS<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleCenterImpl<S, kIOS<S>>
{
  static void run(
      BVSplitter<kIOS<S>>& splitter,
      const kIOS<S>& bv,
      unsigned int* /*primitive_indices*/,
      int /*num_primitives*/)
  {
    computeSplitVector<S, kIOS<S>>(bv, splitter.split_vector);
    computeSplitValue_bvcenter<S, kIOS<S>>(bv, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMeanImpl<S, kIOS<S>>
{
  static void run(
      BVSplitter<kIOS<S>>& splitter,
      const kIOS<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, kIOS<S>>(bv, splitter.split_vector);
    computeSplitValue_mean<S, kIOS<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMedianImpl<S, kIOS<S>>
{
  static void run(
      BVSplitter<kIOS<S>>& splitter,
      const kIOS<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, kIOS<S>>(bv, splitter.split_vector);
    computeSplitValue_median<S, kIOS<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleCenterImpl<S, OBBRSS<S>>
{
  static void run(
      BVSplitter<OBBRSS<S>>& splitter,
      const OBBRSS<S>& bv,
      unsigned int* /*primitive_indices*/,
      int /*num_primitives*/)
  {
    computeSplitVector<S, OBBRSS<S>>(bv, splitter.split_vector);
    computeSplitValue_bvcenter<S, OBBRSS<S>>(bv, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMeanImpl<S, OBBRSS<S>>
{
  static void run(
      BVSplitter<OBBRSS<S>>& splitter,
      const OBBRSS<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, OBBRSS<S>>(bv, splitter.split_vector);
    computeSplitValue_mean<S, OBBRSS<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ComputeRuleMedianImpl<S, OBBRSS<S>>
{
  static void run(
      BVSplitter<OBBRSS<S>>& splitter,
      const OBBRSS<S>& bv,
      unsigned int* primitive_indices,
      int num_primitives)
  {
    computeSplitVector<S, OBBRSS<S>>(bv, splitter.split_vector);
    computeSplitValue_median<S, OBBRSS<S>>(
          bv, splitter.vertices, splitter.tri_indices, primitive_indices,
          num_primitives, splitter.type, splitter.split_vector, splitter.split_value);
  }
};

//==============================================================================
template <typename S>
struct ApplyImpl<S, OBB<S>>
{
  static bool run(
      const BVSplitter<OBB<S>>& splitter,
      const Vector3<S>& q)
  {
    return splitter.split_vector.dot(q) > splitter.split_value;
  }
};

//==============================================================================
template <typename S>
struct ApplyImpl<S, RSS<S>>
{
  static bool run(
      const BVSplitter<RSS<S>>& splitter,
      const Vector3<S>& q)
  {
    return splitter.split_vector.dot(q) > splitter.split_value;
  }
};

//==============================================================================
template <typename S>
struct ApplyImpl<S, kIOS<S>>
{
  static bool run(
      const BVSplitter<kIOS<S>>& splitter,
      const Vector3<S>& q)
  {
    return splitter.split_vector.dot(q) > splitter.split_value;
  }
};

//==============================================================================
template <typename S>
struct ApplyImpl<S, OBBRSS<S>>
{
  static bool run(
      const BVSplitter<OBBRSS<S>>& splitter,
      const Vector3<S>& q)
  {
    return splitter.split_vector.dot(q) > splitter.split_value;
  }
};

//==============================================================================
template <typename BV>
void BVSplitter<BV>::clear()
{
  vertices = nullptr;
  tri_indices = nullptr;
  type = BVH_MODEL_UNKNOWN;
}

//==============================================================================
template <typename S, typename BV>
struct ComputeSplitVectorImpl
{
  static void run(const BV& bv, Vector3<S>& split_vector)
  {
    split_vector = bv.axis.col(0);
  }
};

//==============================================================================
template <typename S, typename BV>
void computeSplitVector(const BV& bv, Vector3<S>& split_vector)
{
  ComputeSplitVectorImpl<S, BV>::run(bv, split_vector);
}

//==============================================================================
template <typename S>
struct ComputeSplitVectorImpl<S, kIOS<S>>
{
  static void run(const kIOS<S>& bv, Vector3<S>& split_vector)
  {
    /*
      switch(bv.num_spheres)
      {
      case 1:
      split_vector = Vector3<S>(1, 0, 0);
      break;
      case 3:
      {
      Vector3<S> v[3];
      v[0] = bv.spheres[1].o - bv.spheres[0].o;
      v[0].normalize();
      generateCoordinateSystem(v[0], v[1], v[2]);
      split_vector = v[1];
      }
      break;
      case 5:
      {
      Vector3<S> v[2];
      v[0] = bv.spheres[1].o - bv.spheres[0].o;
      v[1] = bv.spheres[3].o - bv.spheres[0].o;
      split_vector = v[0].cross(v[1]);
      split_vector.normalize();
      }
      break;
      default:
      ;
      }
    */
    split_vector = bv.obb.axis.col(0);
  }
};

//==============================================================================
template <typename S>
struct ComputeSplitVectorImpl<S, OBBRSS<S>>
{
  static void run(const OBBRSS<S>& bv, Vector3<S>& split_vector)
  {
    split_vector = bv.obb.axis.col(0);
  }
};

//==============================================================================
template <typename S, typename BV>
void computeSplitValue_bvcenter(const BV& bv, S& split_value)
{
  Vector3<S> center = bv.center();
  split_value = center[0];
}

//==============================================================================
template <typename S, typename BV>
void computeSplitValue_mean(
    const BV& bv,
    Vector3<S>* vertices,
    Triangle* triangles,
    unsigned int* primitive_indices,
    int num_primitives,
    BVHModelType type,
    const Vector3<S>& split_vector,
    S& split_value)
{
  S sum = 0.0;
  if(type == BVH_MODEL_TRIANGLES)
  {
    S c[3] = {0.0, 0.0, 0.0};

    for(int i = 0; i < num_primitives; ++i)
    {
      const Triangle& t = triangles[primitive_indices[i]];
      const Vector3<S>& p1 = vertices[t[0]];
      const Vector3<S>& p2 = vertices[t[1]];
      const Vector3<S>& p3 = vertices[t[2]];

      c[0] += (p1[0] + p2[0] + p3[0]);
      c[1] += (p1[1] + p2[1] + p3[1]);
      c[2] += (p1[2] + p2[2] + p3[2]);
    }
    split_value = (c[0] * split_vector[0] + c[1] * split_vector[1] + c[2] * split_vector[2]) / (3 * num_primitives);
  }
  else if(type == BVH_MODEL_POINTCLOUD)
  {
    for(int i = 0; i < num_primitives; ++i)
    {
      const Vector3<S>& p = vertices[primitive_indices[i]];
      Vector3<S> v(p[0], p[1], p[2]);
      sum += v.dot(split_vector);
    }

    split_value = sum / num_primitives;
  }
}

//==============================================================================
template <typename S, typename BV>
void computeSplitValue_median(
    const BV& bv,
    Vector3<S>* vertices,
    Triangle* triangles,
    unsigned int* primitive_indices,
    int num_primitives,
    BVHModelType type,
    const Vector3<S>& split_vector,
    S& split_value)
{
  std::vector<S> proj(num_primitives);

  if(type == BVH_MODEL_TRIANGLES)
  {
    for(int i = 0; i < num_primitives; ++i)
    {
      const Triangle& t = triangles[primitive_indices[i]];
      const Vector3<S>& p1 = vertices[t[0]];
      const Vector3<S>& p2 = vertices[t[1]];
      const Vector3<S>& p3 = vertices[t[2]];
      Vector3<S> centroid3(p1[0] + p2[0] + p3[0],
                      p1[1] + p2[1] + p3[1],
                      p1[2] + p2[2] + p3[2]);

      proj[i] = centroid3.dot(split_vector) / 3;
    }
  }
  else if(type == BVH_MODEL_POINTCLOUD)
  {
    for(int i = 0; i < num_primitives; ++i)
    {
      const Vector3<S>& p = vertices[primitive_indices[i]];
      Vector3<S> v(p[0], p[1], p[2]);
      proj[i] = v.dot(split_vector);
    }
  }

  std::sort(proj.begin(), proj.end());

  if(num_primitives % 2 == 1)
  {
    split_value = proj[(num_primitives - 1) / 2];
  }
  else
  {
    split_value = (proj[num_primitives / 2] + proj[num_primitives / 2 - 1]) / 2;
  }
}

} // namespace detail
} // namespace fcl

#endif
