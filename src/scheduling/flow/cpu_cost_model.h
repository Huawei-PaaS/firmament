/*
 * Firmament
 * Copyright (c) The Firmament Authors.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR
 * A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

#ifndef FIRMAMENT_SCHEDULING_CPU_COST_MODEL_H
#define FIRMAMENT_SCHEDULING_CPU_COST_MODEL_H

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/common.h"
#include "base/types.h"
#include "misc/map-util.h"
#include "scheduling/common.h"
#include "scheduling/flow/cost_model_interface.h"
#include "scheduling/knowledge_base.h"

namespace firmament {

struct CpuMemCostVector_t {
  // record number of dimensions here
  static const int16_t dimensions_ = 3;
  uint64_t cpu_mem_cost_;
  uint64_t balanced_res_cost_;
  uint64_t node_affinity_soft_cost_;
  CpuMemCostVector_t()
      : cpu_mem_cost_(0), balanced_res_cost_(0), node_affinity_soft_cost_(0) {}
};

struct CpuMemResVector_t {
  uint64_t cpu_cores_;
  uint64_t ram_cap_;
};

struct MinMaxScore_t {
  int64_t min_score;
  int64_t max_score;
  MinMaxScore_t() : min_score(-1), max_score(-1) {}
};

struct MinMaxScores_t {
  MinMaxScore_t node_affinity_priority;
  MinMaxScore_t pod_affinity_priority;
};

struct PriorityScore_t {
  // Flag that indicates whether soft constraints are satisfied or not.
  bool satisfy;
  int64_t score;
  int64_t final_score;
  PriorityScore_t() : satisfy(true), score(0), final_score(-1) {}
};

struct PriorityScoresList_t {
  PriorityScore_t node_affinity_priority;
  PriorityScore_t pod_affinity_priority;
};

class CpuCostModel : public CostModelInterface {
 public:
  CpuCostModel(shared_ptr<ResourceMap_t> resource_map,
               shared_ptr<TaskMap_t> task_map,
               shared_ptr<KnowledgeBase> knowledge_base);
  // Costs pertaining to leaving tasks unscheduled
  ArcDescriptor TaskToUnscheduledAgg(TaskID_t task_id);
  ArcDescriptor UnscheduledAggToSink(JobID_t job_id);
  // Per-task costs (into the resource topology)
  ArcDescriptor TaskToResourceNode(TaskID_t task_id, ResourceID_t resource_id);
  // Costs within the resource topology
  ArcDescriptor ResourceNodeToResourceNode(
      const ResourceDescriptor& source, const ResourceDescriptor& destination);
  ArcDescriptor LeafResourceNodeToSink(ResourceID_t resource_id);
  // Costs pertaining to preemption (i.e. already running tasks)
  ArcDescriptor TaskContinuation(TaskID_t task_id);
  ArcDescriptor TaskPreemption(TaskID_t task_id);
  // Costs to equivalence class aggregators
  ArcDescriptor TaskToEquivClassAggregator(TaskID_t task_id, EquivClass_t tec);
  ArcDescriptor EquivClassToResourceNode(EquivClass_t tec, ResourceID_t res_id);
  ArcDescriptor EquivClassToEquivClass(EquivClass_t tec1, EquivClass_t tec2);
  // Calculate costs pertaining to pod priorities such node affinity, pod
  // affinity etc.
  void CalculatePrioritiesCost(const EquivClass_t ec,
                               const ResourceDescriptor& rd);
  // Get the type of equiv class.
  vector<EquivClass_t>* GetTaskEquivClasses(TaskID_t task_id);
  vector<ResourceID_t>* GetOutgoingEquivClassPrefArcs(EquivClass_t tec);
  vector<ResourceID_t>* GetTaskPreferenceArcs(TaskID_t task_id);
  vector<EquivClass_t>* GetEquivClassToEquivClassesArcs(EquivClass_t tec);
  void AddMachine(ResourceTopologyNodeDescriptor* rtnd_ptr);
  void AddTask(TaskID_t task_id);
  void RemoveMachine(ResourceID_t res_id);
  void RemoveTask(TaskID_t task_id);
  FlowGraphNode* GatherStats(FlowGraphNode* accumulator, FlowGraphNode* other);
  void PrepareStats(FlowGraphNode* accumulator);
  FlowGraphNode* UpdateStats(FlowGraphNode* accumulator, FlowGraphNode* other);

 private:
  // Fixed value for OMEGA, the normalization ceiling for each dimension's cost
  // value
  const Cost_t omega_ = 1000;
  // Largest cost seen so far, plus one
  Cost_t infinity_;
  FRIEND_TEST(CpuCostModelTest, AddMachine);
  FRIEND_TEST(CpuCostModelTest, AddTask);
  FRIEND_TEST(CpuCostModelTest, EquivClassToEquivClass);
  FRIEND_TEST(CpuCostModelTest, GetEquivClassToEquivClassesArcs);
  FRIEND_TEST(CpuCostModelTest, GatherStats);
  FRIEND_TEST(CpuCostModelTest, GetOutgoingEquivClassPrefArcs);
  FRIEND_TEST(CpuCostModelTest, GetTaskEquivClasses);
  FRIEND_TEST(CpuCostModelTest, MachineResIDForResource);
  Cost_t FlattenCostVector(CpuMemCostVector_t cv);
  EquivClass_t GetMachineEC(const string& machine_name, uint64_t ec_index);
  ResourceID_t MachineResIDForResource(ResourceID_t res_id);
  inline const TaskDescriptor& GetTask(TaskID_t task_id) {
    TaskDescriptor* td = FindPtrOrNull(*task_map_, task_id);
    CHECK_NOTNULL(td);
    return *td;
  }

  shared_ptr<ResourceMap_t> resource_map_;
  // The task map used in the rest of the system
  shared_ptr<TaskMap_t> task_map_;
  // A knowledge base instance that we will refer to for job runtime statistics.
  shared_ptr<KnowledgeBase> knowledge_base_;
  unordered_map<TaskID_t, float> task_cpu_cores_requirement_;
  unordered_map<TaskID_t, uint64_t> task_rx_bw_requirement_;
  unordered_map<TaskID_t, CpuMemResVector_t> task_resource_requirement_;
  unordered_map<EquivClass_t, float> ec_cpu_cores_requirement_;
  unordered_map<EquivClass_t, uint64_t> ec_rx_bw_requirement_;
  unordered_map<EquivClass_t, CpuMemResVector_t> ec_resource_requirement_;
  unordered_map<ResourceID_t, vector<EquivClass_t>, boost::hash<ResourceID_t>>
      ecs_for_machines_;
  unordered_map<EquivClass_t, ResourceID_t> ec_to_machine_;
  unordered_map<EquivClass_t, uint64_t> ec_to_index_;
  unordered_map<EquivClass_t, const RepeatedPtrField<LabelSelector>>
      ec_to_label_selectors;
  unordered_map<EquivClass_t, const TaskDescriptor> ec_to_td_requirements;
  unordered_map<EquivClass_t, unordered_map<ResourceID_t, PriorityScoresList_t,
                                            boost::hash<boost::uuids::uuid>>>
      ec_to_node_priority_scores;
  unordered_map<EquivClass_t, MinMaxScores_t> ec_to_max_min_priority_scores;
};

}  // namespace firmament

#endif  // FIRMAMENT_SCHEDULING_CPU_COST_MODEL_H
