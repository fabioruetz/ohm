// Copyright (c) 2017
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Kazys Stepanas
#include <gtest/gtest.h>

#include <ohm/MapCache.h>
#include <ohm/GpuMap.h>
#include <ohm/OccupancyMap.h>
#include <ohm/KeyList.h>
#include <ohm/OccupancyUtil.h>
#include <ohm/MapProbability.h>
#include <ohm/MapChunk.h>

#include <ohmtools/OhmCloud.h>
#include <ohmutil/OhmUtil.h>

#include <chrono>
#include <iostream>
#include <fstream>
#include <random>

using namespace ohm;

namespace gpumap
{
  typedef std::chrono::high_resolution_clock TimingClock;

  typedef std::function<void (OccupancyMap &, GpuMap &)> PostGpuMapTestFunc;

  bool dump_keys = false;

  void integrateRays(OccupancyMap &map, const std::vector<glm::dvec3> &rays)
  {
    OccupancyKeyList keys;
    MapCache cache;

    for (size_t i = 0; i < rays.size(); i += 2)
    {
      map.calculateSegmentKeys(keys, rays[i], rays[i + 1], false);

      for (auto &&key : keys)
      {
        map.integrateMiss(key, &cache);
        if (dump_keys)
        {
          std::cout << ". " << key << '\n';
        }
      }

      map.integrateHit(map.voxelKey(rays[i + 1]), &cache);

      if (dump_keys)
      {
          std::cout << "* " << map.voxelKey(rays[i + 1]) << '\n';
      }

      dump_keys = false;
    }
  }

  void gpuMapTest(double resolution, const glm::u8vec3 &region_size,
                  const std::vector<glm::dvec3> &rays,
                  const PostGpuMapTestFunc &post_populate,
                  const char *save_prefix = nullptr,
                  size_t batch_size = 0u,
                  size_t gpu_mem_size = 0u)
  {
    // Test basic map populate using GPU and ensure it matches CPU (close enough).
    OccupancyMap cpu_map(resolution, region_size);
    OccupancyMap gpu_map(resolution, region_size);
    GpuMap gpu_wrap(&gpu_map, true, unsigned(batch_size * 2), gpu_mem_size); // Borrow pointer.

    ASSERT_TRUE(gpu_wrap.gpuOk());

    if (!batch_size)
    {
      batch_size = rays.size() / 2;
    }

    std::cout << "Integrating " << rays.size() / 2 << " rays into each map.\n";

#if 0
    // Output to CSV for Intel Code Builder.
    {
      std::ofstream out("rays.csv");
      glm::dvec3 p = rays[0];
      out << std::setprecision(20);
      for (size_t i = 0; i < rays.size(); ++i)
      {
        p = rays[i];
        out << p.x << ',' << p.y << ',' << p.z;
        if (i + 1 < rays.size())
        {
          out << ',';
        }
        out << '\n';
      }
    }

    {
      std::ofstream out("voxels.csv");

      const MapChunk *chunk = cpuMap.region(glm::i16vec3(0, 0, 0), true);
      const uint8_t *voxelMem = (const uint8_t *)chunk->occupancy;

      for (size_t i = 0; i < cpuMap.regionVoxelVolume(); ++i)
      {
        for (size_t j = 0; j < sizeof(*chunk->occupancy); ++j)
        {
          out << int(voxelMem[j]);

          if (j + 1 < sizeof(*chunk->occupancy) || i + 1 < cpuMap.regionVoxelVolume())
          {
            out << ',' << '\n';
          }
        }
      }
      out << '\n';
    }
    return;
#endif // #

    std::cout << "GPU " << std::flush;
    const auto gpu_start = TimingClock::now();
    for (size_t i = 0; i < rays.size(); i += batch_size * 2)
    {
      const unsigned point_count = unsigned(std::min(batch_size * 2, rays.size() - i));
      gpu_wrap.integrateRays(rays.data() + i, point_count);
    }
    const auto gpu_queued = TimingClock::now();
    std::cout << gpu_queued - gpu_start << '\n';

    std::cout << "GPU sync: " << std::flush;
    gpu_wrap.syncOccupancy();
    const auto gpu_end = TimingClock::now();
    std::cout << (gpu_end - gpu_queued) << '\n';
    std::cout << "Per ray: " << (gpu_end - gpu_start) / (rays.size() / 2);
    std::cout << " queue: " << (gpu_queued - gpu_start) / (rays.size() / 2);
    std::cout << std::endl;

    std::cout << "CPU " << std::flush;
    const auto cpu_start = TimingClock::now();
    integrateRays(cpu_map, rays);
    const auto cpu_end = TimingClock::now();
    const auto cpu_elapsed = cpu_end - cpu_start;
    std::cout << cpu_elapsed << ' ';
    std::cout << cpu_elapsed / (rays.size() / 2) << " per ray\n";

    if (post_populate)
    {
      post_populate(cpu_map, gpu_wrap);
    }

    // std::cout << "Comparing" << std::endl;
    if (save_prefix)
    {
      std::string filename;
      filename = save_prefix;
      filename += "cloud-gpu.ply";
      ohmtools::saveCloud(filename.c_str(), gpu_map);
      filename = save_prefix;
      filename += "cloud-cpu.ply";
      ohmtools::saveCloud(filename.c_str(), cpu_map);
    }
  }

