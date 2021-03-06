/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011-2014, Willow Garage, Inc.
 *  Copyright (c) 2014-2016, Open Source Robotics Foundation
 *  Copyright (c) 2016, Toyota Research Institute
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

#ifndef FCL_MORTON_H
#define FCL_MORTON_H

#include "fcl/common/types.h"
#include "fcl/math/bv/AABB.h"

#include <bitset>

namespace fcl
{

/// @cond IGNORE
namespace detail
{

template <typename S>
uint32 quantize(S x, uint32 n);

/// @brief compute 30 bit morton code
static inline uint32 morton_code(uint32 x, uint32 y, uint32 z);

/// @brief compute 60 bit morton code
static inline uint64 morton_code60(uint32 x, uint32 y, uint32 z);

/// @brief Functor to compute the morton code for a given AABB<S>
/// This is specialized for 32- and 64-bit unsigned integers giving
/// a 30- or 60-bit code, respectively, and for `std::bitset<N>` where
/// N is the length of the code and must be a multiple of 3.
template<typename S, typename T>
struct morton_functor {};

/// @brief Functor to compute 30 bit morton code for a given AABB<S>
template<typename S>
struct morton_functor<S, uint32>
{
  morton_functor(const AABB<S>& bbox);

  uint32 operator() (const Vector3<S>& point) const;

  const Vector3<S> base;
  const Vector3<S> inv;

  static constexpr size_t bits();
};

/// @brief Functor to compute 60 bit morton code for a given AABB<S>
template<typename S>
struct morton_functor<S, uint64>
{
  morton_functor(const AABB<S>& bbox);

  uint64 operator() (const Vector3<S>& point) const;

  const Vector3<S> base;
  const Vector3<S> inv;

  static constexpr size_t bits();
};

/// @brief Functor to compute N bit morton code for a given AABB<S>
/// N must be a multiple of 3.
template<typename S, size_t N>
struct morton_functor<S, std::bitset<N>>
{
  static_assert(N%3==0, "Number of bits must be a multiple of 3");

  morton_functor(const AABB<S>& bbox);

  std::bitset<N> operator() (const Vector3<S>& point) const;

  const Vector3<S> base;
  const Vector3<S> inv;

  static constexpr size_t bits();
};

//============================================================================//
//                                                                            //
//                              Implementations                               //
//                                                                            //
//============================================================================//

//==============================================================================
template <typename S>
uint32 quantize(S x, uint32 n)
{
  return std::max(std::min((uint32)(x * (S)n), uint32(n-1)), uint32(0));
}

//==============================================================================
static inline uint32 morton_code(uint32 x, uint32 y, uint32 z)
{
  x = (x | (x << 16)) & 0x030000FF;
  x = (x | (x <<  8)) & 0x0300F00F;
  x = (x | (x <<  4)) & 0x030C30C3;
  x = (x | (x <<  2)) & 0x09249249;

  y = (y | (y << 16)) & 0x030000FF;
  y = (y | (y <<  8)) & 0x0300F00F;
  y = (y | (y <<  4)) & 0x030C30C3;
  y = (y | (y <<  2)) & 0x09249249;

  z = (z | (z << 16)) & 0x030000FF;
  z = (z | (z <<  8)) & 0x0300F00F;
  z = (z | (z <<  4)) & 0x030C30C3;
  z = (z | (z <<  2)) & 0x09249249;

  return x | (y << 1) | (z << 2);
}

//==============================================================================
static inline uint64 morton_code60(uint32 x, uint32 y, uint32 z)
{
  uint32 lo_x = x & 1023u;
  uint32 lo_y = y & 1023u;
  uint32 lo_z = z & 1023u;
  uint32 hi_x = x >> 10u;
  uint32 hi_y = y >> 10u;
  uint32 hi_z = z >> 10u;

  return (uint64(morton_code(hi_x, hi_y, hi_z)) << 30)
      | uint64(morton_code(lo_x, lo_y, lo_z));
}

//==============================================================================
template<typename S>
morton_functor<S, uint32>::morton_functor(const AABB<S>& bbox)
  : base(bbox.min_),
    inv(1.0 / (bbox.max_[0] - bbox.min_[0]),
    1.0 / (bbox.max_[1] - bbox.min_[1]),
    1.0 / (bbox.max_[2] - bbox.min_[2]))
{
  // Do nothing
}

//==============================================================================
template<typename S>
uint32 morton_functor<S, uint32>::operator()(const Vector3<S>& point) const
{
  uint32 x = detail::quantize((point[0] - base[0]) * inv[0], 1024u);
  uint32 y = detail::quantize((point[1] - base[1]) * inv[1], 1024u);
  uint32 z = detail::quantize((point[2] - base[2]) * inv[2], 1024u);

  return detail::morton_code(x, y, z);
}

//==============================================================================
template<typename S>
morton_functor<S, uint64>::morton_functor(const AABB<S>& bbox)
  : base(bbox.min_),
    inv(1.0 / (bbox.max_[0] - bbox.min_[0]),
    1.0 / (bbox.max_[1] - bbox.min_[1]),
    1.0 / (bbox.max_[2] - bbox.min_[2]))
{
  // Do nothing
}

//==============================================================================
template<typename S>
uint64 morton_functor<S, uint64>::operator()(const Vector3<S>& point) const
{
  uint32 x = detail::quantize((point[0] - base[0]) * inv[0], 1u << 20);
  uint32 y = detail::quantize((point[1] - base[1]) * inv[1], 1u << 20);
  uint32 z = detail::quantize((point[2] - base[2]) * inv[2], 1u << 20);

  return detail::morton_code60(x, y, z);
}

//==============================================================================
template<typename S>
constexpr size_t morton_functor<S, uint64>::bits()
{
  return 60;
}

//==============================================================================
template<typename S>
constexpr size_t morton_functor<S, uint32>::bits()
{
  return 30;
}

//==============================================================================
template<typename S, size_t N>
morton_functor<S, std::bitset<N>>::morton_functor(const AABB<S>& bbox)
  : base(bbox.min_),
    inv(1.0 / (bbox.max_[0] - bbox.min_[0]),
        1.0 / (bbox.max_[1] - bbox.min_[1]),
        1.0 / (bbox.max_[2] - bbox.min_[2]))
{
  // Do nothing
}

//==============================================================================
template<typename S, size_t N>
std::bitset<N> morton_functor<S, std::bitset<N>>::operator()(
    const Vector3<S>& point) const
{
  S x = (point[0] - base[0]) * inv[0];
  S y = (point[1] - base[1]) * inv[1];
  S z = (point[2] - base[2]) * inv[2];
  int start_bit = bits() - 1;
  std::bitset<N> bset;

  x *= 2;
  y *= 2;
  z *= 2;

  for(size_t i = 0; i < bits()/3; ++i)
  {
    bset[start_bit--] = ((z < 1) ? 0 : 1);
    bset[start_bit--] = ((y < 1) ? 0 : 1);
    bset[start_bit--] = ((x < 1) ? 0 : 1);
    x = ((x >= 1) ? 2*(x-1) : 2*x);
    y = ((y >= 1) ? 2*(y-1) : 2*y);
    z = ((z >= 1) ? 2*(z-1) : 2*z);
  }

  return bset;
}

//==============================================================================
template<typename S, size_t N>
constexpr size_t morton_functor<S, std::bitset<N>>::bits()
{
  return N;
}

} // namespace detail
/// @endcond
} // namespace fcl

#endif
