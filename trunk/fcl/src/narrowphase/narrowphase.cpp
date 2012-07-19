/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
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
 *   * Neither the name of Willow Garage, Inc. nor the names of its
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

/** \author Jia Pan */

#include "fcl/narrowphase/narrowphase.h"
#include <boost/math/constants/constants.hpp>
#include <vector>

namespace fcl
{

namespace details
{

bool sphereSphereIntersect(const Sphere& s1, const SimpleTransform& tf1, 
                           const Sphere& s2, const SimpleTransform& tf2,
                           Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal)
{
  Vec3f diff = tf1.transform(Vec3f()) - tf2.transform(Vec3f());
  FCL_REAL len = diff.length();
  if(len > s1.radius + s2.radius)
    return false;

  if(penetration_depth) 
    *penetration_depth = s1.radius + s2.radius - len;
  if(normal) 
  {
    if(len > 0)
      *normal = diff / len;
    else
      *normal = diff;
  }

  if(contact_points)
    *contact_points = tf1.transform(Vec3f()) + diff * 0.5;
  
  return true;
}


bool sphereSphereDistance(const Sphere& s1, const SimpleTransform& tf1,
                          const Sphere& s2, const SimpleTransform& tf2,
                          FCL_REAL* dist)
{
  Vec3f diff = tf1.transform(Vec3f()) - tf2.transform(Vec3f());
  FCL_REAL len = diff.length();
  if(len > s1.radius + s2.radius)
  {
    *dist = len - (s1.radius + s2.radius);
    return true;
  }

  *dist = -1;
  return false;
}

/** \brief the minimum distance from a point to a line */
FCL_REAL segmentSqrDistance(const Vec3f& from, const Vec3f& to,const Vec3f& p, Vec3f& nearest) 
{
  Vec3f diff = p - from;
  Vec3f v = to - from;
  FCL_REAL t = v.dot(diff);
	
  if(t > 0) 
  {
    FCL_REAL dotVV = v.dot(v);
    if(t < dotVV) 
    {
      t /= dotVV;
      diff -= v * t;
    } 
    else 
    {
      t = 1;
      diff -= v;
    }
  } 
  else
    t = 0;

  nearest = from + v * t;
  return diff.dot(diff);	
}

/** \brief Whether a point's projection is in a triangle */
bool projectInTriangle(const Vec3f& p1, const Vec3f& p2, const Vec3f& p3, const Vec3f& normal, const Vec3f& p)
{
  Vec3f edge1(p2 - p1);
  Vec3f edge2(p3 - p2);
  Vec3f edge3(p1 - p3);

  Vec3f p1_to_p(p - p1);
  Vec3f p2_to_p(p - p2);
  Vec3f p3_to_p(p - p3);

  Vec3f edge1_normal(edge1.cross(normal));
  Vec3f edge2_normal(edge2.cross(normal));
  Vec3f edge3_normal(edge3.cross(normal));
	
  FCL_REAL r1, r2, r3;
  r1 = edge1_normal.dot(p1_to_p);
  r2 = edge2_normal.dot(p2_to_p);
  r3 = edge3_normal.dot(p3_to_p);
  if ( ( r1 > 0 && r2 > 0 && r3 > 0 ) ||
       ( r1 <= 0 && r2 <= 0 && r3 <= 0 ) )
    return true;
  return false;
}


bool sphereTriangleIntersect(const Sphere& s, const SimpleTransform& tf,
                             const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal_)
{
  Vec3f normal = (P2 - P1).cross(P3 - P1);
  normal.normalize();
  const Vec3f& center = tf.getTranslation();
  const FCL_REAL& radius = s.radius;
  FCL_REAL radius_with_threshold = radius + std::numeric_limits<FCL_REAL>::epsilon();
  Vec3f p1_to_center = center - P1;
  FCL_REAL distance_from_plane = p1_to_center.dot(normal);

  if(distance_from_plane < 0)
  {
    distance_from_plane *= -1;
    normal *= -1;
  }

  bool is_inside_contact_plane = (distance_from_plane < radius_with_threshold);
  
  bool has_contact = false;
  Vec3f contact_point;
  if(is_inside_contact_plane)
  {
    if(projectInTriangle(P1, P2, P3, center, normal))
    {
      has_contact = true;
      contact_point = center - normal * distance_from_plane;
    }
    else
    {
      FCL_REAL contact_capsule_radius_sqr = radius_with_threshold * radius_with_threshold;
      Vec3f nearest_on_edge;
      FCL_REAL distance_sqr;
      distance_sqr = segmentSqrDistance(P1, P2, center, nearest_on_edge);
      if(distance_sqr < contact_capsule_radius_sqr)
      {
        has_contact = true;
        contact_point = nearest_on_edge;
      }

      distance_sqr = segmentSqrDistance(P2, P3, center, nearest_on_edge);
      if(distance_sqr < contact_capsule_radius_sqr)
      {
        has_contact = true;
        contact_point = nearest_on_edge;
      }

      distance_sqr = segmentSqrDistance(P3, P1, center, nearest_on_edge);
      if(distance_sqr < contact_capsule_radius_sqr)
      {
        has_contact = true;
        contact_point = nearest_on_edge;
      }
    }
  }

  if(has_contact)
  {
    Vec3f contact_to_center = center - contact_point;
    FCL_REAL distance_sqr = contact_to_center.sqrLength();

    if(distance_sqr < radius_with_threshold * radius_with_threshold)
    {
      if(distance_sqr > 0)
      {
        FCL_REAL distance = std::sqrt(distance_sqr);
        if(normal_) *normal_ = normalize(contact_to_center);
        if(contact_points) *contact_points = contact_point;
        if(penetration_depth) *penetration_depth = -(radius - distance);
      }
      else
      {
        FCL_REAL distance = 0;
        if(normal_) *normal_ = normal;
        if(contact_points) *contact_points = contact_point;
        if(penetration_depth) *penetration_depth = -radius;
      }

      return true;
    }
  }

  return false;
}

bool sphereTriangleDistance(const Sphere& sp, const SimpleTransform& tf,
                            const Vec3f& P1, const Vec3f& P2, const Vec3f& P3,
                            FCL_REAL* dist)
{
  // from geometric tools, very different from the collision code.

  const Vec3f& center = tf.getTranslation();
  FCL_REAL radius = sp.radius;
  Vec3f diff = P1 - center;
  Vec3f edge0 = P2 - P1;
  Vec3f edge1 = P3 - P1;
  FCL_REAL a00 = edge0.sqrLength();
  FCL_REAL a01 = edge0.dot(edge1);
  FCL_REAL a11 = edge1.sqrLength();
  FCL_REAL b0 = diff.dot(edge0);
  FCL_REAL b1 = diff.dot(edge1);
  FCL_REAL c = diff.sqrLength();
  FCL_REAL det = fabs(a00*a11 - a01*a01);
  FCL_REAL s = a01*b1 - a11*b0;
  FCL_REAL t = a01*b0 - a00*b1;

  FCL_REAL sqr_dist;

  if(s + t <= det)
  {
    if(s < 0)
    {
      if(t < 0)  // region 4
      {
        if(b0 < 0)
        {
          t = 0;
          if(-b0 >= a00)
          {
            s = 1;
            sqr_dist = a00 + 2*b0 + c;
          }
          else
          {
            s = -b0/a00;
            sqr_dist = b0*s + c;
          }
        }
        else
        {
          s = 0;
          if(b1 >= 0)
          {
            t = 0;
            sqr_dist = c;
          }
          else if(-b1 >= a11)
          {
            t = 1;
            sqr_dist = a11 + 2*b1 + c;
          }
          else
          {
            t = -b1/a11;
            sqr_dist = b1*t + c;
          }
        }
      }
      else  // region 3
      {
        s = 0;
        if(b1 >= 0)
        {
          t = 0;
          sqr_dist = c;
        }
        else if(-b1 >= a11)
        {
          t = 1;
          sqr_dist = a11 + 2*b1 + c;
        }
        else
        {
          t = -b1/a11;
          sqr_dist = b1*t + c;
        }
      }
    }
    else if(t < 0)  // region 5
    {
      t = 0;
      if(b0 >= 0)
      {
        s = 0;
        sqr_dist = c;
      }
      else if(-b0 >= a00)
      {
        s = 1;
        sqr_dist = a00 + 2*b0 + c;
      }
      else
      {
        s = -b0/a00;
        sqr_dist = b0*s + c;
      }
    }
    else  // region 0
    {
      // minimum at interior point
      FCL_REAL inv_det = (1)/det;
      s *= inv_det;
      t *= inv_det;
      sqr_dist = s*(a00*s + a01*t + 2*b0) + t*(a01*s + a11*t + 2*b1) + c;
    }
  }
  else
  {
    FCL_REAL tmp0, tmp1, numer, denom;

    if(s < 0)  // region 2
    {
      tmp0 = a01 + b0;
      tmp1 = a11 + b1;
      if(tmp1 > tmp0)
      {
        numer = tmp1 - tmp0;
        denom = a00 - 2*a01 + a11;
        if(numer >= denom)
        {
          s = 1;
          t = 0;
          sqr_dist = a00 + 2*b0 + c;
        }
        else
        {
          s = numer/denom;
          t = 1 - s;
          sqr_dist = s*(a00*s + a01*t + 2*b0) + t*(a01*s + a11*t + 2*b1) + c;
        }
      }
      else
      {
        s = 0;
        if(tmp1 <= 0)
        {
          t = 1;
          sqr_dist = a11 + 2*b1 + c;
        }
        else if(b1 >= 0)
        {
          t = 0;
          sqr_dist = c;
        }
        else
        {
          t = -b1/a11;
          sqr_dist = b1*t + c;
        }
      }
    }
    else if(t < 0)  // region 6
    {
      tmp0 = a01 + b1;
      tmp1 = a00 + b0;
      if(tmp1 > tmp0)
      {
        numer = tmp1 - tmp0;
        denom = a00 - 2*a01 + a11;
        if(numer >= denom)
        {
          t = 1;
          s = 0;
          sqr_dist = a11 + 2*b1 + c;
        }
        else
        {
          t = numer/denom;
          s = 1 - t;
          sqr_dist = s*(a00*s + a01*t + 2*b0) + t*(a01*s + a11*t + 2*b1) + c;
        }
      }
      else
      {
        t = 0;
        if(tmp1 <= 0)
        {
          s = 1;
          sqr_dist = a00 + 2*b0 + c;
        }
        else if(b0 >= 0)
        {
          s = 0;
          sqr_dist = c;
        }
        else
        {
          s = -b0/a00;
          sqr_dist = b0*s + c;
        }
      }
    }
    else  // region 1
    {
      numer = a11 + b1 - a01 - b0;
      if(numer <= 0)
      {
        s = 0;
        t = 1;
        sqr_dist = a11 + 2*b1 + c;
      }
      else
      {
        denom = a00 - 2*a01 + a11;
        if(numer >= denom)
        {
          s = 1;
          t = 0;
          sqr_dist = a00 + 2*b0 + c;
        }
        else
        {
          s = numer/denom;
          t = 1 - s;
          sqr_dist = s*(a00*s + a01*t + 2*b0) + t*(a01*s + a11*t + 2*b1) + c;
        }
      }
    }
  }

  // Account for numerical round-off error.
  if(sqr_dist < 0)
    sqr_dist = 0;

  if(sqr_dist > radius * radius)
  {
    *dist = std::sqrt(sqr_dist) - radius;
    return true;
  }
  else
  {
    *dist = -1;
    return false;
  }
}



struct ContactPoint
{
  Vec3f normal;
  Vec3f point;
  FCL_REAL depth;
  ContactPoint(const Vec3f& n, const Vec3f& p, FCL_REAL d) : normal(n), point(p), depth(d) {}
};


static inline void lineClosestApproach(const Vec3f& pa, const Vec3f& ua,
                                       const Vec3f& pb, const Vec3f& ub,
                                       FCL_REAL* alpha, FCL_REAL* beta)
{
  Vec3f p = pb - pa;
  FCL_REAL uaub = ua.dot(ub);
  FCL_REAL q1 = ua.dot(p);
  FCL_REAL q2 = -ub.dot(p);
  FCL_REAL d = 1 - uaub * uaub;
  if(d <= (FCL_REAL)(0.0001f))
  {
    *alpha = 0;
    *beta = 0;
  }
  else
  {
    d = 1 / d;
    *alpha = (q1 + uaub * q2) * d;
    *beta = (uaub * q1 + q2) * d;
  }
}

// find all the intersection points between the 2D rectangle with vertices
// at (+/-h[0],+/-h[1]) and the 2D quadrilateral with vertices (p[0],p[1]),
// (p[2],p[3]),(p[4],p[5]),(p[6],p[7]).
//
// the intersection points are returned as x,y pairs in the 'ret' array.
// the number of intersection points is returned by the function (this will
// be in the range 0 to 8).
static int intersectRectQuad2(FCL_REAL h[2], FCL_REAL p[8], FCL_REAL ret[16])
{
  // q (and r) contain nq (and nr) coordinate points for the current (and
  // chopped) polygons
  int nq = 4, nr = 0;
  FCL_REAL buffer[16];
  FCL_REAL* q = p;
  FCL_REAL* r = ret;
  for(int dir = 0; dir <= 1; ++dir) 
  {
    // direction notation: xy[0] = x axis, xy[1] = y axis
    for(int sign = -1; sign <= 1; sign += 2) 
    {
      // chop q along the line xy[dir] = sign*h[dir]
      FCL_REAL* pq = q;
      FCL_REAL* pr = r;
      nr = 0;
      for(int i = nq; i > 0; --i) 
      {
	// go through all points in q and all lines between adjacent points
	if(sign * pq[dir] < h[dir]) 
        {
	  // this point is inside the chopping line
	  pr[0] = pq[0];
	  pr[1] = pq[1];
	  pr += 2;
	  nr++;
	  if(nr & 8) 
          {
	    q = r;
	    goto done;
	  }
	}
	FCL_REAL* nextq = (i > 1) ? pq+2 : q;
	if((sign*pq[dir] < h[dir]) ^ (sign*nextq[dir] < h[dir])) 
        {
	  // this line crosses the chopping line
	  pr[1-dir] = pq[1-dir] + (nextq[1-dir]-pq[1-dir]) /
	    (nextq[dir]-pq[dir]) * (sign*h[dir]-pq[dir]);
	  pr[dir] = sign*h[dir];
	  pr += 2;
	  nr++;
	  if(nr & 8) 
          {
	    q = r;
	    goto done;
	  }
	}
	pq += 2;
      }
      q = r;
      r = (q == ret) ? buffer : ret;
      nq = nr;
    }
  }
 
 done:
  if(q != ret) memcpy(ret, q, nr*2*sizeof(FCL_REAL));
  return nr;  
}

// given n points in the plane (array p, of size 2*n), generate m points that
// best represent the whole set. the definition of 'best' here is not
// predetermined - the idea is to select points that give good box-box
// collision detection behavior. the chosen point indexes are returned in the
// array iret (of size m). 'i0' is always the first entry in the array.
// n must be in the range [1..8]. m must be in the range [1..n]. i0 must be
// in the range [0..n-1].
static inline void cullPoints2(int n, FCL_REAL p[], int m, int i0, int iret[])
{
  // compute the centroid of the polygon in cx,cy
  FCL_REAL a, cx, cy, q;
  switch(n)
  {
  case 1:
    cx = p[0];
    cy = p[1];
    break;
  case 2:
    cx = 0.5 * (p[0] + p[2]);
    cy = 0.5 * (p[1] + p[3]);
    break;
  default:
    a = 0;
    cx = 0;
    cy = 0;
    for(int i = 0; i < n-1; ++i) 
    {
      q = p[i*2]*p[i*2+3] - p[i*2+2]*p[i*2+1];
      a += q;
      cx += q*(p[i*2]+p[i*2+2]);
      cy += q*(p[i*2+1]+p[i*2+3]);
    }
    q = p[n*2-2]*p[1] - p[0]*p[n*2-1];
    if(std::abs(a+q) > std::numeric_limits<FCL_REAL>::epsilon())
      a = 1/(3*(a+q));
    else
      a= 1e18f;
    
    cx = a*(cx + q*(p[n*2-2]+p[0]));
    cy = a*(cy + q*(p[n*2-1]+p[1]));
  }


  // compute the angle of each point w.r.t. the centroid
  FCL_REAL A[8];
  for(int i = 0; i < n; ++i) 
    A[i] = atan2(p[i*2+1]-cy,p[i*2]-cx);

  // search for points that have angles closest to A[i0] + i*(2*pi/m).
  int avail[8];
  for(int i = 0; i < n; ++i) avail[i] = 1;
  avail[i0] = 0;
  iret[0] = i0;
  iret++;
  const double pi = boost::math::constants::pi<FCL_REAL>();
  for(int j = 1; j < m; ++j) 
  {
    a = j*(2*pi/m) + A[i0];
    if (a > pi) a -= 2*pi;
    FCL_REAL maxdiff= 1e9, diff;

    *iret = i0;	// iret is not allowed to keep this value, but it sometimes does, when diff=#QNAN0
    for(int i = 0; i < n; ++i) 
    {
      if(avail[i]) 
      {
	diff = std::abs(A[i]-a);
	if(diff > pi) diff = 2*pi - diff;
	if(diff < maxdiff) 
        {
	  maxdiff = diff;
	  *iret = i;
	}
      }
    }
    avail[*iret] = 0;
    iret++;
  }
}



int boxBox2(const Vec3f& side1, const Matrix3f& R1, const Vec3f& T1,
            const Vec3f& side2, const Matrix3f& R2, const Vec3f& T2,
            Vec3f& normal, FCL_REAL* depth, int* return_code,
            int maxc, std::vector<ContactPoint>& contacts)
{
  const FCL_REAL fudge_factor = FCL_REAL(1.05);
  Vec3f normalC;
  FCL_REAL s, s2, l;
  int invert_normal, code;

  Vec3f p = T2 - T1; // get vector from centers of box 1 to box 2, relative to box 1
  Vec3f pp = R1.transposeTimes(p); // get pp = p relative to body 1

  // get side lengths / 2
  Vec3f A = side1 * 0.5;
  Vec3f B = side2 * 0.5;

  // Rij is R1'*R2, i.e. the relative rotation between R1 and R2
  Matrix3f R = R1.transposeTimes(R2);
  Matrix3f Q = abs(R);


  // for all 15 possible separating axes:
  //   * see if the axis separates the boxes. if so, return 0.
  //   * find the depth of the penetration along the separating axis (s2)
  //   * if this is the largest depth so far, record it.
  // the normal vector will be set to the separating axis with the smallest
  // depth. note: normalR is set to point to a column of R1 or R2 if that is
  // the smallest depth normal so far. otherwise normalR is 0 and normalC is
  // set to a vector relative to body 1. invert_normal is 1 if the sign of
  // the normal should be flipped.

  int best_col_id = -1;
  FCL_REAL tmp = 0;

  s = - std::numeric_limits<FCL_REAL>::max();
  invert_normal = 0;
  code = 0;

  // separating axis = u1, u2, u3
  tmp = pp[0];
  s2 = std::abs(tmp) - (Q.dotX(B) + A[0]);
  if(s2 > 0) { *return_code = 0; return 0; }
  if(s2 > s) 
  {
    s = s2; 
    best_col_id = 0;
    invert_normal = (tmp < 0);
    code = 1;
  }

  tmp = pp[1]; 
  s2 = std::abs(tmp) - (Q.dotY(B) + A[1]);
  if(s2 > 0) { *return_code = 0; return 0; }
  if(s2 > s) 
  {
    s = s2; 
    best_col_id = 1;
    invert_normal = (tmp < 0);
    code = 2;
  }

  tmp = pp[2];
  s2 = std::abs(tmp) - (Q.dotZ(B) + A[2]);
  if(s2 > 0) { *return_code = 0; return 0; }
  if(s2 > s)
  {
    s = s2;
    best_col_id = 2;
    invert_normal = (tmp < 0);
    code = 3;
  }

  // separating axis = v1, v2, v3
  tmp = R2.transposeDotX(p);
  s2 = std::abs(tmp) - (Q.transposeDotX(A) + B[0]);
  if(s2 > 0) { *return_code = 0; return 0; }
  if(s2 > s)
  {
    s = s2;
    best_col_id = 0;
    code = 4;
  }

  tmp = R2.transposeDotY(p);
  s2 = std::abs(tmp) - (Q.transposeDotY(A) + B[1]);
  if(s2 > 0) { *return_code = 0; return 0; }
  if(s2 > s)
  {
    s = s2;
    best_col_id = 1;
    code = 5;
  }

  tmp = R2.transposeDotZ(p);
  s2 =  std::abs(tmp) - (Q.transposeDotZ(A) + B[2]);
  if(s2 > 0) { *return_code = 0; return 0; }
  if(s2 > s)
  {
    s = s2;
    best_col_id = 2;
    code = 6;
  }
  

  FCL_REAL fudge2(1.0e-6);
  Q += fudge2;

  Vec3f n;
  FCL_REAL eps = std::numeric_limits<FCL_REAL>::epsilon();

  // separating axis = u1 x (v1,v2,v3)
  tmp = pp[2] * R(1, 0) - pp[1] * R(2, 0);
  s2 = std::abs(tmp) - (A[1] * Q(2, 0) + A[2] * Q(1, 0) + B[1] * Q(0, 2) + B[2] * Q(0, 1));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(0, -R(2, 0), R(1, 0));
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 7;
    }
  }

