#include "Burst.h"

#include <iostream>
#include <stdexcept>

Halide::Runtime::Buffer<uint16_t> Burst::ToBuffer() const {
  if (Raws.empty()) {
    return Halide::Runtime::Buffer<uint16_t>();
  }

  Halide::Runtime::Buffer<uint16_t> result(GetWidth(), GetHeight(),
                                           Raws.size());
  for (size_t i = 0; i < Raws.size(); ++i) {
    auto resultSlice = result.sliced(2, i);
    Raws[i].CopyToBuffer(resultSlice);
  }
  return result;
}

void Burst::CopyToBuffer(Halide::Runtime::Buffer<uint16_t> &buffer) const {
  buffer.copy_from(ToBuffer());
}

std::vector<RawImage> Burst::LoadRaws(const std::string &dirPath,
                                      std::vector<std::string> &inputs) {
  std::vector<RawImage> result;
  for (const auto &input : inputs) {
    const std::string img_path = dirPath + "/" + input;
    result.emplace_back(img_path);
  }
  return result;
}

const RawImage &Burst::GetRaw(const size_t i) const { return this->Raws[i]; }

void Burst::Validate() const {
  std::vector<FrameMetadata> frames;
  frames.reserve(Raws.size());
  for (const auto &raw : Raws) {
    frames.push_back(raw.GetMetadata());
  }
  const ValidationResult result = ValidateBurst(frames);
  for (const auto &warning : result.warnings) {
    std::cerr << "Warning: " << warning << std::endl;
  }
  if (!result.errors.empty()) {
    std::string message = "Invalid burst:";
    for (const auto &error : result.errors) {
      std::cerr << "Error: " << error << std::endl;
      message += "\n  " + error;
    }
    throw std::invalid_argument(message);
  }
}
