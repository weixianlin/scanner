/* Copyright 2016 Carnegie Mellon University, NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "storehouse/storage_backend.h"
#include "metadata.pb.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace scanner {

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

///////////////////////////////////////////////////////////////////////////////
/// Global constants
extern i32 PUS_PER_NODE;           // # of available processing units per node
extern i32 GPUS_PER_NODE;          // # of available GPUs per node
extern i32 WORK_ITEM_SIZE;         // Base size of a work item
extern i32 TASKS_IN_QUEUE_PER_PU;  // How many tasks per PU to allocate
extern i32 LOAD_WORKERS_PER_NODE;  // # of worker threads loading data
extern i32 SAVE_WORKERS_PER_NODE;  // # of worker threads loading data
extern i32 NUM_CUDA_STREAMS;       // # of cuda streams for image processing

///////////////////////////////////////////////////////////////////////////////
/// Path functions
inline std::string database_metadata_path() { return "db_metadata.bin"; }

inline std::string dataset_descriptor_path(const std::string& dataset_name) {
  return dataset_name + "_dataset_descriptor.bin";
}

inline std::string dataset_item_data_path(const std::string& dataset_name,
                                          const std::string& item_name) {
  return dataset_name + "_dataset/" + item_name + "_data.bin";
}

inline std::string dataset_item_video_path(const std::string& dataset_name,
                                           const std::string& item_name) {
  return dataset_name + "_dataset/" + item_name + ".mp4";
}

inline std::string dataset_item_video_timestamps_path(
    const std::string& dataset_name, const std::string& item_name) {
  return dataset_name + "_dataset/" + item_name + "_web_timestamps.bin";
}

inline std::string dataset_item_metadata_path(const std::string& dataset_name,
                                              const std::string& item_name) {
  return dataset_name + "_dataset/" + item_name + "_metadata.bin";
}

inline std::string job_item_output_path(const std::string& job_name,
                                        const std::string& item_name,
                                        const std::string& layer_name,
                                        i32 start, i32 end) {
  return job_name + "_job/" + item_name + "_" + layer_name + "_" +
         std::to_string(start) + "-" + std::to_string(end) + ".bin";
}

inline std::string job_descriptor_path(const std::string& job_name) {
  return job_name + "_job_descriptor.bin";
}

inline std::string job_profiler_path(const std::string& job_name, i32 node) {
  return job_name + "_job_profiler_" + std::to_string(node) + ".bin";
}

inline i32 frames_per_work_item() {
  return WORK_ITEM_SIZE;
}

///////////////////////////////////////////////////////////////////////////////
/// Common persistent data structs and their serialization helpers
struct DatabaseMetadata {
 public:
  DatabaseMetadata();
  DatabaseMetadata(const DatabaseDescriptor& descriptor);

  const DatabaseDescriptor& get_descriptor() const;

  bool has_dataset(const std::string& dataset) const;
  bool has_dataset(i32 dataset_id) const;
  i32 get_dataset_id(const std::string& dataset) const;
  const std::string& get_dataset_name(i32 dataset_id) const;
  i32 add_dataset(const std::string& dataset);
  void remove_dataset(i32 dataset_id);

  bool has_job(const std::string& job) const;
  bool has_job(i32 job_id) const;
  i32 get_job_id(const std::string& job_name) const;
  const std::string& get_job_name(i32 job_id) const;
  i32 add_job(i32 dataset_id, const std::string& job_name);
  void remove_job(i32 job_id);

//private:
  i32 next_dataset_id;
  i32 next_job_id;
  std::map<i32, std::string> dataset_names;
  std::map<i32, std::set<i32>> dataset_job_ids;
  std::map<i32, std::string> job_names;

private:
  mutable DatabaseDescriptor descriptor;
};

enum class DeviceType {
  GPU,
  CPU,
};

struct VideoMetadata {
 public:
  VideoMetadata();
  VideoMetadata(const VideoDescriptor& descriptor);

  const VideoDescriptor& get_descriptor() const;

  i32 frames() const;
  i32 width() const;
  i32 height() const;
  std::vector<i64> keyframe_positions() const;
  std::vector<i64> keyframe_byte_offsets() const;

 private:
  mutable VideoDescriptor descriptor;
};

void serialize_database_metadata(storehouse::WriteFile* file,
                                 const DatabaseMetadata& metadata);

DatabaseMetadata deserialize_database_metadata(storehouse::RandomReadFile* file,
                                               u64& file_pos);

void serialize_dataset_descriptor(storehouse::WriteFile* file,
                                  const DatasetDescriptor& descriptor);

DatasetDescriptor deserialize_dataset_descriptor(
    storehouse::RandomReadFile* file, u64& file_pos);

void serialize_video_metadata(storehouse::WriteFile *file,
                                const VideoMetadata &metadata);

VideoMetadata deserialize_video_metadata(storehouse::RandomReadFile *file,
                                         u64 &file_pos);

void serialize_web_timestamps(storehouse::WriteFile *file,
                              const WebTimestamps &metadata);

WebTimestamps deserialize_web_timestamps(
    storehouse::RandomReadFile* file, u64& file_pos);

void serialize_job_descriptor(storehouse::WriteFile* file,
                              const JobDescriptor& descriptor);

JobDescriptor deserialize_job_descriptor(storehouse::RandomReadFile* file,
                                         u64& file_pos);
}
