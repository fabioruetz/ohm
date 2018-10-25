//
// author Kazys Stepanas
//
#include <glm/glm.hpp>

#include <ohm/MapCache.h>
#include <ohm/Key.h>
#include <ohm/KeyList.h>
#include <ohm/OccupancyMap.h>
#include <ohm/MapSerialise.h>
#include <ohm/Voxel.h>
#include <ohm/OccupancyType.h>
#include <ohmutil/PlyMesh.h>
#include <ohmutil/ProgressMonitor.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace
{
  int quit = 0;

  void onSignal(int arg)
  {
    if (arg == SIGINT || arg == SIGTERM)
    {
      ++quit;
    }
  }

  enum ExportMode
  {
    kExportOccupancy,
    kExportClearance
  };

  struct Options
  {
    std::string map_file;
    std::string ply_file;
    // expire regions older than this
    double expiry_time = 0;
    float cull_distance = 0;
    float occupancy_threshold = -1.0f;
    float colour_scale = 3.0f;
    ExportMode mode = kExportOccupancy;
  };

  template <typename NUMERIC> bool optionValue(const char *arg, int argc, char *argv[], NUMERIC &value)
  {
    std::string str_value;
    if (optionValue(arg, argc, argv, str_value))
    {
      std::istringstream instr(str_value);
      instr >> value;
      return !instr.fail();
    }

    return false;
  }

  class LoadMapProgress : public ohm::SerialiseProgress
  {
  public:
    LoadMapProgress(ProgressMonitor &monitor)
      : monitor_(monitor)
    {}

    bool quit() const override { return ::quit > 1; }

    void setTargetProgress(unsigned target) override { monitor_.beginProgress(ProgressMonitor::Info(target)); }
    void incrementProgress(unsigned inc = 1) override { monitor_.incrementProgressBy(inc); }

  private:
    ProgressMonitor &monitor_;
  };
}


// Custom option parsing. Must come before we include Options.h/cxxopt.hpp
std::istream &operator>>(std::istream &in, ExportMode &mode)
{
  std::string mode_str;
  in >> mode_str;
  if (mode_str.compare("occupancy") == 0)
  {
    mode = kExportOccupancy;
  }
  else if (mode_str.compare("clearance") == 0)
  {
    mode = kExportClearance;
  }
  // else
  // {
  //   throw cxxopts::invalid_option_format_error(modeStr);
  // }
  return in;
}

std::ostream &operator<<(std::ostream &out, const ExportMode mode)
{
  switch (mode)
  {
  case kExportOccupancy:
    out << "occupancy";
    break;
  case kExportClearance:
    out << "clearance";
    break;
  }
  return out;
}


// Must be after argument streaming operators.
#include <ohmutil/Options.h>

int parseOptions(Options &opt, int argc, char *argv[])
{
  cxxopts::Options opt_parse(argv[0], "Convert an occupancy map to a point cloud. Defaults to generate a positional "
                                     "point cloud, but can generate a clearance cloud as well.");
  opt_parse.positional_help("<map.ohm> <cloud.ply>");

  try
  {
    // Build GPU options set.
    // clang-format off
    opt_parse.add_options()
      ("help", "Show help.")
      ("colour-scale", "Colour max scaling value for colouring a clearance cloud. Max colour at this range..", cxxopts::value(opt.colour_scale))
      ("cloud", "The output cloud file (ply).", cxxopts::value(opt.ply_file))
      ("cull", "Remove regions farther than the specified distance from the map origin.", cxxopts::value(opt.cull_distance)->default_value(optStr(opt.cull_distance)))
      ("map", "The input map file (ohm).", cxxopts::value(opt.map_file))
      ("mode", "Export mode [occupancy,clearance]: select which data to export from the map.", cxxopts::value(opt.mode)->default_value(optStr(opt.mode)))
      ("expire", "Expire regions with a timestamp before the specified time. These are not exported.", cxxopts::value(opt.expiry_time))
      ("threshold", "Override the map's occupancy threshold. Only occupied points are exported.", cxxopts::value(opt.occupancy_threshold)->default_value(optStr(opt.occupancy_threshold)))
      ;
    // clang-format on

    opt_parse.parse_positional({ "map", "cloud" });

    cxxopts::ParseResult parsed = opt_parse.parse(argc, argv);

    if (parsed.count("help") || parsed.arguments().empty())
    {
      // show usage.
      std::cout << opt_parse.help({ "", "Map", "Mapping", "GPU" }) << std::endl;
      return 1;
    }

    if (opt.map_file.empty())
    {
      std::cerr << "Missing input map file name" << std::endl;
      return -1;
    }
    if (opt.ply_file.empty())
    {
      std::cerr << "Missing output file name" << std::endl;
      return -1;
    }
  }
  catch (const cxxopts::OptionException &e)
  {
    std::cerr << "Argument error\n" << e.what() << std::endl;
    return -1;
  }

  return 0;
}


