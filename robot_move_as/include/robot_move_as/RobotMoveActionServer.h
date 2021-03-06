//
// Created by shipei on 2/17/17.
// modified wsn 3/5/18
//

#ifndef ROBOT_MOVE_AS_ROBOT_MOVE_AS_H
#define ROBOT_MOVE_AS_ROBOT_MOVE_AS_H

#include <ros/ros.h>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <unordered_map>

#include <eigen3/Eigen/Eigen>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>

#include <actionlib/server/simple_action_server.h>

#include <sensor_msgs/JointState.h>
#include <sensor_msgs/LaserScan.h>
#include <trajectory_msgs/JointTrajectory.h>
#include <geometry_msgs/PoseStamped.h>
#include <robot_move_as/RobotInterface.h>
#include <robot_move_as/RobotMoveAction.h>
#include <trajectory_msgs/JointTrajectory.h>
#include <trajectory_msgs/JointTrajectoryPoint.h>
#include <control_msgs/FollowJointTrajectoryAction.h>
#include <tf/transform_listener.h>
#include <xform_utils/xform_utils.h>
#include <ariac_ur_fk_ik/ur_kin.h>
#include <osrf_gear/VacuumGripperControl.h>
#include <osrf_gear/VacuumGripperState.h>
//#include <inventory_msgs/Inventory.h>
#include <inventory_msgs/Part.h>



#include <robot_move_as/RobotInterface.h>

using namespace std;
using namespace Eigen;
using namespace inventory_msgs;
//using namespace cwru_ariac;

//part frame notes:
// GASKET: frame is centered, but with origin at BOTTOM surface; x-axis points towards pin feature
// PISTON ROD: frame origin is at BOTTOM surface of part, centered in hole; y-axis points towards opposite end of rod
// GEAR: origin is at BOTTOM of part (centered), and Y-AXIS points towards pin feature
//DISK: origin is at BOTTOM of part (centered), pin feature is 45 deg (between x-axis and y_axis)
const double PISTON_ROD_PART_THICKNESS= 0.007; // wsn modified for qual3; 0.0075; //works for qual2
const double GEAR_PART_THICKNESS = 0.0124; // modified for qual3; 0.015; // 0.015 works for qual2
const double DISK_PART_THICKNESS = 0.0247;  //note sure about this one...maybe OK
const double GASKET_PART_THICKNESS = 0.02; // 0.0336; //wsn change for gear 1.1
//per  /opt/ros/indigo/share/osrf_gear/models/pulley_part_ariac, pulley-part collision model should be 0.0720 thick
const double PULLEY_PART_THICKNESS = 0.0728;  //0.7255 = z on bin; 0.7983 on top of another pulley: 0.0728 thickness; origin on bottom

//here are some hand-tuned "kludge" parameters to tweak grasp transforms;
const double GEAR_PART_GRASP_Y_OFFSET = 0.04;
const double DISK_PART_GRASP_Z_OFFSET = 0.005;
const double GASKET_PART_GRASP_Z_OFFSET = 0.0; //0.006;
const double GASKET_PART_GRASP_X_OFFSET = 0.03;
const double PULLEY_PART_GRASP_Z_OFFSET = 0.007; //0.01; //0.005;



 
const bool UP = true;
const bool DOWN = false;

//surface heights:
// had to increase tray height by 0.010 to get drop-off height of tray correct.  Don't know why
//const double TRAY1_HEIGHT = 0.755+0.010; //pad tray height as manual fix...gravity droop problem?
const double BIN_HEIGHT = 0.725;
//const double CONVEYOR_HEIGHT = 0.907;

const double BASE_LINK_HEIGHT = 0.8;
const double BASE_LINK_X_COORD = 0.3; //    Oe[0] = BASE_LINK_X_COORD; //0.3;

const double BOX_HEIGHT = 0.460; //0.4446;  //try elevating box height to provide dropoff clearance

//const double QUAL2_CONVEYOR_SPEED = -0.2;

//const double CONVEYOR_TRACK_FUDGE_TIME = 0.0; //0.5;
//const double CONVEYOR_FETCH_QRAIL_MIN = -1.0; // don't go more negative than this


class RobotMoveActionServer {
private:
    ros::NodeHandle nh;
    RobotInterface robotInterface;
    actionlib::SimpleActionServer<robot_move_as::RobotMoveAction> as;
    robot_move_as::RobotMoveGoal goal_;
    robot_move_as::RobotMoveFeedback feedback_;
    robot_move_as::RobotMoveResult result_;
    bool isPreempt;
    bool goalComplete;
    //RobotState robotState;
    unordered_map<int8_t, string> placeFinder;
    ros::Publisher joint_trajectory_publisher_;
    control_msgs::FollowJointTrajectoryGoal traj_goal_;
    trajectory_msgs::JointTrajectory traj_;
    trajectory_msgs::JointTrajectory jspace_pose_to_traj(Eigen::VectorXd joints, double dtime=2.0);
    
    
    //here are the action functions: robot moves
    void move_to_jspace_pose(Eigen::VectorXd q_vec, double dtime=2.0); //case robot_move_as::RobotMoveGoal::TO_PREDEFINED_POSE:
    unsigned short int flip_part_fnc(const robot_move_as::RobotMoveGoalConstPtr& goal); 
    unsigned short int grasp_fnc(double timeout=2.0);  //default timeout; rtns error code
    unsigned short int release_fnc(double timeout=2.0); //default timeout for release
    unsigned short int pick_part_fnc(const robot_move_as::RobotMoveGoalConstPtr& goal); //rtns err code; used within other fncs
    unsigned short int place_part_fnc_no_release(inventory_msgs::Part part);
    unsigned short int move_part(const robot_move_as::RobotMoveGoalConstPtr &goal,double timeout=0);   
    unsigned short int is_pickable(const robot_move_as::RobotMoveGoalConstPtr &goal);
    unsigned short int is_placeable(inventory_msgs::Part part);
    