  tmp = pp[2] * R(1, 1) - pp[1] * R(2, 1);
  s2 = std::abs(tmp) - (A[1] * Q(2, 1) + A[2] * Q(1, 1) + B[0] * Q(0, 2) + B[2] * Q(0, 0));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(0, -R(2, 1), R(1, 1));
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 8;
    }
  }
  
  tmp = pp[2] * R(1, 2) - pp[1] * R(2, 2);
  s2 = std::abs(tmp) - (A[1] * Q(2, 2) + A[2] * Q(1, 2) + B[0] * Q(0, 1) + B[1] * Q(0, 0));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(0, -R(2, 2), R(1, 2));
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 9;
    }
  }

  // separating axis = u2 x (v1,v2,v3)
  tmp = pp[0] * R(2, 0) - pp[2] * R(0, 0);
  s2 = std::abs(tmp) - (A[0] * Q(2, 0) + A[2] * Q(0, 0) + B[1] * Q(1, 2) + B[2] * Q(1, 1));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(R(2, 0), 0, -R(0, 0));
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 10;
    }
  }

  tmp = pp[0] * R(2, 1) - pp[2] * R(0, 1);
  s2 = std::abs(tmp) - (A[0] * Q(2, 1) + A[2] * Q(0, 1) + B[0] * Q(1, 2) + B[2] * Q(1, 0));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(R(2, 1), 0, -R(0, 1));
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 11;
    }
  }
  
  tmp = pp[0] * R(2, 2) - pp[2] * R(0, 2);
  s2 = std::abs(tmp) - (A[0] * Q(2, 2) + A[2] * Q(0, 2) + B[0] * Q(1, 1) + B[1] * Q(1, 0));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(R(2, 2), 0, -R(0, 2));
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 12;
    }
  }

  // separating axis = u3 x (v1,v2,v3)
  tmp = pp[1] * R(0, 0) - pp[0] * R(1, 0);
  s2 = std::abs(tmp) - (A[0] * Q(1, 0) + A[1] * Q(0, 0) + B[1] * Q(2, 2) + B[2] * Q(2, 1));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(-R(1, 0), R(0, 0), 0);
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 13;
    }
  }

  tmp = pp[1] * R(0, 1) - pp[0] * R(1, 1);
  s2 = std::abs(tmp) - (A[0] * Q(1, 1) + A[1] * Q(0, 1) + B[0] * Q(2, 2) + B[2] * Q(2, 0));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(-R(1, 1), R(0, 1), 0);
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 14;
    }
  }
  
  tmp = pp[1] * R(0, 2) - pp[0] * R(1, 2);
  s2 = std::abs(tmp) - (A[0] * Q(1, 2) + A[1] * Q(0, 2) + B[0] * Q(2, 1) + B[1] * Q(2, 0));
  if(s2 > eps) { *return_code = 0; return 0; }
  n = Vec3f(-R(1, 2), R(0, 2), 0);
  l = n.length();
  if(l > eps) 
  {
    s2 /= l;
    if(s2 * fudge_factor > s)
    {
      s = s2;
      best_col_id = 0;
      normalC = n / l;
      invert_normal = (tmp < 0);
      code = 15;
    }
  }


  
  if (!code) { *return_code = code; return 0; }

  // if we get to this point, the boxes interpenetrate. compute the normal
  // in global coordinates.
  if(best_col_id != -1) 
    normal = R.getColumn(best_col_id);
  else 
    normal = R1 * normalC;
  
  if(invert_normal) 
    normal.negate();

  *depth = -s;

  // compute contact point(s)

  if(code > 6) 
  {
    // an edge from box 1 touches an edge from box 2.
    // find a point pa on the intersecting edge of box 1
    Vec3f pa(T1);
    FCL_REAL sign;
  
    for(int j = 0; j < 3; ++j)
    {
      sign = (R1.transposeDot(j, normal) > 0) ? 1 : -1;
      pa += R1.getColumn(j) * (A[j] * sign);
    }
  
    // find a point pb on the intersecting edge of box 2
    Vec3f pb;
    pb = T2;
    for(int j = 0; j < 3; ++j)
    {
      sign = (R2.transposeDot(j, normal) > 0) ? 1 : -1;
      pb += R2.getColumn(j) * (B[j] * sign);
    }

    FCL_REAL alpha, beta;
    Vec3f ua(R1.getColumn((code-7)/3));
    Vec3f ub(R2.getColumn((code-7)%3));
    
    lineClosestApproach(pa, ua, pb, ub, &alpha, &beta);
    pa += ua * alpha;
    pb += ub * beta;
    
    
    Vec3f pointInWorld((pa + pb) * 0.5);
    contacts.push_back(ContactPoint(-normal, pointInWorld, -*depth));
    *return_code = code;
    
    return 1;
  }

  // okay, we have a face-something intersection (because the separating
  // axis is perpendicular to a face). define face 'a' to be the reference
  // face (i.e. the normal vector is perpendicular to this) and face 'b' to be
  // the incident face (the closest face of the other box).
  
  const Matrix3f *Ra, *Rb;
  const Vec3f *pa, *pb, *Sa, *Sb;

  if(code <= 3)
  {
    Ra = &R1;
    Rb = &R2;
    pa = &T1;
    pb = &T2;
    Sa = &A;
    Sb = &B;
  }
  else
  {
    Ra = &R2;
    Rb = &R1;
    pa = &T2;
    pb = &T1;
    Sa = &B;
    Sb = &A;
  }

  // nr = normal vector of reference face dotted with axes of incident box.
  // anr = absolute values of nr.
  Vec3f normal2, nr, anr;
  if(code <= 3) 
    normal2 = normal;
  else 
    normal2 = -normal;

  nr = Rb->transposeTimes(normal2);
  anr = abs(anr);

  // find the largest compontent of anr: this corresponds to the normal
  // for the indident face. the other axis numbers of the indicent face
  // are stored in a1,a2.
  int lanr, a1, a2;
  if(anr[1] > anr[0]) 
  {
    if(anr[1] > anr[2]) 
    {
      a1 = 0;
      lanr = 1;
      a2 = 2;
    }
    else 
    {
      a1 = 0;
      a2 = 1;
      lanr = 2;
    }
  }
  else 
  {
    if(anr[0] > anr[2]) 
    {
      lanr = 0;
      a1 = 1;
      a2 = 2;
    }
    else 
    {
      a1 = 0;
      a2 = 1;
      lanr = 2;
    }
  }

  // compute center point of incident face, in reference-face coordinates
  Vec3f center;
  if(nr[lanr] < 0) 
    center = (*pb) - (*pa) + Rb->getColumn(lanr) * ((*Sb)[lanr]);
  else
    center = (*pb) - (*pa) - Rb->getColumn(lanr) * ((*Sb)[lanr]);

  // find the normal and non-normal axis numbers of the reference box
  int codeN, code1, code2;
  if(code <= 3) 
    codeN = code-1; 
  else codeN = code-4;
  
  if(codeN == 0) 
  {
    code1 = 1;
    code2 = 2;
  }
  else if(codeN == 1) 
  {
    code1 = 0;
    code2 = 2;
  }
  else 
  {
    code1 = 0;
    code2 = 1;
  }

  // find the four corners of the incident face, in reference-face coordinates
  FCL_REAL quad[8]; // 2D coordinate of incident face (x,y pairs)
  FCL_REAL c1, c2, m11, m12, m21, m22;
  c1 = Ra->transposeDot(code1, center);
  c2 = Ra->transposeDot(code2, center);
  // optimize this? - we have already computed this data above, but it is not
  // stored in an easy-to-index format. for now it's quicker just to recompute
  // the four dot products.
  Vec3f tempRac = Ra->getColumn(code1);
  m11 = Rb->transposeDot(a1, tempRac);
  m12 = Rb->transposeDot(a2, tempRac);
  tempRac = Ra->getColumn(code2);
  m21 = Rb->transposeDot(a1, tempRac);
  m22 = Rb->transposeDot(a2, tempRac);
 
  FCL_REAL k1 = m11 * (*Sb)[a1];
  FCL_REAL k2 = m21 * (*Sb)[a1];
  FCL_REAL k3 = m12 * (*Sb)[a2];
  FCL_REAL k4 = m22 * (*Sb)[a2];
  quad[0] = c1 - k1 - k3;
  quad[1] = c2 - k2 - k4;
  quad[2] = c1 - k1 + k3;
  quad[3] = c2 - k2 + k4;
  quad[4] = c1 + k1 + k3;
  quad[5] = c2 + k2 + k4;
  quad[6] = c1 + k1 - k3;
  quad[7] = c2 + k2 - k4;

  // find the size of the reference face
  FCL_REAL rect[2];
  rect[0] = (*Sa)[code1];
  rect[1] = (*Sa)[code2];

  // intersect the incident and reference faces
  FCL_REAL ret[16];
  int n_intersect = intersectRectQuad2(rect, quad, ret);
  if(n_intersect < 1) { *return_code = code; return 0; } // this should never happen

  // convert the intersection points into reference-face coordinates,
  // and compute the contact position and depth for each point. only keep
  // those points that have a positive (penetrating) depth. delete points in
  // the 'ret' array as necessary so that 'point' and 'ret' correspond.
  Vec3f points[8]; // penetrating contact points
  FCL_REAL dep[8]; // depths for those points
  FCL_REAL det1 = 1.f/(m11*m22 - m12*m21);
  m11 *= det1;
  m12 *= det1;
  m21 *= det1;
  m22 *= det1;
  int cnum = 0;	// number of penetrating contact points found
  for(int j = 0; j < n_intersect; ++j) 
  {
    FCL_REAL k1 =  m22*(ret[j*2]-c1) - m12*(ret[j*2+1]-c2);
    FCL_REAL k2 = -m21*(ret[j*2]-c1) + m11*(ret[j*2+1]-c2);
    points[cnum] = center + Rb->getColumn(a1) * k1 + Rb->getColumn(a2) * k2;
    dep[cnum] = (*Sa)[codeN] - normal2.dot(points[cnum]);
    if(dep[cnum] >= 0) 
    {
      ret[cnum*2] = ret[j*2];
      ret[cnum*2+1] = ret[j*2+1];
      cnum++;
    }
  }
  if(cnum < 1) { *return_code = code; return 0; } // this should never happen

  // we can't generate more contacts than we actually have
  if(maxc > cnum) maxc = cnum;
  if(maxc < 1) maxc = 1;

  if(cnum <= maxc) 
  {
    if(code<4) 
    {
      // we have less contacts than we need, so we use them all
      for(int j = 0; j < cnum; ++j) 
      {
        Vec3f pointInWorld = points[j] + (*pa);
        contacts.push_back(ContactPoint(-normal, pointInWorld, -dep[j]));
      }
    } 
    else
    {
      // we have less contacts than we need, so we use them all
      for(int j = 0; j < cnum; ++j) 
      {
        Vec3f pointInWorld = points[j] + (*pa) - normal * dep[j];
        contacts.push_back(ContactPoint(-normal, pointInWorld, -dep[j]));
      }
    }
  }
  else 
  {
    // we have more contacts than are wanted, some of them must be culled.
    // find the deepest point, it is always the first contact.
    int i1 = 0;
    FCL_REAL maxdepth = dep[0];
    for(int i = 1; i < cnum; ++i) 
    {
      if(dep[i] > maxdepth) 
      {
	maxdepth = dep[i];
	i1 = i;
      }
    }

    int iret[8];
    cullPoints2(cnum, ret, maxc, i1, iret);

    for(int j = 0; j < maxc; ++j) 
    {
      Vec3f posInWorld = points[iret[j] * 3] + (*pa);
      if(code < 4)
        contacts.push_back(ContactPoint(-normal, posInWorld, -dep[iret[j]]));
      else
        contacts.push_back(ContactPoint(-normal, posInWorld - normal * dep[iret[j]], -dep[iret[j]]));
    }
    cnum = maxc;
  }

  *return_code = code;
  return cnum;
}



