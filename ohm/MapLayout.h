// Copyright (c) 2018
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Kazys Stepanas
#ifndef OHM_MAPLAYOUT_H
#define OHM_MAPLAYOUT_H

#include "OhmConfig.h"

#include "MapLayoutMatch.h"

#include <initializer_list>
#include <utility>
#include <vector>

namespace ohm
{
struct MapLayoutDetail;
class MapLayer;
struct MapChunk;

/// A @c MapLayout defines the structure of the voxel in an @c OccupancyMap stored in a @c MapChunk. Each chunk has
/// @c MapChunk::voxel_maps which store voxel data in a series of independent layers. The @c MapLayout defines the
/// number of layers and identifies the data structure of each layer (via @c MapLayer).
///
/// Each @c MapLayer is named, indexes a specific array in @c MapChunk::voxel_maps and defines the data contained
/// in the layer via a @c VoxelLayout. The @c VoxelLayout may be used to define a pseudo data structure by adding
/// named "members" of the specified type and default value. The @c MapLayer also provided functions for accessing
/// voxels from the @c MapChunk. A @c MapLayer may optionally downsample the map's default voxel resolution.
///
/// The default @c MapLayout specifies the following layers:
/// - @c DL_Occupancy - An array of float voxels identifying the occupancy for each voxel.
/// - @c DL_Clearance - An array of float ranges identifying the distance to the nearest obstruction for each voxel.
/// - @c DL_CoarseClearance - A float array downsampling the @c DL_Clearance layer (*NYI*).
/// Additional layers are user defined.
///
/// The following example code shows firstly how the default layers are created and adds user defined layer to hold
/// a data structure.
///
/// @code
/// struct UserLayerStruct
/// {
///   double timestamp;
///   float x, y, z;
/// };
///
/// void createLayers(ohm::OccupancyMap &map)
/// {
///   ohm::MapLayer *layer;
///   ohm::VoxelLayout voxel;
///   size_t clearValue = 0;
///
///   ohm::MayLayout layout = map.layout();
///   layout.clear(); // Clear the existing layout. Must be done before adding to the map.
///
///   // Setup DL_Occupancy layer.
///   // Fetch the value we'll clear voxels with (default value).
///   const float invalidMarkerValue = unobservedOccupancyValue();
///   /// Write the invalidMarkerValue value into a size_t item which will be the clear value for the member.
///   memcpy(&clearValue, &invalidMarkerValue, std::min(sizeof(invalidMarkerValue), sizeof(clearValue)));
///   // Create the occupancy layer.
///   layer = layout.addLayer(default_layer::occupancyLayerName(), 0);
///   // Get it's VoxelLayout for modification.
///   voxel = layer->voxelLayout();
///   /// Add the occupancy value member as a float. Set the default value to -1.
///   voxel.addMember(default_layer::occupancyLayerName(), DataType::Float, clearValue);
///
///   // Setup DL_Clearance
///   const float defaultClearance = -1.0f; // Default value is -1.
///   memcpy(&clearValue, &defaultClearance, std::min(sizeof(defaultClearance), sizeof(clearValue)));
///   // Create the layer.
///   layer = layout.addLayer(default_layer::clearanceLayerName(), 0);
///   voxel = layer->voxelLayout();
///   // Add clearance value member.
///   voxel.addMember(default_layer::clearanceLayerName(), DataType::Float, clearValue);
///
///   // Add a layer supporting UserLayerStruct above. Always clear to zero for these members.
///   clearValue = 0;
///   // Add the layer.
///   layer = layout.addLayer("userLayer");
///   voxel = layer->voxelLayout();
///   // Add multiple members in order matching UserLayerStruct.
///   voxel.addMember("timestamp", DataType::Double, clearValue);
///   voxel.addMember("x", DataType::Float, clearValue);
///   voxel.addMember("y", DataType::Float, clearValue);
///   voxel.addMember("z", DataType::Float, clearValue);
///   // Assert the size matches the expected size.
///   // Note: in practice packing directives may be needed on UserLayerStruct to make this true.
///   assert(sizeof(UserLayerStruct) == voxel.voxelByteSize());
/// }
///
/// UserLayerStruct *getUserLayerStruct(const ohm::Key &voxelKey, ohm::OccupancyMap &map)
/// {
///   MapChunk *chunk = map.region(voxelKey.regionKey(), false);
///   MapLayer *layer = map.layout().layer("userLayer");
///   if (!chunk || !layer)
///   {
///     return nullptr;
///   }
///
///   UserLayerStruct *voxels = return layer->voxelsAs<UserLayerStruct>(*chunk);
///   // Resolve the layer dimensions via the layer API to account for any downsampling.
///   const glm::u8vec3 layerDimensions = layer->dimensions(map.regionVoxelDimensions());
///   // Convert to a linear index into voxels.
///   const unsigned voxelIndex = ohm::voxelIndex(voxelKey, layerDimensions); // From MapChunk.h
///   return voxels[voxelIndex];
/// }
/// @endcode
class ohm_API MapLayout
{
public:
  /// Create an empty layout.
  MapLayout();
  /// Move constructor.
  /// @param other The layout to move.
  MapLayout(MapLayout &&other) noexcept;
  /// Copy constructor.
  /// @param other The layout to deep copy.
  MapLayout(const MapLayout &other);

  /// Destructor.
  ~MapLayout();

  /// Drop all layout information, resulting in an empty layout.
  void clear();

