// Copyright (c) 2022
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Fabio Ruetz
#ifndef OHM_SEMANTIC_LABEL_H
#define OHM_SEMANTIC_LABEL_H

namespace ohm
{
typedef struct alignas(8) SemanticLabel_t  // NOLINT(readability - identifier - naming, modernize - use - using)
{
  uint16_t label;
  uint16_t state_label;
  float prob_label;
} SemanticLabel;

}  // namespace ohm


#endif
