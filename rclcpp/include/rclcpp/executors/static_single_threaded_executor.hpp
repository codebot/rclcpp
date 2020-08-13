// Copyright 2019 Nobleo Technology
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RCLCPP__EXECUTORS__STATIC_SINGLE_THREADED_EXECUTOR_HPP_
#define RCLCPP__EXECUTORS__STATIC_SINGLE_THREADED_EXECUTOR_HPP_

#include <cassert>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>

#include "rmw/rmw.h"

#include "rclcpp/executor.hpp"
#include "rclcpp/executors/static_executor_entities_collector.hpp"
#include "rclcpp/experimental/executable_list.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/memory_strategies.hpp"
#include "rclcpp/node.hpp"
#include "rclcpp/rate.hpp"
#include "rclcpp/utilities.hpp"
#include "rclcpp/visibility_control.hpp"

namespace rclcpp
{
namespace executors
{

/// Static executor implementation
/**
 * This executor is a static version of the original single threaded executor.
 * It's static because it doesn't reconstruct the executable list for every iteration.
 * All nodes, callbackgroups, timers, subscriptions etc. are created before
 * spin() is called, and modified only when an entity is added/removed to/from a node.
 *
 * To run this executor instead of SingleThreadedExecutor replace:
 * rclcpp::executors::SingleThreadedExecutor exec;
 * by
 * rclcpp::executors::StaticSingleThreadedExecutor exec;
 * in your source code and spin node(s) in the following way:
 * exec.add_node(node);
 * exec.spin();
 * exec.remove_node(node);
 */
class StaticSingleThreadedExecutor : public rclcpp::Executor
{
public:
  RCLCPP_SMART_PTR_DEFINITIONS(StaticSingleThreadedExecutor)

  /// Default constructor. See the default constructor for Executor.
  RCLCPP_PUBLIC
  explicit StaticSingleThreadedExecutor(
    const rclcpp::ExecutorOptions & options = rclcpp::ExecutorOptions());

  /// Default destrcutor.
  RCLCPP_PUBLIC
  virtual ~StaticSingleThreadedExecutor();

  /// Static executor implementation of spin.
  /**
   * This function will block until work comes in, execute it, and keep blocking.
   * It will only be interrupted by a CTRL-C (managed by the global signal handler).
   * \throws std::runtime_error when spin() called while already spinning
   */
  RCLCPP_PUBLIC
  void
  spin() override;

  /// Add a callback group to an executor.
  /**
   * If the callback group or node has been associated to an executor, throw
   * an exception. Otherwise, add to `entities_collector`
   * \param[in] group_ptr a shared ptr that points to a callback group
   * \param[in] node_ptr a shared pointer that points to a node base interface
   * \param[in] notify True to trigger the interrupt guard condition during this function. If
   * the executor is blocked at the rmw layer while waiting for work and it is notified that a new
   * callback group was added, it will wake up.
   */
  RCLCPP_PUBLIC
  void
  add_callback_group(
    rclcpp::CallbackGroup::SharedPtr group_ptr,
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr,
    bool notify = true) override;

  /// Remove callback group from the executor
  /**
   * Remove callback group from `entities_collector_` and unassociate it
   * with the executor.
   * \param[in] group_ptr a shared ptr that points to a callback group
   * \param[in] notify True to trigger the interrupt guard condition during this function. If
   * the executor is blocked at the rmw layer while waiting for work and it is notified that a new
   * callback group was added, it will wake up.
   */
  RCLCPP_PUBLIC
  void
  remove_callback_group(
    rclcpp::CallbackGroup::SharedPtr group_ptr,
    bool notify = true) override;

  /// Get callback groups that belong to executor.
  /**
   * This function returns a vector of weak pointers that point to callback groups that were
   * associated with the executor. The callback groups associated might have been added with
   * `add_callback_groups`, added when a node is added to the executor with `add_node`, or
   * automatically added when it was not associated to an executor and allows an executor
   * to automatically add it if the node that it belongs to is associated with the executor.
   *
   * \return a vector of weak pointers that point to callback groups that are associated with
   * the executor
   */
  RCLCPP_PUBLIC
  std::vector<rclcpp::CallbackGroup::WeakPtr>
  get_callback_groups() override;

  /// Add a node to the executor.
  /**
   * An executor can have zero or more nodes which provide work during `spin` functions.
   * \param[in] node_ptr Shared pointer to the node to be added.
   * \param[in] notify True to trigger the interrupt guard condition during this function. If
   * the executor is blocked at the rmw layer while waiting for work and it is notified that a new
   * node was added, it will wake up.
   * \throw std::runtime_error if node was already added or if rcl_trigger_guard_condition
   * return an error
   */
  RCLCPP_PUBLIC
  void
  add_node(
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr,
    bool notify = true) override;

  /// Convenience function which takes Node and forwards NodeBaseInterface.
  /**
   * \throw std::runtime_error if node was already added or if rcl_trigger_guard_condition
   * returns an error
   */
  RCLCPP_PUBLIC
  void
  add_node(std::shared_ptr<rclcpp::Node> node_ptr, bool notify = true) override;