  /// Cached index to the "occupancy" layer.
  /// @return The occupancy layer index or -1 if not present.
  int occupancyLayer() const;

  /// Cached index to the "mean" layer. This layer holds @c VoxelMean data.
  /// @return The voxel mean layer index or -1 if not present.
  int meanLayer() const;

  /// Cache index to the "traversal" layer. This layer holds a single @c float per voxel which tracks the accumulated
  /// distance travelled of all rays though that voxel.
  /// @return The voxel traversal layer index or -1 if not present.
  int traversalLayer() const;

  /// Cached index to the "covariance" layer. This layer holds the @c CovarianceVoxel data.
  /// @return The voxel covariance layer index or -1 if not present.
  int covarianceLayer() const;

  /// Cached index to the "clearance" layer.
  /// @return The clearance layer index or -1 if not present.
  int clearanceLayer() const;

  /// Cached index to the "intensity" layer.
  /// @return The intensity layer index or -1 if not present.
  int intensityLayer() const;

  /// Cached index to the "hit_miss_count" layer.
  /// @return The hit miss count layer index or -1 if not present.
  int hitMissCountLayer() const;

  /// Cached index to the "semantic" layer.
  /// @return The hit miss count layer index or -1 if not present.
  int semanticLayer() const;

  /// Check if this @c MapLayout is equivalent to @p other.
  ///
  /// The layouts are may be equivalent if they share the same number of layers, the voxel patterns are the same
  /// for each layer and the clearing patterns match. The names do not have to be the same.
  ///
  /// The layouts match if all layers names and voxel layouts match.
  ///
  /// @param other The other layout to compare against.
  /// @return The equivalence @c MapLayoutMatch
  MapLayoutMatch checkEquivalent(const MapLayout &other) const;

  /// Calculate which layers from this object are also present in @p other . For any match we add an entry to @p overlap
  /// which identifies this object's layer index and the @p other object's layer index (in that order).
  ///
  /// Layers are matched first by name, then using @c MapLayer::checkEquivalent() looking for an exact match.
  ///
  /// @param[out] overlap The overlap set. Note: it is the caller's responsibility to ensure this object is empty before
  ///   calling this function.
  /// @param other The map to determine the overlap with.
  /// @return The number of layers matched between the @c MapLayout objects.
  size_t calculateOverlappingLayerSet(std::vector<std::pair<unsigned, unsigned>> &overlap,
                                      const MapLayout &other) const;

  /// Remove all layers except for the named layers. Removing interleaved layers may create gaps in the layer
  /// array. These gaps are removed with the array being repacked.
  /// @param preserve_layers Names of the layers to preserve. Exact match required.
  void filterLayers(const std::initializer_list<const char *> &preserve_layers);

  /// Remove all layers except for the identified layers. Removing interleaved layers may create gaps in the layer
  /// array. These gaps are removed with the array being repacked.
  /// @param preserve_layers Indices of the layers to preserve.
  void filterLayers(const std::initializer_list<unsigned> &preserve_layers);

  /// Add a layer to the map. The layer starts undefined and needs to have it's @c VoxelLayout populated.
  ///
  /// @param name The name of the new layer. Should be unique, but this is not checked.
  /// @param subsampling Voxel subsampling. Each increment combines 8 voxels into 1.
  /// @return The new layer. The @c MapLayer::layerIndex() serves as it's id for use with @c layer() calls.
  MapLayer *addLayer(const char *name, uint16_t subsampling = 0);

  /// Retrieve a layer by name (exact match). By iterative search.
  /// @param layer_name The name of the layer to search for.
  /// @return The first layer matching @p layerName or null if not found.
  const MapLayer *layer(const char *layer_name) const;

  /// Retrieve a layer by index.
  /// @param index The layer index in the range [0, @c layerCount()).
  /// @return The layer at the given @p index.
  const MapLayer &layer(size_t index) const;

  /// Retrieve a layer pointer by index. This allows @p index to be out of range.
  /// @param index The layer index.
  /// @return The layer at the given @p index or null if @p index is out of range [0, @c layerCount()).
  const MapLayer *layerPtr(size_t index) const;

  /// Retrieve a layer pointer by index. This allows @p index to be out of range.
  ///
  /// For internal use only. Changing a layer will invalidate a map leading to undefined behaviour.
  ///
  /// @param index The layer index.
  /// @return The layer at the given @p index or null if @p index is out of range [0, @c layerCount()).
  MapLayer *layerPtr(size_t index);

  /// Search for a layer matching @p layer_name and return it's index or -1 if not found.
  /// @param layer_name Name of the layer to search for.
  /// @return The index of the first layer matching @p layer_name or -1 if not found.
  int layerIndex(const char *layer_name) const;

  /// Retrieve the number of layers.
  /// @return The number of registered layers.
  size_t layerCount() const;

  /// Move assignment.
  /// @param other The layout to move.
  MapLayout &operator=(MapLayout &&other) noexcept;

  /// Copy assignment (deep copy).
  /// @param other The layout to copy.
  MapLayout &operator=(const MapLayout &other);

private:
  /// Cache layer index if @p layer is a known layer such as @c meanLayer() .
  /// @param layer The layer to check.
  void cacheLayerIndex(const MapLayer *layer);

  /// Cache all known layer indices such as @c meanLayer() .
  void cacheLayerIndices();

  MapLayoutDetail *imp_;
};
}  // namespace ohm

#endif  // OHM_MAPLAYOUT_H
