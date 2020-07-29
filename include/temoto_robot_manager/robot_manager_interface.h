/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright 2019 TeMoto Telerobotics
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Author: Veiko Vunder */
/* Author: Fabian Parra */

#ifndef TEMOTO_ROBOT_MANAGER__ROBOT_MANAGER_INTERFACE_H
#define TEMOTO_ROBOT_MANAGER__ROBOT_MANAGER_INTERFACE_H

#include "temoto_core/trr/resource_registrar.h"
#include "temoto_core/common/temoto_id.h"
#include "temoto_core/common/console_colors.h"
#include "temoto_robot_manager/robot_manager_services.h"
#include "yaml-cpp/yaml.h"
#include <vector>
#include <string>

namespace robot_manager
{

template <class ParentSubsystem>
class RobotManagerInterface : public temoto_core::BaseSubsystem
{
public:
  RobotManagerInterface()
  {
    class_name_ = __func__;
  }

  void initialize(ParentSubsystem* parent_subsystem)
  {
    initializeBase(parent_subsystem);
    log_group_ = "interfaces." + parent_subsystem->class_name_;
    subsystem_name_ = parent_subsystem->class_name_ + "/robot_manager_interface";

    // create resource manager
    resource_registrar_ = std::unique_ptr<temoto_core::trr::ResourceRegistrar<RobotManagerInterface>>(
        new temoto_core::trr::ResourceRegistrar<RobotManagerInterface>(subsystem_name_, this));

    // ensure that resource_registrar was created
    validateInterface();

    // register status callback function
    resource_registrar_->registerStatusCb(&RobotManagerInterface::statusInfoCb);

    client_plan_ =
      nh_.serviceClient<temoto_robot_manager::RobotPlanManipulation>(robot_manager::srv_name::SERVER_PLAN);
    client_exec_ =
      nh_.serviceClient<temoto_robot_manager::RobotExecutePlan>(robot_manager::srv_name::SERVER_EXECUTE);
    client_viz_info_ =
      nh_.serviceClient<temoto_robot_manager::RobotGetVizInfo>(robot_manager::srv_name::SERVER_GET_VIZ_INFO);
    client_set_manipulation_target_ =
      nh_.serviceClient<temoto_robot_manager::RobotSetTarget>(robot_manager::srv_name::SERVER_SET_MANIPULATION_TARGET);
    client_get_manipulation_target_ =
      nh_.serviceClient<temoto_robot_manager::RobotGetTarget>(robot_manager::srv_name::SERVER_GET_MANIPULATION_TARGET);
    client_navigation_goal_ =
      nh_.serviceClient<temoto_robot_manager::RobotNavigationGoal>(robot_manager::srv_name::SERVER_NAVIGATION_GOAL);
    client_gripper_control_position_ =
      nh_.serviceClient<temoto_robot_manager::RobotGripperControlPosition>(robot_manager::srv_name::SERVER_GRIPPER_CONTROL_POSITION);
    client_get_robot_config_ =
      nh_.serviceClient<temoto_robot_manager::RobotGetConfig>(robot_manager::srv_name::SERVER_GET_CONFIG);
  }

