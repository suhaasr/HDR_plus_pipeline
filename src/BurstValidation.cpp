#include "BurstValidation.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {

constexpr float kRelativeTolerance = 0.01f;

bool IsSupportedCfa(const std::string &cfa) {
  return cfa == "RGGB" || cfa == "GRBG" || cfa == "BGGR" || cfa == "GBRG";
}

bool NearlyEqual(float a, float b) {
  const float scale = std::max(std::abs(a), std::abs(b));
  return std::abs(a - b) <= kRelativeTolerance * scale;
}

template <size_t N> bool NearlyEqual(const std::array<float, N> &a,
                                     const std::array<float, N> &b) {
  for (size_t i = 0; i < N; ++i) {
    if (!NearlyEqual(a[i], b[i])) {
      return false;
    }
  }
  return true;
}

template <size_t N> bool AllZero(const std::array<float, N> &m) {
  return std::all_of(m.begin(), m.end(), [](float v) { return v == 0.f; });
}

bool IsIdentity(const std::array<float, 9> &m) {
  for (int i = 0; i < 9; ++i) {
    const float expected = (i % 4 == 0) ? 1.f : 0.f;
    if (std::abs(m[i] - expected) > 1e-4f) {
      return false;
    }
  }
  return true;
}

template <typename T> std::string Str(const T &value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

template <size_t N> std::string Str(const std::array<float, N> &values) {
  std::ostringstream out;
  for (size_t i = 0; i < N; ++i) {
    out << (i ? " " : "") << values[i];
  }
  return out.str();
}

} // namespace

ValidationResult ValidateBurst(const std::vector<FrameMetadata> &frames) {
  ValidationResult result;
  auto &errors = result.errors;
  auto &warnings = result.warnings;
  if (frames.size() < 2) {
    errors.push_back("Burst has " + Str(frames.size()) +
                       " frame(s); merging needs at least 2 to reduce noise.");
  }
  if (frames.empty()) {
    return result;
  }

  for (const auto &f : frames) {
    if (!f.has_raw_data) {
      errors.push_back(f.path +
                       ": no single-plane Bayer raw data (e.g. X-Trans, "
                         "Foveon or linear DNG).");
    } else if (!f.is_bayer) {
      errors.push_back(f.path + ": sensor is not a 3-color 2x2 Bayer layout.");
    } else if (!IsSupportedCfa(f.cfa_pattern)) {
      errors.push_back(f.path + ": unsupported CFA pattern '" +
                       f.cfa_pattern + "'.");
    }
    if (f.decode_errors > 0) {
      errors.push_back(f.path + ": LibRaw reported " + Str(f.decode_errors) +
                         " decode error(s); the file may be corrupt or use an "
                         "unsupported compression (e.g. Nikon HE/HE*).");
    }
  }

  const FrameMetadata &ref = frames[0];
  if (IsIdentity(ref.rgb_cam) || AllZero(ref.cam_xyz)) {
    warnings.push_back(
        ref.path + ": no color profile for camera '" + ref.make + " " +
        ref.model +
        "' in this LibRaw version; colors will be wrong. Try a newer LibRaw.");
  }

  for (size_t i = 1; i < frames.size(); ++i) {
    const FrameMetadata &f = frames[i];
    const auto mismatch = [&](std::vector<std::string> &out,
                              const std::string &field, const std::string &a,
                              const std::string &b,
                              const std::string &consequence) {
      out.push_back(f.path + ": " + field + " " + b + " differs from " +
                    ref.path + " (" + a + "); " + consequence);
    };

    if (f.width != ref.width || f.height != ref.height) {
      mismatch(errors, "size", Str(ref.width) + "x" + Str(ref.height),
               Str(f.width) + "x" + Str(f.height),
               "all frames must be the same size.");
    }
    if (f.cfa_pattern != ref.cfa_pattern) {
      mismatch(errors, "CFA pattern", ref.cfa_pattern, f.cfa_pattern,
               "colors will be mixed up when merging.");
    }
    if (f.black_level != ref.black_level) {
      mismatch(errors, "black level", Str(ref.black_level), Str(f.black_level),
               "frame 0's levels would be applied to all frames.");
    }
    if (f.white_level != ref.white_level) {
      mismatch(errors, "white level (bit depth)", Str(ref.white_level),
               Str(f.white_level), "frame 0's levels would be applied to all frames.");
    }
    if (f.flip != ref.flip) {
      mismatch(errors, "orientation", Str(ref.flip), Str(f.flip),
               "frames will not align.");
    }
    if (f.make != ref.make || f.model != ref.model) {
      mismatch(warnings, "camera", ref.make + " " + ref.model, f.make + " " + f.model,
               "bursts should come from a single camera.");
    }
    if (!NearlyEqual(f.iso, ref.iso)) {
      mismatch(warnings, "ISO", Str(ref.iso), Str(f.iso),
               "HDR+ expects equal exposure across frames.");
    }
    if (!NearlyEqual(f.shutter, ref.shutter)) {
      mismatch(warnings, "shutter", Str(ref.shutter) + "s", Str(f.shutter) + "s",
               "HDR+ expects equal exposure across frames.");
    }
    if (!NearlyEqual(f.aperture, ref.aperture)) {
      mismatch(warnings, "aperture", "f/" + Str(ref.aperture), "f/" + Str(f.aperture),
               "HDR+ expects equal exposure across frames.");
    }
    if (!NearlyEqual(f.white_balance, ref.white_balance)) {
      mismatch(warnings, "white balance", Str(ref.white_balance), Str(f.white_balance),
               "only frame 0's white balance is applied.");
    }
  }
  return result;
}