bool boxBoxIntersect(const Box& s1, const SimpleTransform& tf1,
                     const Box& s2, const SimpleTransform& tf2,
                     Vec3f* contact_points, FCL_REAL* penetration_depth_, Vec3f* normal_)
{
  std::vector<ContactPoint> contacts;
  int return_code; 
  Vec3f normal;
  FCL_REAL depth;
  int cnum = boxBox2(s1.side, tf1.getRotation(), tf1.getTranslation(),
                     s2.side, tf2.getRotation(), tf2.getTranslation(),
                     normal, &depth, &return_code,
                     4, contacts);

  if(normal_) *normal_ = normal;
  if(penetration_depth_) *penetration_depth_ = depth;

  if(contact_points)
  {
    Vec3f contact_point;
    for(size_t i = 0; i < contacts.size(); ++i)
    {
      contact_point += contacts[i].point;
    }

    contact_point = contact_point / (FCL_REAL)contacts.size();

    *contact_points = contact_point;
  }

  return return_code != 0;
}




} // details


template<>
bool GJKSolver_libccd::shapeIntersect<Sphere, Sphere>(const Sphere& s1, const SimpleTransform& tf1,
                                                      const Sphere& s2, const SimpleTransform& tf2,
                                                      Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::sphereSphereIntersect(s1, tf1, s2, tf2, contact_points, penetration_depth, normal);
}