    unsigned short int current_pose_code_;

    
    Eigen::VectorXd q_des_7dof_,q_cruise_pose_,bin_cruise_jspace_pose_,bin_hover_jspace_pose_;
    //Eigen::VectorXd agv_hover_pose_,agv_cruise_pose_;
    Eigen::VectorXd source_hover_pose_,source_cruise_pose_; 
    Eigen::VectorXd destination_hover_pose_,destination_cruise_pose_;
    double source_track_displacement_,destination_track_displacement_;
    
    Eigen::VectorXd q_Q1_rvrs_discard_,q_Q1_rvrs_hover_,q_Q1_rvrs_hover_flip_,q_Q1_rvrs_cruise_;
    Eigen::VectorXd q_Q1_fwd_discard_,q_Q1_fwd_hover_,q_Q1_fwd_hover_flip_,q_Q1_fwd_cruise_;
    Eigen::VectorXd q_Q1_arm_vertical_;
    Eigen::VectorXd q_Q1_dropoff_near_left_,q_Q1_dropoff_far_left_,q_Q1_dropoff_near_right_,q_Q1_dropoff_far_right_;
    
    //get rid of some of  these!
    
    
    Eigen::VectorXd box_hover_pose_,box_cruise_pose_;   
    
    Eigen::VectorXd q_box_Q2_hover_pose_,q_box_Q2_cruise_pose_;   
    Eigen::VectorXd q_Q1_discard_pose_,q_Q2_discard_pose_;
    Eigen::VectorXd q_Q1_cruise_pose_,q_Q1_hover_pose_;
    Eigen::VectorXd q_bin_cruise_pose_,q_destination_cruise_pose_;
    Eigen::VectorXd q_manip_nom_;
    
    Eigen::VectorXd q_init_pose_,q_hover_pose_;
    Eigen::VectorXd pickup_jspace_pose_,dropoff_jspace_pose_;
    Eigen::VectorXd approach_pickup_jspace_pose_,approach_dropoff_jspace_pose_;
    
    
    //Eigen::VectorXd q_agv1_hover_pose_,q_agv1_cruise_pose_;  
    //Eigen::VectorXd q_agv2_hover_pose_,q_agv2_cruise_pose_;    
    Eigen::VectorXd q_conveyor_hover_pose_,q_conveyor_cruise_pose_;    
    Eigen::VectorXd q_bin8_cruise_pose_,q_bin8_hover_pose_,q_bin8_retract_pose_;    
    Eigen::VectorXd q_bin7_cruise_pose_,q_bin7_hover_pose_,q_bin7_retract_pose_;  
    Eigen::VectorXd q_bin6_cruise_pose_,q_bin6_hover_pose_,q_bin6_retract_pose_;  
    Eigen::VectorXd q_bin5_cruise_pose_,q_bin5_hover_pose_,q_bin5_retract_pose_;  
    Eigen::VectorXd q_bin4_cruise_pose_,q_bin4_hover_pose_,q_bin4_retract_pose_;  
    Eigen::VectorXd q_bin3_cruise_pose_,q_bin3_hover_pose_,q_bin3_retract_pose_;  
    Eigen::VectorXd q_bin2_cruise_pose_,q_bin2_hover_pose_,q_bin2_retract_pose_;  
    Eigen::VectorXd q_bin1_cruise_pose_,q_bin1_hover_pose_,q_bin1_retract_pose_;
    Eigen::VectorXd q_bin_pulley_flip_;
    Eigen::Affine3d grasp_transform_;
    Eigen::VectorXd j1;
    
    Eigen::Affine3d affine_vacuum_pickup_pose_wrt_base_link_;
    Eigen::Affine3d affine_vacuum_dropoff_pose_wrt_base_link_;
    //double rail_stops[];
    //unordered_map<int8_t, int> placeIndex;
    void set_key_poses();
    //fncs to get key joint-space poses:
    //each bin gets a corresponding rail pose; return "true" if valid bin code
    bool rail_prepose(int8_t location, double &q_rail);
    //each bin has a corresponding "hover" pose; set q_vec and return true if valid bin code
    //bool bin_hover_jspace_pose(int8_t bin, Eigen::VectorXd &q_vec);
    //cruise pose depends on bin code and whether to point towards agv1 or agv2
    // provide bin code and agv code; get back q_vec to prepare for cruise to agv
    //bool bin_cruise_jspace_pose(int8_t bin, int8_t agv, Eigen::VectorXd &q_vec);
    //bool bin_cruise_jspace_pose(int8_t location, Eigen::VectorXd &q_vec);
    

