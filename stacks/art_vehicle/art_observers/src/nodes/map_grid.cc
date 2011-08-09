/*
 *  Copyright (C) 2010 UT-Austin &  Austin Robot Technology, Michael Quinlan
 * 
 *  License: Modified BSD Software License 
 */

/** \file

  This node create a pseudo occupancy grid on top of the maplanes data struture.

*/

#include <algorithm>

#include <ros/ros.h>

#include <art_obstacles/map_grid.h>

MapGrid::MapGrid(ros::NodeHandle* node) {
  node_ = node;

  tf_listener_= new tf::TransformListener();

  // Set the publishers
  visualization_publisher_ = node_->advertise <visualization_msgs::MarkerArray>("visualization_marker_array",1, true);
  nearest_front_publisher_ = node_->advertise <art_msgs::Observation>("nearest_front",1, true);
  nearest_rear_publisher_ = node_->advertise <art_msgs::Observation>("nearest_rear",1, true);
  
}

MapGrid::~MapGrid() {
  delete tf_listener_;
}

void MapGrid::processObstacles(const sensor_msgs::PointCloud &msg) {
   //ROS_INFO("num 3d points = %d", msg->points.size());
  obs_quads_.polygons.clear();
  added_quads_.clear();
  
	//if(msg.points.size()<1)
  //		return;
  
  transformPointCloud(msg);

  filterPointsInLocalMap();
 
  runObservers();
  
  publishObstacleVisualization();
}

void MapGrid::processLocalMap(const art_msgs::ArtLanes &msg) {
  local_map_ = msg;
}

void MapGrid::filterPointsInLocalMap() {  
 // set the exact point cloud size -- the vectors should already have
  // enough space
  size_t npoints = transformed_obstacles_.points.size();
  added_quads_.clear();
  for (unsigned i = 0; i < npoints; ++i)   {
      isPointInAPolygon(transformed_obstacles_.points[i].x,transformed_obstacles_.points[i].y);
  }
}

void MapGrid::transformPointCloud(const sensor_msgs::PointCloud &msg) {
  try {
    tf_listener_->transformPointCloud("/map", msg, transformed_obstacles_);

    calcRobotPolygon(); // Hopefully not needed in future
 } catch (tf::TransformException ex){
    ROS_ERROR("%s",ex.what());
  }
}

bool MapGrid::isPointInAPolygon(float x, float y) {
  size_t num_polys = local_map_.polygons.size();
  
  bool inside = false;
  std::pair<std::tr1::unordered_set<int>::iterator, bool> pib;
  
  for (size_t i=0; i<num_polys; i++) {
    art_msgs::ArtQuadrilateral *p= &(local_map_.polygons[i]);
    float dist=(p->midpoint.x-x)*(p->midpoint.x-x) + (p->midpoint.y-y)*(p->midpoint.y-y) ;
    if (dist>16) continue; // quickly check if we are near the polygon
    inside = quad_ops::quickPointInPolyRatio(x,y,*p,0.6);  //quad_ops::quickPointInPoly(x,y,*p); 
    if (inside) {
      pib = added_quads_.insert(p->poly_id);
      if (pib.second) {
        obs_quads_.polygons.push_back(*p);
      }
    }
  }

  return inside;
}

void MapGrid::runObservers() {
  // Extract polygons for nearest front observer
  art_msgs::ArtLanes nearest_front_quads = quad_ops::filterLanes(robot_polygon_,local_map_,*quad_ops::compare_forward_seg_lane);
  art_msgs::ArtLanes nearest_front_obstacles = quad_ops::filterLanes(robot_polygon_,obs_quads_,*quad_ops::compare_forward_seg_lane);

  // Run the observer
  art_msgs::Observation n_f = nearest_front_observer_.update(robot_polygon_.poly_id,nearest_front_quads,nearest_front_obstacles);

  // Extract polygons for nearest rear observer
  art_msgs::ArtLanes nearest_rear_quads = quad_ops::filterLanes(robot_polygon_,local_map_,*quad_ops::compare_backward_seg_lane);
  art_msgs::ArtLanes nearest_rear_obstacles = quad_ops::filterLanes(robot_polygon_,obs_quads_,*quad_ops::compare_backward_seg_lane);

  // Reverse the vectors because the observer experts polygons in order of distance from base polyon
  std::reverse(nearest_rear_quads.polygons.begin(),nearest_rear_quads.polygons.end());
  std::reverse(nearest_rear_obstacles.polygons.begin(),nearest_rear_obstacles.polygons.end());

  // Run the observer
  art_msgs::Observation n_r = nearest_rear_observer_.update(robot_polygon_.poly_id,nearest_rear_quads,nearest_rear_obstacles);

  // Publish observations
  nearest_front_publisher_.publish(n_f);
  nearest_rear_publisher_.publish(n_r);
}                                                                     