  YAML::Node getRobotConfig(const std::string& robot_name)
  {
    validateInterface();
    temoto_robot_manager::RobotGetConfig msg;
    msg.request.robot_name = robot_name;
    if (!client_get_robot_config_.call(msg))
    {
      throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, "Service call returned false.");
    }
    else if (msg.response.code == temoto_core::trr::status_codes::FAILED)
    {
      throw FORWARD_ERROR(msg.response.error_stack);
    }
    else
    {
      try
      {
        YAML::Node robot_config = YAML::Load(msg.response.robot_config);
        robot_config["robot_absolute_namespace"] = msg.response.robot_absolute_namespace;
        return robot_config;
      }
      catch(const std::exception& e)
      {
        throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, e.what());
      }
    }
  }

  void loadRobot(std::string robot_name = "")
  {
    validateInterface();
    // Contact the "Context Manager", pass the gesture specifier and if successful, get
    // the name of the topic
    temoto_robot_manager::RobotLoad load_srvc;
    load_srvc.request.robot_name = robot_name;
    try
    {
      resource_registrar_->template call<temoto_robot_manager::RobotLoad>(
          robot_manager::srv_name::MANAGER, robot_manager::srv_name::SERVER_LOAD, load_srvc);
    }
    catch(temoto_core::error::ErrorStack& error_stack)
    {
      throw FORWARD_ERROR(error_stack);
    }
  }

  void planManipulation(const std::string& robot_name, std::string planning_group = "")
  {
    temoto_robot_manager::RobotPlanManipulation msg;
    msg.request.use_default_target = true;
    msg.request.use_named_target = false;
    msg.request.planning_group = planning_group;
    msg.request.robot_name = robot_name;
    if (!client_plan_.call(msg))
    {
      throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, "Service call returned false.");
    }
    else if (msg.response.code == temoto_core::trr::status_codes::FAILED)
    {
      throw FORWARD_ERROR(msg.response.error_stack);
    }
  }

  void planManipulation(const std::string& robot_name,
            const std::string& planning_group,
            const geometry_msgs::PoseStamped& pose)
  {
    temoto_robot_manager::RobotPlanManipulation msg;
    msg.request.use_default_target = false;
    msg.request.use_named_target = false;
    msg.request.target_pose = pose;
    msg.request.planning_group = planning_group;
    msg.request.robot_name = robot_name;
    
    if (!client_plan_.call(msg))
    {
      throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, "Service call returned false.");
    }
    else if (msg.response.code == temoto_core::trr::status_codes::FAILED)
    {
      throw FORWARD_ERROR(msg.response.error_stack);
    }
  }

  void planManipulation(const std::string& robot_name,const std::string& planning_group,const std::string& named_target_pose)
  {
    temoto_robot_manager::RobotPlanManipulation msg;
    msg.request.use_default_target = false;
    msg.request.use_named_target = true;
    msg.request.named_target = named_target_pose;
    msg.request.planning_group = planning_group;
    msg.request.robot_name = robot_name;
    
    if (!client_plan_.call(msg))
    {
      throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, "Service call returned false.");
    }
    else if (msg.response.code == temoto_core::trr::status_codes::FAILED)
    {
      throw FORWARD_ERROR(msg.response.error_stack);
    }
  }

  void executePlan(const std::string& robot_name)
  {
    temoto_robot_manager::RobotExecutePlan msg;
    msg.request.robot_name = robot_name;
    if (!client_exec_.call(msg))
    {
      throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, "Service call returned false.");
    }
    else if (msg.response.code == temoto_core::trr::status_codes::FAILED)
    {
      throw FORWARD_ERROR(msg.response.error_stack);
    }
  }

  std::string getMoveitRvizConfig()
  {
    temoto_robot_manager::RobotGetVizInfo msg;
    if (!client_viz_info_.call(msg))
    {
      throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, "Service call returned false.");
    }
    else if (msg.response.code == temoto_core::trr::status_codes::FAILED)
    {
      throw FORWARD_ERROR(msg.response.error_stack);
    }
    return msg.response.info;
  }

  void setTarget(std::string object_name)
  {
    temoto_robot_manager::RobotSetTarget msg;
    msg.request.object_name = object_name;
    if (!client_set_manipulation_target_.call(msg))
    {
      throw CREATE_ERROR(temoto_core::error::Code::SERVICE_REQ_FAIL, "Service call returned false.");
    }
    else if (msg.response.code == temoto_core::trr::status_codes::FAILED)
    {
      throw FORWARD_ERROR(msg.response.error_stack);
    }
  }

 geometry_msgs::Pose getEndEffPose(const std::string& robot_name)
 {
    geometry_msgs::Pose pose;
    temoto_robot_manager::RobotGetTarget msg; 
    msg.request.robot_name = robot_name;
    client_get_manipulation_target_.call(msg);
    //TEMOTO_INFO_STREAM(msg.response.pose);
    pose = msg.response.pose;
    
    return pose;
  }

  void navigationGoal(const std::string& robot_name, const std::string& reference_frame, const geometry_msgs::PoseStamped& pose)
  {
    temoto_robot_manager::RobotNavigationGoal msg; 
    msg.request.reference_frame = reference_frame;
    msg.request.target_pose = pose;
    msg.request.robot_name = robot_name;
    if (client_navigation_goal_.call(msg))
    {
      TEMOTO_DEBUG("The goal was set successfully");
    }
    else
    {
      TEMOTO_ERROR("Failed to reach the server"); 
    }  
  }

  void controlGripperPosition(const std::string& robot_name,const float& position)
  {
    temoto_robot_manager::RobotGripperControlPosition msg;    
    msg.request.robot_name = robot_name;
    msg.request.control = position;

    if (client_gripper_control_position_.call(msg))
    {
    TEMOTO_DEBUG("The gripper move");
    }
    else
    {
    TEMOTO_ERROR("Failed to reach the server for gripper control"); 
    }
  }
  
  /**
   * @brief validateInterface()
   * @param sensor_type
   */
  void validateInterface()
  {
    if (!resource_registrar_)
    {
      throw CREATE_ERROR(temoto_core::error::Code::UNINITIALIZED, "Interface is not initalized.");
    }
  }

  const std::string& getName() const
  {
    return subsystem_name_;
  }

  void statusInfoCb(temoto_core::ResourceStatus& srv)
  {
    try
    {
      validateInterface();

      TEMOTO_DEBUG_STREAM("status info was received");
      TEMOTO_DEBUG_STREAM(srv.request);

      auto robot_it = std::find_if(
        allocated_robots_.begin(),
        allocated_robots_.end(),
        [&](const temoto_robot_manager::RobotLoad& robot) -> bool {
          return robot.response.trr.resource_id == srv.request.resource_id;
        });

      if (robot_it != allocated_robots_.end())
      {
        if (srv.request.status_code == temoto_core::trr::status_codes::FAILED)
        {
          TEMOTO_WARN("The status info reported a resource failure.");

          TEMOTO_WARN_STREAM("Sending a request to unload the failed robot ...");
          resource_registrar_->unloadClientResource(robot_it->response.trr.resource_id);

          TEMOTO_DEBUG("Asking the same component again");

          // this call automatically updates the response in allocated robots vec
          resource_registrar_->template call<temoto_robot_manager::RobotLoad>(robot_manager::srv_name::MANAGER
          , robot_manager::srv_name::SERVER_LOAD
          , *robot_it);

          return;
        }
      }
    }
    catch (temoto_core::error::ErrorStack& error_stack)
    {
      throw FORWARD_ERROR(error_stack);
    }
  }

  ~RobotManagerInterface()
  {
    // Shutdown robot manager clients.
    client_load_.shutdown();
    client_plan_.shutdown();
    client_exec_.shutdown();
    client_viz_info_.shutdown();
    client_set_manipulation_target_.shutdown();
    client_get_manipulation_target_.shutdown();
    client_navigation_goal_.shutdown();
    client_gripper_control_position_.shutdown();

    TEMOTO_DEBUG("RobotManagerInterface destroyed.");
  }

private:

  std::vector<temoto_robot_manager::RobotLoad> allocated_robots_;

  ros::NodeHandle nh_;
  ros::ServiceClient client_load_;
  ros::ServiceClient client_plan_;
  ros::ServiceClient client_exec_;
  ros::ServiceClient client_viz_info_;
  ros::ServiceClient client_set_manipulation_target_;  
  ros::ServiceClient client_get_manipulation_target_;
  ros::ServiceClient client_navigation_goal_;
  ros::ServiceClient client_gripper_control_position_;
  ros::ServiceClient client_get_robot_config_; 

  std::unique_ptr<temoto_core::trr::ResourceRegistrar<RobotManagerInterface>> resource_registrar_;
};

} // namespace
#endif

