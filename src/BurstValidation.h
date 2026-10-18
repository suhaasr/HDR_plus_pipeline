#pragma once

#include <array>
#include <string>
#include <vector>

// Per-frame metadata used to sanity-check a burst before processing. Kept free
// of LibRaw/Halide types so the checks can be unit tested without raw files.
struct FrameMetadata {
  std::string path;
  bool has_raw_data = true; // LibRaw produced a single-plane raw_image
  bool is_bayer = true;     // 3-color sensor with a 2x2 Bayer filter
  std::string cfa_pattern;  // e.g. "RGGB"
  int width = 0;
  int height = 0;
  std::array<float, 4> black_level{};
  int white_level = 0;
  int flip = 0;
  std::string make;
  std::string model;
  float iso = 0.f;
  float shutter = 0.f;
  float aperture = 0.f;
  std::array<float, 3> white_balance{}; // R, G, B multipliers
  std::array<float, 9> rgb_cam{};       // camera RGB -> sRGB, row-major
  std::array<float, 9> cam_xyz{};       // XYZ -> camera RGB, row-major
  int decode_errors = 0;
};

struct ValidationResult {
  // Issues that would make the output wrong or crash the pipeline.
  std::vector<std::string> errors;
  // Issues that degrade the output but still let it run.
  std::vector<std::string> warnings;
};

// Frame 0 is the reference: the pipeline applies its metadata to every frame.
ValidationResult ValidateBurst(const std::vector<FrameMetadata> &frames);
