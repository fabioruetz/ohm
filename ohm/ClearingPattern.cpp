// Copyright (c) 2019
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Kazys Stepanas
#include "ClearingPattern.h"
#include "private/ClearingPatternDetail.h"

#include "OccupancyMap.h"
#include "RayPattern.h"

using namespace ohm;

ClearingPattern::ClearingPattern(const RayPattern *pattern, bool take_ownership)
  : imp_(new ClearingPatternDetail)
{
  imp_->pattern = pattern;
  imp_->has_pattern_ownership = take_ownership;
}

ClearingPattern::~ClearingPattern()
{
  if (imp_->has_pattern_ownership)
  {
    delete imp_->pattern;
  }
}

const RayPattern *ClearingPattern::pattern() const
{
  return imp_->pattern;
}

bool ClearingPattern::hasPatternOwnership() const
{
  return imp_->has_pattern_ownership;
}

void ClearingPattern::apply(OccupancyMap *map, const glm::dvec3 &position, const glm::dquat &rotation,
                            double scaling)
{
  // Reserve memory for the ray set.
  imp_->pattern->buildRays(&imp_->ray_set, position, rotation, scaling);
  map->integrateRays(imp_->ray_set.data(), imp_->ray_set.size(),
                     kRfEndPointAsFree | kRfStopOnFirstOccupied | kRfClearOnly);
}


const glm::dvec3 *ClearingPattern::buildRaySet(size_t *element_count, const glm::dvec3 &position,
                                               const glm::dquat &rotation, double scaling)
{
  imp_->pattern->buildRays(&imp_->ray_set, position, rotation, scaling);
  *element_count = imp_->ray_set.size();
  return imp_->ray_set.data();
}