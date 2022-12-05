#include "ros/ros.h"
#include "std_msgs/String.h"
#include "virtuose/out_virtuose_status.h"
#include "virtuose/out_virtuose_pose.h"
#include "virtuose/out_virtuose_physical_pose.h"

#include "virtuose/in_virtuose_force.h"
#include "virtuose/virtuose_impedance.h"
#include "virtuose/virtuose_reset.h"
#include "geometry_msgs/PoseStamped.h"
#include <geometry_msgs/WrenchStamped.h>
#include <iostream>
#include <fstream>


// Storage for virtuose pose and ur5 position
float cur_pose_H[7];//pose ->virtuose
float curr_wrench[3];
float old_pose[7];
float initial_pose_H[7];
float delta_pos_H[7];
float delta_vel_H[7];
uint64_t status_date;


int old_H_flag=0;
uint64_t pose_date;
int dt=0.01;
//
//Sping Only parameters: force limit=10; KK2=50
float error_x=0, error_y =0, error_z= 0;
float error_sensitivity=0.001;//0.01;
float opto_sensitivity=50;
float force_limit=10;
float damping_coeff=0;//100;
float damping_force[3];
float fixed_spring_x,fixed_spring_y,fixed_spring_z;
float spring_force[3];
float fixed_spring_x_force,fixed_spring_y_force,fixed_spring_z_force;
float curr_spring_x_force,curr_spring_y_force, curr_spring_z_force;
float test_error;
//
float virutal_elong__H[3];
float KK=0;
float KK_fix=0;//100;// for VIRUTOSE ALONE SPRING TEST set 100;
float KK_curr=0;//1000;
// Storage for virtuose_node status
int status_state = 0;
int status_button;
float test_x,test_y,test_z;  


ros::Publisher* _out_virtuose_delta_pose;
 

// Storage for virtuose pose
float cur_pose[7], init_pose[7], delta_pose[7];
bool init_pose_record=false;

// ??tf2::Quaternion delta_quat, init_quat, cur_quat;





// Callback for topic out_virtuose_pose
void out_virtuose_poseCB(const virtuose::out_virtuose_pose::ConstPtr& msg)
{
// Store last pose date
  pose_date = msg->header.stamp.toNSec();
  cur_pose_H[0] = msg->virtuose_pose.translation.x; //virtuose_pose  cambia solo se si impugna con decisione il manettino
  cur_pose_H[1] = msg->virtuose_pose.translation.y;
  cur_pose_H[2] = msg->virtuose_pose.translation.z;
  cur_pose_H[3] = msg->virtuose_pose.rotation.x;
  cur_pose_H[4] = msg->virtuose_pose.rotation.y;
  cur_pose_H[5] = msg->virtuose_pose.rotation.z;
  cur_pose_H[6] = msg->virtuose_pose.rotation.w;

if(cur_pose_H[0]!=0.0 && cur_pose_H[1]!=0.0 && cur_pose_H[2]!=0.0)
  {
    if (old_H_flag==0) 
  {
    for(int i=0;i<7;i++)
    {
      initial_pose_H[i]=cur_pose_H[i];
      old_pose[i]=initial_pose_H[i];
    }
    old_H_flag=1;
  } 
  }
}

void __OnOPTO0Contact(const geometry_msgs::WrenchStamped::ConstPtr& msg)
{

  //std::cout << " OPTOFORCE 0 "<< "x" << msg->wrench.force.x <<"y" << msg->wrench.force.y <<"z" << msg->wrench.force.z <<"\n";
  curr_wrench[0]= msg->wrench.force.x;
  curr_wrench[1]= msg->wrench.force.y;
  curr_wrench[2]= msg->wrench.force.z;
}

// Callback for topic out_virtuose_status
void out_virtuose_statusCB(const virtuose::out_virtuose_status::ConstPtr& msg)
{
  // Store last status date
  status_date = msg->header.stamp.toNSec();
  status_state = msg->state;
  status_button = msg->buttons;
}