void MapGrid::calcRobotPolygon() {
  // --- Hack to get my own polygon
  geometry_msgs::PointStamped laser_point;
  laser_point.header.frame_id = "/vehicle";  
  laser_point.header.stamp = ros::Time();
  
  laser_point.point.x = 0.0;
  laser_point.point.y = 0.0;
  laser_point.point.z = 0.0;
  
  geometry_msgs::PointStamped robot_point;
  tf_listener_->transformPoint("/map", laser_point, robot_point);

  size_t numPolys = local_map_.polygons.size();
  float x = robot_point.point.x; 
  float y = robot_point.point.y;
  bool inside=false;
  for (size_t i=0; i<numPolys; i++) {
    art_msgs::ArtQuadrilateral *p= &(local_map_.polygons[i]);
    float dist=(p->midpoint.x-x)*(p->midpoint.x-x) + (p->midpoint.y-y)*(p->midpoint.y-y) ;
    if (dist>16) continue; // quickly check if we are near the polygon
    inside = quad_ops::quickPointInPoly(x,y,*p); 
    if (inside) {
      robot_polygon_ = *p;
    }
  }
}

void MapGrid::publishObstacleVisualization()
{
  if (visualization_publisher_.getNumSubscribers()==0) {
    return;
  }
  ros::Time now = ros::Time::now();
  // clear message array, this is a class variable to avoid memory
  // allocation and deallocation on every cycle
  marks_msg_.markers.clear();

  int i=0;
  for (obs_it_=obs_quads_.polygons.begin(); obs_it_!=obs_quads_.polygons.end(); obs_it_++) {
    visualization_msgs::Marker mark;
    mark.header.stamp = now;
    mark.header.frame_id = "/map";
    
    mark.ns = "obstacle_polygons";
    mark.id = (int32_t) i;
    mark.type = visualization_msgs::Marker::CUBE;
    mark.action = visualization_msgs::Marker::ADD;
    
    mark.pose.position = obs_it_->midpoint;
    mark.pose.orientation = tf::createQuaternionMsgFromYaw(obs_it_->heading);
    
    mark.scale.x = 1.5;
    mark.scale.y = 1.5;
    mark.scale.z = 0.1;
    mark.lifetime =  ros::Duration(0.2);
    
    mark.color.a = 0.8;     // way-points are slightly transparent
    mark.color.r = 0.0;
    mark.color.g = 0.0;
    mark.color.b = 1.0;
    
    marks_msg_.markers.push_back(mark);
    i++;
  }

  // Draw the polygon containing the robot
  visualization_msgs::Marker mark;
  mark.header.stamp = now;
  mark.header.frame_id = "/map";
  
  mark.ns = "obstacle_polygons";
  mark.id = (int32_t) i;
  mark.type = visualization_msgs::Marker::CUBE;
  mark.action = visualization_msgs::Marker::ADD;
    
  mark.pose.position = robot_polygon_.midpoint;
  mark.pose.orientation = tf::createQuaternionMsgFromYaw(robot_polygon_.heading);
    
  mark.scale.x = 1.5;
  mark.scale.y = 1.5;
  mark.scale.z = 0.1;
  mark.lifetime =  ros::Duration(0.2);
  
  mark.color.a = 0.8;     // way-points are slightly transparent
  mark.color.r = 0.3;
  mark.color.g = 0.7;
  mark.color.b = 0.9;
  
  marks_msg_.markers.push_back(mark);

  // Publish the markers
  visualization_publisher_.publish(marks_msg_);
}