    //bool box_cruise_jspace_pose(int8_t box, Eigen::VectorXd &q_vec); 
    bool hover_jspace_pose_w_code(int8_t bin, unsigned short int box_placement_location_code, Eigen::VectorXd &q_vec);
    bool hover_jspace_pose(int8_t bin, Eigen::VectorXd &q_vec); //{ hover_jspace_pose(bin,(unsigned short int) 0,&q_vec)};  //default, no box location  code    
    //bool RobotMoveActionServer::hover_jspace_pose(int8_t bin, int8_t box_placement_location_code, Eigen::VectorXd &qvec)
    bool cruise_jspace_pose_w_code(int8_t bin, unsigned short int box_placement_location_code, Eigen::VectorXd &q_vec);
    bool cruise_jspace_pose(int8_t bin,  Eigen::VectorXd &q_vec); // { return cruise_jspace_pose(bin,robot_move_as::RobotMoveGoal::Q1_DROPOFF_UNKNOWN,&q_vec);}; //default, no box location code
    bool set_q_manip_nom_from_destination_part(Part part);
    //trivial func to compute affine3 for robot_base w/rt world;  only depends on rail displacement
    Eigen::Affine3d  affine_base_link(double q_rail);

    double get_pickup_offset(Part part); //fnc to return offset values for gripper: part top relative to part frame
    double get_dropoff_offset(Part part);
    double get_surface_height(Part part);
    
    //"Part" should include part pose w/rt world, so can determine if part is right-side up or up-side down
    bool get_grasp_transform(Part part,Eigen::Affine3d &grasp_transform);



    bool eval_up_down(geometry_msgs::PoseStamped part_pose_wrt_world);
    //given rail displacement, and given Part description (including name and pose info) compute where the gripper should be, as
    //an Affine3 w/rt base_link frame
    Eigen::Affine3d affine_vacuum_pickup_pose_wrt_base_link(Part part, double q_rail);
    //similarly, compute gripper pose for dropoff, accounting for part height
    Eigen::Affine3d affine_vacuum_dropoff_pose_wrt_base_link(Part part, double q_rail);
    //do IK to place gripper at specified affine3; choose solution that is closest to provided jspace pose
    bool get_pickup_IK(Eigen::Affine3d affine_vacuum_gripper_pose_wrt_base_link,Eigen::VectorXd approx_jspace_pose,Eigen::VectorXd &q_vec_soln);
    //similarly for drop-off solution
    //bool get_dropoff_IK(Eigen::Affine3d affine_vacuum_gripper_pose_wrt_base_link,Eigen::VectorXd approx_jspace_pose,Eigen::VectorXd &q_vec_soln);
    //compute q_vec_soln corresponding to approach to specified grasp pose; specify approach distance; choose IK soln that is closest
    //to grasp IK soln
    //2nd flavor, includes bool for wrist-near/wrist-far soln choices 
    bool get_pickup_IK(Eigen::Affine3d affine_vacuum_gripper_pose_wrt_base_link,
                                          unsigned short int box_placement_location_code, Eigen::VectorXd &q_vec_soln);

    bool compute_approach_IK(Eigen::Affine3d affine_vacuum_gripper_pose_wrt_base_link,Eigen::VectorXd approx_jspace_pose,double approach_dist,Eigen::VectorXd &q_vec_soln);


    void grab();
    void release();  
    //RobotState calcRobotState();
    osrf_gear::VacuumGripperState getGripperState();
    bool attached_;
    bool isGripperAttached();
    bool waitForGripperAttach(double timeout);
  
    ros::ServiceClient gripper_client;
    osrf_gear::VacuumGripperState currentGripperState_;
    osrf_gear::VacuumGripperControl attach_;
    osrf_gear::VacuumGripperControl detach_;
    
    tf::TransformListener* tfListener_;
    tf::StampedTransform tfCameraWrtWorld_,tfTray1WrtWorld_,tfTray2WrtWorld_;
    XformUtils xformUtils_;
    UR10FwdSolver fwd_solver_;
    UR10IkSolver ik_solver_;
    Eigen::Affine3d agv1_tray_frame_wrt_world_,agv2_tray_frame_wrt_world_;
    double approach_dist_;
    inventory_msgs::Part part_of_interest_;
public:
    RobotMoveActionServer(ros::NodeHandle nodeHandle, string topic);
    void executeCB(const robot_move_as::RobotMoveGoalConstPtr &goal);
    void preemptCB();
    bool get_pose_from_code(unsigned short int POSE_CODE, Eigen::VectorXd &q_vec);

};


#endif //ROBOT_MOVE_AS_ROBOT_MOVE_AS_H