// Main function
int main(int argc, char **argv)
{
  printf("Starting test_thumb_impedance node\n");
  // Init ROS library
  ros::init(argc, argv, "thumb_impedance");
  // Create node handle
  ros::NodeHandle n_th;

  // Connect to virtuose_node
  printf("Connecting to virtuose node\n");
  ros::Subscriber out_virtuose_status = n_th.subscribe<virtuose::out_virtuose_status>("out_thumb_status", 1, out_virtuose_statusCB);
  ros::Subscriber out_virtuose_pose = n_th.subscribe<virtuose::out_virtuose_pose>("out_thumb_pose", 1, out_virtuose_poseCB);
  ros::Publisher in_virtuose_force = n_th.advertise<virtuose::in_virtuose_force>("in_thumb_force", 1);

  // ros::Publisher out_virtuose_delta_pose = n.advertise<virtuose::out_virtuose_pose>("out_virtuose_delta_pose", 1);
  // _out_virtuose_delta_pose = &out_virtuose_delta_pose;

  ros::ServiceClient virtuose_impedance = n_th.serviceClient<virtuose::virtuose_impedance>("thumb_impedance");
  //ros::ServiceClient virtuose_impedance = n.serviceClient<virtuose::virtuose_impedance>("index_impedance");
  //ros::ServiceClient virtuose_impedance = n.serviceClient<virtuose::virtuose_impedance>("middle_impedance");

  ros::ServiceClient virtuose_reset = n_th.serviceClient<virtuose::virtuose_reset>("virtuose_reset");


  //OPTOFORCE SUBSCRIBERS
  ros::Subscriber optoForce0_feedback = n_th.subscribe("/optoforce_wrench_0", 5, __OnOPTO0Contact);

  // Reset virtuose_node
  virtuose::virtuose_reset res;
  // virtuose_reset.call(res);
  
  // Create multithread spinner
  ros::AsyncSpinner spinner(2);
  spinner.start();

  uint64_t start = ros::Time::now().toNSec();

  // Request impedance mode
  printf("Sending impedance request\n");
  virtuose::virtuose_impedance imp;
  imp.request.ip_address = "localhost#53211"; //11 thumb SvcHaptic_HGlove_finger_thumb_n5.conf
  imp.request.indexing_mode = 0;
  imp.request.speed_factor = 1.0;
  imp.request.force_factor = 1.0;
  imp.request.power_enable = 1;
  imp.request.max_force = 10.0;
  imp.request.max_torque = 1.0;
  imp.request.base_frame.translation.x = 0.0;
  imp.request.base_frame.translation.y = 0.0;
  imp.request.base_frame.translation.z = 0.0;
  imp.request.base_frame.rotation.x = 0.0;
  imp.request.base_frame.rotation.y = 0.0;
  imp.request.base_frame.rotation.z = 0.0;
  imp.request.base_frame.rotation.w = 1.0;
  if (!virtuose_impedance.call(imp))
  {
    printf("Call failed\n");
    return 0;
  }
  // Store client ID given by virtuose_node
  uint32_t client_id = 0;
  client_id = imp.response.client_id;
  printf("Our client ID is %d\n", client_id);

/*
IMPEDANCE CONTROLLER

*/


// Perform impedance loop at 500 Hz
  ros::Rate r(500);
  int ctr = 0;
  while(ros::ok())
  {
// Create null force
    virtuose::in_virtuose_force force;
    force.header.stamp = ros::Time::now();
    force.client_id = client_id;
    force.virtuose_force.force.x = 0.0;
    force.virtuose_force.force.y = 0.0;
    force.virtuose_force.force.z = 0.0;
    force.virtuose_force.torque.x = 0.0;
    force.virtuose_force.torque.y = 0.0;
    force.virtuose_force.torque.z = 0.0;

    
//DELTA TRANSLATION NO ROTATION
    for(int j=0;j<3;j++)
    {
      delta_pos_H[j]=cur_pose_H[j] - old_pose[j];
      delta_vel_H[j]=delta_pos_H[j]/dt;
      old_pose[j]=cur_pose_H[j];
    }
    

    // myfile << "cur_pose_H"<< "," <<cur_pose_H[0]<< "," << cur_pose_H[1]<< "," <<cur_pose_H[2]<< ","  ;
    // myfile << "old_pose_H"<< "," <<old_pose[0]<< "," << old_pose[1]<< "," <<old_pose[2]<< ","  ;
    // myfile << "delta_pos H"<< "," <<delta_pos_H[0]<< "," << delta_pos_H[1]<< "," <<delta_pos_H[2]<< ",";


//VIRTUAL ELONG FIXED FRAME VIRTUOSE
// printf("initial pose: x %f - y %f - z %f \n",initial_pose_H[0],initial_pose_H[1],initial_pose_H[2]);
// printf("cur_pose_H : x %f - y %f - z %f \n\n",cur_pose_H[0],cur_pose_H[1],cur_pose_H[2]);

virutal_elong__H[0]=-(initial_pose_H[0]-cur_pose_H[0]);
virutal_elong__H[1]=-(initial_pose_H[1]-cur_pose_H[1]);
virutal_elong__H[2]=-(initial_pose_H[2]-cur_pose_H[2]);


    force_limit=5;
    KK_fix=50;// 100 STRONG FORCE, 10 LIGHT FORCE 
    KK_curr=0;//150;

fixed_spring_x_force=KK_fix*(virutal_elong__H[0]);
fixed_spring_y_force=KK_fix*(virutal_elong__H[1]);
fixed_spring_z_force=KK_fix*(virutal_elong__H[2]);

spring_force[0]=-(fixed_spring_x_force);
spring_force[1]=-(fixed_spring_y_force);
spring_force[2]=-(fixed_spring_z_force);
//printf("spring_force: x %f - y %f - z %f \n",spring_force[0],spring_force[1],spring_force[2]);

/*

BELOW FORCE SETTINGS AND SAFETY CHECKS

*/

//GENERATE X FORCE
if (abs(curr_wrench[0])>opto_sensitivity||abs(curr_wrench[1])>opto_sensitivity||abs(curr_wrench[2])>opto_sensitivity)
{
  //printf("A\n");
  force.virtuose_force.force.x = spring_force[0];
  if (force.virtuose_force.force.x > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.x = force_limit;
    spring_force[0]=force_limit;
    //printf("WARNING: FORCE TRESHOLD X REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[0], force_limit);
  }
  else if (force.virtuose_force.force.x < -force_limit)
  {
    force.virtuose_force.force.x = -force_limit;
    spring_force[0]=-force_limit;
    //printf("WARNING: FORCE TRESHOLD X REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[0], force_limit);
  }     

//GENERATE Y FORCE

  force.virtuose_force.force.y = spring_force[1];
  if (force.virtuose_force.force.y > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.y = force_limit;
    spring_force[1]=force_limit;
    //printf("WARNING: FORCE TRESHOLD Y REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[1], force_limit);
  }
  else if (force.virtuose_force.force.y < -force_limit)
  {
    force.virtuose_force.force.y = -force_limit;
    spring_force[1]=-force_limit;
    // printf("WARNING: FORCE TRESHOLD Y REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[1], force_limit);
  }     


  //GENERATE Z FORCE

  force.virtuose_force.force.z = spring_force[2];
  if (force.virtuose_force.force.z > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.z = force_limit;
    spring_force[2]=force_limit;
    // printf("WARNING: FORCE TRESHOLD Z REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[2], force_limit);
  }
  else if (force.virtuose_force.force.z < -force_limit)
  {
    force.virtuose_force.force.z = -force_limit;
    spring_force[2]=-force_limit;
    // printf("WARNING: FORCE TRESHOLD Z REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[2], force_limit);
  }     
}
else 
{
        force.virtuose_force.force.x=0.0;
        force.virtuose_force.force.y = 0.0;
        force.virtuose_force.force.z=0.0;
}

//PUBLISH FORCE
    test_x=force.virtuose_force.force.x;
    test_y=force.virtuose_force.force.y;
    test_z=force.virtuose_force.force.z;


    if (abs(test_x)>force_limit || abs(test_y)>force_limit || abs(test_z)>force_limit)
      {
        printf("DANGEROUS FORCE FEEDBACK  TURN OFF THE GREEN BUTTON ");
        force.virtuose_force.force.x=0.0;
        force.virtuose_force.force.y = 0.0;
        force.virtuose_force.force.z=0.0;
      }

    
    //std::cout << "FORCE PUB -- x : -- " << force.virtuose_force.force.x << " -- y : -- " << force.virtuose_force.force.y << "-- z : -- " << force.virtuose_force.force.z<< "\n \n";
    in_virtuose_force.publish(force);
    
    ctr ++;
    // printf status every second
    // if (ctr % 500 == 0)
    // {
    //   printf("");
    //   //printf("Status: %ld %d %d %f %f %f\n", status_date - start, status_state, status_button,
    //    //      cur_pose_H[0], cur_pose_H[1], cur_pose_H[2]);
    // }
    r.sleep();
  }

// Reset virtuose_node
  virtuose_reset.call(res);

  return 0;
}