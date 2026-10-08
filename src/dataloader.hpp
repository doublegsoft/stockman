/*                                                                                                             
**        ██               ▄▄                          
** ▄█▀▀▀ ▀██▀▀ ▄███▄ ▄████ ██ ▄█▀ ███▄███▄  ▀▀█▄ ████▄ 
** ▀███▄  ██   ██ ██ ██    ████   ██ ██ ██ ▄█▀██ ██ ██ 
** ▄▄▄█▀  ██   ▀███▀ ▀████ ██ ▀█▄ ██ ██ ██ ▀█▄██ ██ ██    
** 
** Copyright 2026 doublegsoft.me
** 
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
** 
**     http://www.apache.org/licenses/LICENSE-2.0
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

struct OhlcvRecord {
  std::uint32_t date = 0;

  double open = 0.0;
  double high = 0.0;
  double low = 0.0;
  double close = 0.0;

  float amount = 0.0f;
  std::uint32_t volume = 0;
  std::uint32_t reserved = 0;
};

class OhlcvParser {

public:
  static constexpr std::size_t RecordSize = 32;

public:
  OhlcvParser() = default;
  ~OhlcvParser() = default;

  OhlcvParser(const OhlcvParser&) = default;
  OhlcvParser& operator=(const OhlcvParser&) = default;

  bool parse(const std::string& path) {
    records_.clear();

    std::ifstream file(
      path,
      std::ios::binary | std::ios::ate
    );

    if (!file) {
      return false;
    }

    const std::streamsize size = file.tellg();

    if (size < 0 || size % RecordSize != 0) {
      return false;
    }

    file.seekg(0, std::ios::beg);

    const std::size_t count =
      static_cast<std::size_t>(size) / RecordSize;

    records_.reserve(count);

    std::uint8_t buffer[RecordSize];

    for (std::size_t i = 0; i < count; ++i) {
      if (!file.read(
            reinterpret_cast<char*>(buffer),
            RecordSize)) {
        records_.clear();
        return false;
      }

      OhlcvRecord record;

      record.date = readU32(buffer + 0);

      record.open =
        static_cast<double>(readU32(buffer + 4)) / 100.0;

      record.high =
        static_cast<double>(readU32(buffer + 8)) / 100.0;

      record.low =
        static_cast<double>(readU32(buffer + 12)) / 100.0;

      record.close =
        static_cast<double>(readU32(buffer + 16)) / 100.0;

      record.amount = readFloat(buffer + 20);

      record.volume = readU32(buffer + 24);

      record.reserved = readU32(buffer + 28);

      records_.push_back(record);
    }

    return true;
  }

  const std::vector<OhlcvRecord>& records() const {
    return records_;
  }

  std::vector<OhlcvRecord>& records() {
    return records_;
  }

  std::size_t size() const {
    return records_.size();
  }

  bool empty() const {
    return records_.empty();
  }

  const OhlcvRecord* at(std::size_t index) const {
    if (index >= records_.size()) {
      return nullptr;
    }

    return &records_[index];
  }

  OhlcvRecord* at(std::size_t index) {
    if (index >= records_.size()) {
      return nullptr;
    }

    return &records_[index];
  }

private:
  static std::uint32_t readU32(const std::uint8_t* data) {
    return
      static_cast<std::uint32_t>(data[0]) |
      (static_cast<std::uint32_t>(data[1]) << 8) |
      (static_cast<std::uint32_t>(data[2]) << 16) |
      (static_cast<std::uint32_t>(data[3]) << 24);
  }

  static float readFloat(const std::uint8_t* data) {
    const std::uint32_t value = readU32(data);

    float result;

    std::memcpy(
      &result,
      &value,
      sizeof(result)
    );

    return result;
  }

private:
  std::vector<OhlcvRecord> records_;
};