// Copyright (c) 2019
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Kazys Stepanas
#ifndef RAYPATTERN_H
#define RAYPATTERN_H

#include "OhmConfig.h"

#include <glm/fwd.hpp>

#include <memory>
#include <vector>

namespace ohm
{
  struct RayPatternDetail;

  /// A @c RayPattern defines a set of ray end points with a common origin. The class may be used to define a custom
  /// point set or derived to define a pre-determined pattern.
  ///
  /// The @c RayPattern is intended for use with the @c ClearingPattern utility.
  class ohm_API RayPattern
  {
  protected:
    /// Constructor for sub-classes to derive the detail.
    /// @param detail Custom implementation or null to use the default..
    RayPattern(ohm::RayPatternDetail *detail);

  public:
    /// Create an empty ray pattern.
    RayPattern();
    /// Virtual destructor.
    virtual ~RayPattern();

    /// A a set of points to the pattern.
    /// @param points The array of points to add.
    /// @param point_count Number of elements in @p points.
    void addPoints(const glm::dvec3 *points, size_t point_count);

    /// Add a single point to the pattern.
    /// @param point The new point to add.
    inline void addPoint(const glm::dvec3 &point) { addPoints(&point, 1); }

    /// Query the number of points in the pattern.
    /// @return The number of points in the pattern.
    size_t pointCount() const;

    /// Access the point array.
    /// @return A pointer to the start of the array of points in the pattern. They are @c pointCount() in number.
    const glm::dvec3 *points() const;

    /// Build the ray set from the base pattern of points. The @p rays container is populated with pairs of start/end
    /// points which can be used with @c OccupancyMap::intergratePoints(). The first item of every pair is equal to
    /// @p position, while the second is a point from the pattern, rotated by @p rotation and translated by @p position.
    /// @param rays The ray set to populate. Cleared before use.
    /// @param position The translation for the pattern application.
    /// @param rotation The rotation for the pattern application.
    /// @param scaling Optional uniform scaling to apply to the pattern.
    /// @return The number of elements added to @p rays (twice the @c pointCount()).
    size_t buildRays(std::vector<glm::dvec3> *rays, const glm::dvec3 &position, const glm::dquat &rotation,
                     double scaling = 1.0) const;

  private:
    std::unique_ptr<ohm::RayPatternDetail> imp_;
  };
}  // namespace ohm

// 0. Pre-build pattern.
// 1. Position and rotate pattern.

#endif  // RAYPATTERN_H