  /// Remove a node from the executor.
  /**
   * \param[in] node_ptr Shared pointer to the node to remove.
   * \param[in] notify True to trigger the interrupt guard condition and wake up the executor.
   * This is useful if the last node was removed from the executor while the executor was blocked
   * waiting for work in another thread, because otherwise the executor would never be notified.
   * \throw std::runtime_error if rcl_trigger_guard_condition returns an error
   */
  RCLCPP_PUBLIC
  void
  remove_node(
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr,
    bool notify = true) override;

  /// Convenience function which takes Node and forwards NodeBaseInterface.
  /**
   * \throw std::runtime_error if rcl_trigger_guard_condition returns an error
   */
  RCLCPP_PUBLIC
  void
  remove_node(std::shared_ptr<rclcpp::Node> node_ptr, bool notify = true) override;

  /// Spin (blocking) until the future is complete, it times out waiting, or rclcpp is interrupted.
  /**
   * \param[in] future The future to wait on. If this function returns SUCCESS, the future can be
   *   accessed without blocking (though it may still throw an exception).
   * \param[in] timeout Optional timeout parameter, which gets passed to
   *    Executor::execute_ready_executables.
   *   `-1` is block forever, `0` is non-blocking.
   *   If the time spent inside the blocking loop exceeds this timeout, return a TIMEOUT return
   *   code.
   * \return The return code, one of `SUCCESS`, `INTERRUPTED`, or `TIMEOUT`.
   *
   *  Example usage:
   *  rclcpp::executors::StaticSingleThreadedExecutor exec;
   *  // ... other part of code like creating node
   *  // define future
   *  exec.add_node(node);
   *  exec.spin_until_future_complete(future);
   */
  template<typename FutureT, typename TimeRepT = int64_t, typename TimeT = std::milli>
  rclcpp::FutureReturnCode
  spin_until_future_complete(
    FutureT & future,
    std::chrono::duration<TimeRepT, TimeT> timeout = std::chrono::duration<TimeRepT, TimeT>(-1))
  {
    std::future_status status = future.wait_for(std::chrono::seconds(0));
    if (status == std::future_status::ready) {
      return rclcpp::FutureReturnCode::SUCCESS;
    }

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::nanoseconds timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      timeout);
    if (timeout_ns > std::chrono::nanoseconds::zero()) {
      end_time += timeout_ns;
    }
    std::chrono::nanoseconds timeout_left = timeout_ns;

    entities_collector_->init(&wait_set_, memory_strategy_, &interrupt_guard_condition_);

    while (rclcpp::ok(this->context_)) {
      // Do one set of work.
      entities_collector_->refresh_wait_set(timeout_left);
      execute_ready_executables();
      // Check if the future is set, return SUCCESS if it is.
      status = future.wait_for(std::chrono::seconds(0));
      if (status == std::future_status::ready) {
        return rclcpp::FutureReturnCode::SUCCESS;
      }
      // If the original timeout is < 0, then this is blocking, never TIMEOUT.
      if (timeout_ns < std::chrono::nanoseconds::zero()) {
        continue;
      }
      // Otherwise check if we still have time to wait, return TIMEOUT if not.
      auto now = std::chrono::steady_clock::now();
      if (now >= end_time) {
        return rclcpp::FutureReturnCode::TIMEOUT;
      }
      // Subtract the elapsed time from the original timeout.
      timeout_left = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - now);
    }

    // The future did not complete before ok() returned false, return INTERRUPTED.
    return rclcpp::FutureReturnCode::INTERRUPTED;
  }

  /// Not yet implemented, see https://github.com/ros2/rclcpp/issues/1219 for tracking
  RCLCPP_PUBLIC
  void
  spin_some(std::chrono::nanoseconds max_duration = std::chrono::nanoseconds(0)) override
  {
    (void)max_duration;
    throw rclcpp::exceptions::UnimplementedError(
            "spin_some is not implemented for StaticSingleThreadedExecutor, use spin or "
            "spin_until_future_complete");
  }

  /// Not yet implemented, see https://github.com/ros2/rclcpp/issues/1219 for tracking
  RCLCPP_PUBLIC
  void
  spin_all(std::chrono::nanoseconds) override
  {
    throw rclcpp::exceptions::UnimplementedError(
            "spin_all is not implemented for StaticSingleThreadedExecutor, use spin or "
            "spin_until_future_complete");
  }

  /// Not yet implemented, see https://github.com/ros2/rclcpp/issues/1219 for tracking
  RCLCPP_PUBLIC
  void
  spin_once(std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1)) override
  {
    (void)timeout;
    throw rclcpp::exceptions::UnimplementedError(
            "spin_once is not implemented for StaticSingleThreadedExecutor, use spin or "
            "spin_until_future_complete");
  }

protected:
  /// Check which executables in ExecutableList struct are ready from wait_set and execute them.
  /**
   * \param[in] exec_list Structure that can hold subscriptionbases, timerbases, etc
   * \param[in] timeout Optional timeout parameter.
   */
  RCLCPP_PUBLIC
  void
  execute_ready_executables();

private:
  RCLCPP_DISABLE_COPY(StaticSingleThreadedExecutor)

  StaticExecutorEntitiesCollector::SharedPtr entities_collector_;

  std::vector<rclcpp::CallbackGroup::WeakPtr> associated_callback_groups_;
};

}  // namespace executors
}  // namespace rclcpp

#endif  // RCLCPP__EXECUTORS__STATIC_SINGLE_THREADED_EXECUTOR_HPP_