template<> 
bool GJKSolver_libccd::shapeTriangleIntersect(const Sphere& s, const SimpleTransform& tf,
                                              const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::sphereTriangleIntersect(s, tf, P1, P2, P3, contact_points, penetration_depth, normal);
}

template<> 
bool GJKSolver_libccd::shapeTriangleIntersect(const Sphere& s, const SimpleTransform& tf,
                                              const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, const Matrix3f& R, const Vec3f& T, Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::sphereTriangleIntersect(s, tf, R * P1 + T, R * P2 + T, R * P3 + T, contact_points, penetration_depth, normal);
}

template<>
bool GJKSolver_libccd::shapeDistance<Sphere, Sphere>(const Sphere& s1, const SimpleTransform& tf1,
                                                     const Sphere& s2, const SimpleTransform& tf2,
                                                     FCL_REAL* dist) const
{
  return details::sphereSphereDistance(s1, tf1, s2, tf2, dist);
}

template<>
bool GJKSolver_libccd::shapeTriangleDistance<Sphere>(const Sphere& s, const SimpleTransform& tf,
                                                     const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, 
                                                     FCL_REAL* dist) const
{
  return details::sphereTriangleDistance(s, tf, P1, P2, P3, dist);
}

template<> 
bool GJKSolver_libccd::shapeTriangleDistance<Sphere>(const Sphere& s, const SimpleTransform& tf, 
                                                     const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, const Matrix3f& R, const Vec3f& T,
                                                     FCL_REAL* dist) const
{
  return details::sphereTriangleDistance(s, tf, R * P1 + T, R * P2 + T, R * P3 + T, dist);
}