  void compareMaps(const OccupancyMap &reference_map, const OccupancyMap &test_map)
  {
    // We need to allow for some discrepancies as the GPU map is non-deterministic.
    const float allowed_failure_ratio = 0.01f;

    // Note: this test may be too prescriptive.
    // Iterate the CPU map and lookup the GPU map.
    unsigned failures = 0;
    unsigned processed = 0;
    unsigned logged_failures = 0;
    for (auto iter = reference_map.begin(); iter != reference_map.end(); ++iter)
    {
      if (iter->isValid() && iter->value() != ohm::VoxelBase::invalidMarkerValue())
      {
        ++processed;
        ohm::VoxelConst gpu_voxel = test_map.voxel(iter->key());
        EXPECT_TRUE(gpu_voxel.isValid());
        if (gpu_voxel.isValid() && gpu_voxel.value())
        {
          if (std::abs(iter->value() - gpu_voxel.value()) >= reference_map.hitValue() * 0.5f)
          {
            ++failures;
            if (processed >= 100 && float(failures) / float(processed) > allowed_failure_ratio && logged_failures < 1000)
            {
              EXPECT_NEAR(iter->value(), gpu_voxel.value(), reference_map.hitValue() * 0.5f);
              ++logged_failures;
            }
          }
        }
      }
    }

    if (processed)
    {
      EXPECT_LT(float(failures) / float(processed), allowed_failure_ratio);
    }
  }

  void compareCpuGpuMaps(const OccupancyMap &reference_map, const GpuMap &test_map)
  {
    return compareMaps(reference_map, test_map.map());
  }

  TEST(GpuMap, PopulateTiny)
  {
    const double resolution = 0.25;
    const unsigned batch_size = 1;
    const glm::u8vec3 region_size(32);

    // Make a ray.
    std::vector<glm::dvec3> rays;
    rays.emplace_back(glm::dvec3(0.3));
    rays.emplace_back(glm::dvec3(1.1));

    rays.emplace_back(glm::dvec3(-5.0));
    rays.emplace_back(glm::dvec3(0.3));

    gpuMapTest(resolution, region_size, rays, compareCpuGpuMaps, "tiny", batch_size);
  }

  TEST(GpuMap, PopulateSmall)
  {
    const double map_extents = 50.0;
    const double resolution = 0.25;
    const unsigned ray_count = 64;
    const unsigned batch_size = 32;
    const glm::u8vec3 region_size(32);
    // Make some rays.
    std::mt19937 rand_engine;
    std::uniform_real_distribution<double> rand(-map_extents, map_extents);
    std::vector<glm::dvec3> rays;

    while (rays.size() < ray_count * 2)
    {
      rays.emplace_back(glm::dvec3(0.05));
      rays.emplace_back(glm::dvec3(rand(rand_engine), rand(rand_engine), rand(rand_engine)));
    }

    gpuMapTest(resolution, region_size, rays, compareCpuGpuMaps, "small", batch_size);
  }

