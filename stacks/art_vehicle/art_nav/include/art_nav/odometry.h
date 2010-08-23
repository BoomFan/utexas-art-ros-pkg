/* -*- mode: C++ -*-
 *
 *  Odometry class interface for observers driver.
 *
 *  Copyright (C) 2007, 2010, Austin Robot Technology
 *
 *  License: Modified BSD Software License Agreement
 *
 *  $Id$
 */

#ifndef __ODOMETRY_HH__
#define __ODOMETRY_HH__

#include <art/Position.h>
#include <nav_msgs/Odometry.h>

class Odometry
{
public:

  // public data
  nav_msgs::Odometry curr_pos;          // current position
  double time;				// last message timestamp
    
  Odometry(int _verbose);
  ~Odometry();

  int configure();
  int subscribe();

  Position::Pose3D *poselist;
  double *timelist;
  
  unsigned int listsize,list_start,list_end, list_curr;

  bool have_odom;

private:


  // .cfg variables:
  int verbose;				// log level verbosity

  // odometry interface
  ros::Subscriber odom_topic_;          // odom topic
};

#endif