int main(int argc, char *argv[])
{
  Options opt;
  std::cout.imbue(std::locale(""));

  int res = parseOptions(opt, argc, argv);

  if (res)
  {
    return res;
  }

  signal(SIGINT, onSignal);
  signal(SIGTERM, onSignal);

  std::cout << "Loading map " << opt.map_file.c_str() << std::endl;
  ProgressMonitor prog(10);
  LoadMapProgress load_progress(prog);
  ohm::OccupancyMap map(1.0f);

  prog.setDisplayFunction([](const ProgressMonitor::Progress &prog) {
    // if (!opt.quiet)
    {
      std::ostringstream out;
      out.imbue(std::locale(""));
      out << '\r';

      if (prog.info.info && prog.info.info[0])
      {
        out << prog.info.info << " : ";
      }

      out << std::setfill(' ') << std::setw(12) << prog.progress;
      if (prog.info.total)
      {
        out << " / " << std::setfill(' ') << std::setw(12) << prog.info.total;
      }
      out << "    ";
      std::cout << out.str() << std::flush;
    }
  });

  prog.startThread();
  res = ohm::load(opt.map_file.c_str(), map, &load_progress);
  prog.endProgress();

  std::cout << std::endl;

  if (res != 0)
  {
    std::cerr << "Failed to load map. Error code: " << res << std::endl;
    return res;
  }

  if (opt.occupancy_threshold >= 0)
  {
    map.setOccupancyThresholdProbability(opt.occupancy_threshold);
  }

  if (opt.cull_distance)
  {
    std::cout << "Culling regions beyond range : " << opt.cull_distance << std::endl;
    const unsigned removed = map.removeDistanceRegions(map.origin(), opt.cull_distance);
    std::cout << "Removed " << removed << " regions" << std::endl;
    ;
  }
  if (opt.expiry_time)
  {
    std::cout << "Expiring regions before time: " << opt.expiry_time << std::endl;
    unsigned removed = map.expireRegions(opt.expiry_time);
    std::cout << "Removed " << removed << " regions" << std::endl;
  }

  std::cout << "Converting to PLY cloud" << std::endl;
  PlyMesh ply;
  glm::vec3 v;
  const size_t region_count = map.regionCount();
  glm::i16vec3 last_region = map.begin().key().regionKey();
  uint64_t point_count = 0;

  prog.beginProgress(ProgressMonitor::Info(region_count));

  for (auto iter = map.begin(); iter != map.end() && !quit; ++iter)
  {
    const ohm::VoxelConst voxel = *iter;
    if (last_region != iter.key().regionKey())
    {
      prog.incrementProgress();
      last_region = iter.key().regionKey();
    }
    if (opt.mode == kExportOccupancy)
    {
      if (map.occupancyType(voxel) == ohm::Occupied)
      {
        v = map.voxelCentreLocal(voxel.key());
        ply.addVertex(v);
        ++point_count;
      }
    }
    else if (opt.mode == kExportClearance)
    {
      if (voxel.isValid() && voxel.clearance() >= 0 && voxel.clearance() < opt.colour_scale)
      {
        const float range_value = voxel.clearance();
        uint8_t c = uint8_t(255 * std::max(0.0f, (opt.colour_scale - range_value) / opt.colour_scale));
        v = map.voxelCentreLocal(voxel.key());
        ply.addVertex(v, Colour(c, 128, 0));
        ++point_count;
      }
    }
  }

  prog.endProgress();
  prog.pause();
  prog.joinThread();
  std::cout << "\nExporting " << point_count << " points" << std::endl;

  if (!quit)
  {
    ply.save(opt.ply_file.c_str(), true);
  }

  return res;
}