  TEST(GpuMap, PopulateLarge)
  {
    const double map_extents = 50.0;
    const double resolution = 0.25;
    const unsigned ray_count = 1024 * 128;
    const unsigned batch_size = 1024 * 2;
    const glm::u8vec3 region_size(32);
    // Make some rays.
    std::mt19937 rand_engine;
    std::uniform_real_distribution<double> rand(-map_extents, map_extents);
    std::vector<glm::dvec3> rays;

    while (rays.size() < ray_count * 2)
    {
      rays.emplace_back(glm::dvec3(0.05));
      rays.emplace_back(glm::dvec3(rand(rand_engine), rand(rand_engine), rand(rand_engine)));
    }

    gpuMapTest(resolution, region_size, rays, compareCpuGpuMaps, "large", batch_size);
  }

  TEST(GpuMap, PopulateSmallCache)
  {
    const double map_extents = 50.0;
    const double resolution = 0.25;
    const unsigned ray_count = 1024 * 8;
    const unsigned batch_size = 1024 * 2;
    const glm::u8vec3 region_size(32);
    // Make some rays.
    std::mt19937 rand_engine;
    std::uniform_real_distribution<double> rand(-map_extents, map_extents);
    std::vector<glm::dvec3> rays;

    while (rays.size() < ray_count * 2)
    {
      rays.emplace_back(glm::dvec3(0.05));
      rays.emplace_back(glm::dvec3(rand(rand_engine), rand(rand_engine), rand(rand_engine)));
    }

    // Small cache: 256MiB.
    gpuMapTest(resolution, region_size, rays, PostGpuMapTestFunc(), "small-cache-", batch_size, 256u * 1024u * 1024);
  }

  TEST(GpuMap, PopulateMultiple)
  {
    // Test having multiple GPU maps operating at once to ensure we don't get any GPU management issues.
    const double map_extents = 50.0;
    const double resolution = 0.25;
    const unsigned ray_count = 1024 * 8;
    const unsigned batch_size = 1024 * 2; // Must be even
    const glm::u8vec3 region_size(32);
    // Make some rays.
    std::mt19937 rand_engine;
    std::uniform_real_distribution<double> rand(-map_extents, map_extents);
    std::vector<glm::dvec3> rays;

    while (rays.size() < ray_count * 2)
    {
      rays.emplace_back(glm::dvec3(0.05));
      rays.emplace_back(glm::dvec3(rand(rand_engine), rand(rand_engine), rand(rand_engine)));
    }

    // Two simultaneous, maps with the same scope.
    OccupancyMap map1(resolution, region_size);
    GpuMap gpu_map1(&map1, true); // Borrow pointer.
    OccupancyMap map2(resolution, region_size);
    GpuMap gpu_map2(&map2, true); // Borrow pointer.

    // Third map with transient GpuMap wrapper.
    OccupancyMap map3(resolution, region_size);

    for (unsigned i = 0; i < rays.size(); i += batch_size)
    {
      std::cout << "\r" << i << " / " << rays.size() << std::flush;

      const unsigned remaining = unsigned(rays.size() - i);
      const unsigned current_batch_size = std::min(batch_size, remaining);
      gpu_map1.integrateRays(rays.data() + i, current_batch_size);
      gpu_map2.integrateRays(rays.data() + i, current_batch_size);

      GpuMap gpu_map3(&map3, true); // Borrow pointer.
      gpu_map3.integrateRays(rays.data() + i, current_batch_size);
      gpu_map3.syncOccupancy();

      // Forth, transient map.
      OccupancyMap map4(resolution, region_size);
      // std::cout << "\n" << map4.origin() << std::endl;
      GpuMap gpu_map4(&map4, true); // Borrow pointer.
      gpu_map4.integrateRays(rays.data() + i, current_batch_size);
      gpu_map4.syncOccupancy();
    }
    std::cout << "\r" << rays.size() << " / " << rays.size() << std::endl;

    gpu_map1.syncOccupancy();
    gpu_map2.syncOccupancy();

    std::cout << "Comparing maps" << std::endl;
    compareMaps(map1, map2);
    compareMaps(map1, map3);
  }

