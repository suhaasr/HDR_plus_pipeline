#include "src/BurstValidation.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

#define EXPECT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << __FILE__ << ":" << __LINE__ << ": expected " #cond          \
                << std::endl;                                                  \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

FrameMetadata Frame(const std::string &path) {
  FrameMetadata f;
  f.path = path;
  f.cfa_pattern = "RGGB";
  f.width = 6064;
  f.height = 4040;
  f.black_level = {1008, 1008, 1008, 1008};
  f.white_level = 16383;
  f.make = "Nikon";
  f.model = "Z 6_2";
  f.iso = 100;
  f.shutter = 1.f / 250;
  f.aperture = 4;
  f.white_balance = {1.8f, 1.f, 1.4f};
  f.rgb_cam = {1.5f, -0.3f, -0.2f, -0.1f, 1.5f, -0.4f, 0.f, -0.4f, 1.4f};
  f.cam_xyz = {1.f, -0.3f, -0.1f, -0.5f, 1.3f, 0.2f, -0.1f, 0.2f, 0.8f};
  return f;
}

std::vector<FrameMetadata> Burst(int n = 3) {
  std::vector<FrameMetadata> frames;
  for (int i = 0; i < n; ++i) {
    frames.push_back(Frame("f" + std::to_string(i) + ".nef"));
  }
  return frames;
}

bool Mentions(const std::vector<std::string> &messages,
              const std::string &needle) {
  for (const auto &m : messages) {
    if (m.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Applies `mutate` to frame 1 and expects exactly one error mentioning `needle`.
void ExpectError(const std::string &needle,
                 const std::function<void(FrameMetadata &)> &mutate) {
  auto frames = Burst();
  mutate(frames[1]);
  const auto result = ValidateBurst(frames);
  EXPECT(result.errors.size() == 1);
  EXPECT(Mentions(result.errors, needle));
  EXPECT(Mentions(result.errors, "f1.nef"));
}

// Applies `mutate` to frame 1 and expects one warning and no errors.
void ExpectWarning(const std::string &needle,
                   const std::function<void(FrameMetadata &)> &mutate) {
  auto frames = Burst();
  mutate(frames[1]);
  const auto result = ValidateBurst(frames);
  EXPECT(result.errors.empty());
  EXPECT(result.warnings.size() == 1);
  EXPECT(Mentions(result.warnings, needle));
}

void TestValidBurst() {
  const auto result = ValidateBurst(Burst());
  EXPECT(result.errors.empty());
  EXPECT(result.warnings.empty());
}

void TestFrameCount() {
  EXPECT(Mentions(ValidateBurst({}).errors, "0 frame(s)"));
  EXPECT(Mentions(ValidateBurst(Burst(1)).errors, "1 frame(s)"));
  EXPECT(ValidateBurst(Burst(2)).errors.empty());
}

void TestPerFrameErrors() {
  ExpectError("no single-plane", [](auto &f) { f.has_raw_data = false; });
  ExpectError("not a 3-color", [](auto &f) { f.is_bayer = false; });
  ExpectError("decode error", [](auto &f) { f.decode_errors = 3; });

  auto frames = Burst();
  for (auto &f : frames) {
    f.cfa_pattern = "RGBE";
  }
  const auto result = ValidateBurst(frames);
  EXPECT(result.errors.size() == 3);
  EXPECT(Mentions(result.errors, "unsupported CFA pattern 'RGBE'"));
}

void TestMismatchErrors() {
  ExpectError("size", [](auto &f) { f.width = 8256; });
  ExpectError("size", [](auto &f) { f.height = 5504; });
  ExpectError("CFA pattern", [](auto &f) { f.cfa_pattern = "GRBG"; });
  ExpectError("black level", [](auto &f) { f.black_level[2] = 252; });
  ExpectError("bit depth", [](auto &f) { f.white_level = 4095; });
  ExpectError("orientation", [](auto &f) { f.flip = 6; });
}

void TestMismatchWarnings() {
  ExpectWarning("camera", [](auto &f) { f.model = "Z6_3"; });
  ExpectWarning("camera", [](auto &f) { f.make = "Canon"; });
  ExpectWarning("ISO", [](auto &f) { f.iso = 400; });
  ExpectWarning("shutter", [](auto &f) { f.shutter = 1.f / 60; });
  ExpectWarning("aperture", [](auto &f) { f.aperture = 5.6f; });
  ExpectWarning("white balance", [](auto &f) { f.white_balance[0] = 2.1f; });
}

void TestSmallDifferencesAreTolerated() {
  auto frames = Burst();
  frames[1].shutter *= 1.005f;
  frames[1].white_balance[2] *= 0.995f;
  const auto result = ValidateBurst(frames);
  EXPECT(result.errors.empty());
  EXPECT(result.warnings.empty());
}

void TestMissingColorProfile() {
  auto identity = Burst();
  identity[0].rgb_cam = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  EXPECT(Mentions(ValidateBurst(identity).warnings, "no color profile"));

  auto zero_xyz = Burst();
  zero_xyz[0].cam_xyz = {};
  const auto result = ValidateBurst(zero_xyz);
  EXPECT(result.errors.empty());
  EXPECT(Mentions(result.warnings, "no color profile"));
}

} // namespace

int main() {
  TestValidBurst();
  TestFrameCount();
  TestPerFrameErrors();
  TestMismatchErrors();
  TestMismatchWarnings();
  TestSmallDifferencesAreTolerated();
  TestMissingColorProfile();
  if (failures) {
    std::cerr << failures << " check(s) failed" << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "All burst validation tests passed" << std::endl;
  return EXIT_SUCCESS;
}