template<>
bool GJKSolver_indep::shapeIntersect<Sphere, Sphere>(const Sphere& s1, const SimpleTransform& tf1,
                                                      const Sphere& s2, const SimpleTransform& tf2,
                                                      Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::sphereSphereIntersect(s1, tf1, s2, tf2, contact_points, penetration_depth, normal);
}

template<> 
bool GJKSolver_indep::shapeTriangleIntersect(const Sphere& s, const SimpleTransform& tf,
                                             const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::sphereTriangleIntersect(s, tf, P1, P2, P3, contact_points, penetration_depth, normal);
}

template<> 
bool GJKSolver_indep::shapeTriangleIntersect(const Sphere& s, const SimpleTransform& tf,
                                             const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, const Matrix3f& R, const Vec3f& T, Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::sphereTriangleIntersect(s, tf, R * P1 + T, R * P2 + T, R * P3 + T, contact_points, penetration_depth, normal);
}


template<>
bool GJKSolver_indep::shapeDistance<Sphere, Sphere>(const Sphere& s1, const SimpleTransform& tf1,
                                                    const Sphere& s2, const SimpleTransform& tf2,
                                                    FCL_REAL* dist) const
{
  return details::sphereSphereDistance(s1, tf1, s2, tf2, dist);
}


template<>
bool GJKSolver_indep::shapeTriangleDistance<Sphere>(const Sphere& s, const SimpleTransform& tf,
                                                    const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, 
                                                    FCL_REAL* dist) const
{
  return details::sphereTriangleDistance(s, tf, P1, P2, P3, dist);
}

template<> 
bool GJKSolver_indep::shapeTriangleDistance<Sphere>(const Sphere& s, const SimpleTransform& tf, 
                                                    const Vec3f& P1, const Vec3f& P2, const Vec3f& P3, const Matrix3f& R, const Vec3f& T,
                                                    FCL_REAL* dist) const
{
  return details::sphereTriangleDistance(s, tf, R * P1 + T, R * P2 + T, R * P3 + T, dist);
}


template<>
bool GJKSolver_libccd::shapeIntersect<Box, Box>(const Box& s1, const SimpleTransform& tf1,
                                                const Box& s2, const SimpleTransform& tf2,
                                                Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::boxBoxIntersect(s1, tf1, s2, tf2, contact_points, penetration_depth, normal);
}

template<>
bool GJKSolver_indep::shapeIntersect<Box, Box>(const Box& s1, const SimpleTransform& tf1,
                                                const Box& s2, const SimpleTransform& tf2,
                                                Vec3f* contact_points, FCL_REAL* penetration_depth, Vec3f* normal) const
{
  return details::boxBoxIntersect(s1, tf1, s2, tf2, contact_points, penetration_depth, normal);
}



} // fcl