  TEST(GpuMap, Compare)
  {
    const double resolution = 0.25;
    const glm::u8vec3 region_size(16);
    std::vector<glm::dvec3> rays;

    // Create a map for generating voxel centres.
    OccupancyMap grid_map(resolution, region_size);
    OccupancyKey key(glm::i16vec3(0), 0, 0, 0);
    glm::dvec3 v;
    // Create a set of rays which will densely populate a single region.
    for (int z = 0; z < region_size.z; ++z)
    {
      key.setLocalAxis(2, z);
      for (int y = 0; y < region_size.y; ++y)
      {
        key.setLocalAxis(1, y);
        for (int x = 0; x < region_size.x; ++x)
        {
          key.setLocalAxis(0, x);
          v = grid_map.voxelCentreGlobal(key);
          // Create a ray starting and ending in the selected voxel.
          rays.emplace_back(v);
          rays.emplace_back(v);
        }
      }
    }

    const auto compare_results = [region_size] (OccupancyMap &cpu_map, OccupancyMap &gpu_map)
    {
      OccupancyKey key(glm::i16vec3(0), 0, 0, 0);
      VoxelConst cpu_voxel, gpu_voxel;
      // Walk the region pulling a voxel from both maps and comparing.
      for (int z = 0; z < region_size.z; ++z)
      {
        key.setLocalAxis(2, z);
        for (int y = 0; y < region_size.y; ++y)
        {
          key.setLocalAxis(1, y);
          for (int x = 0; x < region_size.x; ++x)
          {
            key.setLocalAxis(0, x);
            cpu_voxel = cpu_map.voxel(key);
            gpu_voxel = gpu_map.voxel(key);

            EXPECT_TRUE(cpu_voxel.isValid());
            EXPECT_TRUE(gpu_voxel.isValid());

            EXPECT_EQ(cpu_voxel.value(), gpu_voxel.value());

            if (cpu_voxel.value() != gpu_voxel.value())
            {
              std::cout << "Voxel error: " << key << '\n';
            }
          }
        }
      }
    };

    const auto compare_and_clear = [region_size, compare_results] (OccupancyMap &cpu_map, GpuMap &gpu_map)
    {
      compare_results(cpu_map, gpu_map.map());

      // Now we will try clear all the voxels from the bottom slice, except for those at max Y in the region.
      // To help, we adjust the miss value to greater than the hit probability and then some.
      cpu_map.setMissProbability(valueToProbability(-cpu_map.hitValue() + cpu_map.missValue()));
      gpu_map.map().setMissProbability(valueToProbability(-gpu_map.map().hitValue() + gpu_map.map().missValue()));

      // Build the clearing rays.
      std::vector<glm::dvec3> clear_rays;
      OccupancyKey from_key(glm::i16vec3(0), 0, 0, 0);
      OccupancyKey to_key(glm::i16vec3(0), 0, region_size.y - 1, 0);
      glm::dvec3 from, to;

      for (int x = 0; x < region_size.x; ++x)
      {
        from_key.setLocalAxis(0, x);
        to_key.setLocalAxis(0, x);

        from = cpu_map.voxelCentreGlobal(from_key);
        to = cpu_map.voxelCentreGlobal(to_key);

        clear_rays.emplace_back(from);
        clear_rays.emplace_back(to);
      }

      // Add the rays.
      gpu_map.integrateRays(clear_rays.data(), unsigned(clear_rays.size()));
      // dumpKeys = true;
      integrateRays(cpu_map, clear_rays);
      gpu_map.syncOccupancy();

      compare_results(cpu_map, gpu_map.map());
    };

    // gpuMapTest(resolution, regionSize, rays, compareResults, "grid-");
    gpuMapTest(resolution, region_size, rays, compare_and_clear, "grid-");
  }
}
