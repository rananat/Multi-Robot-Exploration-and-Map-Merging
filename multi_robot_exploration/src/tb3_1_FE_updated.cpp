/// \file
/// \brief This node causes the robot to move autonomously via Frontier Exploration

#include <ros/ros.h>
#include <std_msgs/Header.h>
#include <nav_msgs/OccupancyGrid.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf2_ros/transform_listener.h>
#include <geometry_msgs/TransformStamped.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <visualization_msgs/MarkerArray.h>
#include <std_srvs/Empty.h>
#include <multi_robot_exploration/tb3_1_start.h>
#include <vector>
#include <iostream>
#include <cmath> 
#include <typeinfo>


class FrontExpl
{
    public:
        FrontExpl()
        {
            tb3_1_FE_map_pub = nh.advertise<nav_msgs::OccupancyGrid>("edges_map_1", 1);
            region_map_pub = nh.advertise<nav_msgs::OccupancyGrid>("region_map_1", 1);
            map_1_sub = nh.subscribe<nav_msgs::OccupancyGrid>("tb3_1/map", 10, &FrontExpl::mapCallback, this);
            marker_pub = nh.advertise<visualization_msgs::MarkerArray>("markers_1", 1, true);
            tf2_ros::TransformListener tfListener(tfBuffer);
            MoveBaseClient ac("tb3_1/move_base", true);

            // Create the start service 
            start_srv = nh.advertiseService("tb3_1_start", &FrontExpl::startCallback, this);

            std::cout << "Initialized all the things" << std::endl;
        }

        /// \brief Starts the frontier exploration
        /// \param req - service request
        /// \param res - service response
        /// \returns true when done
        bool startCallback(multi_robot_exploration::tb3_1_start::Request &req, multi_robot_exploration::tb3_1_start::Response &res)
        {
            std::cout << "Got to start service" << std::endl;
            start_flag = true;
            return true;
        }

        /// / \brief Grabs the position of the robot from the pose subscriber and stores it
        /// / \param msg - pose message
        /// \returns nothing
        void mapCallback(const nav_msgs::OccupancyGrid::ConstPtr & msg)
        {
            map_1_msg.header = msg->header;
            map_1_msg.info = msg->info;
            map_1_msg.data = msg->data;

            std::cout << "Got to map callback" << std::endl;
        }

        void neighborhood(int cell)
        {
            // std::cout << "Finding neighborhood values " << std::endl;

            neighbor_index.clear();
            neighbor_value.clear();

            // Store neighboring values and indexes
            neighbor_index.push_back(cell - map_width - 1);
            neighbor_index.push_back(cell - map_width);
            neighbor_index.push_back(cell - map_width + 1);
            neighbor_index.push_back(cell-1);
            neighbor_index.push_back(cell+1);
            neighbor_index.push_back(cell + map_width - 1);
            neighbor_index.push_back(cell + map_width);
            neighbor_index.push_back(cell + map_width + 1);

            sort( neighbor_index.begin(), neighbor_index.end() );
            neighbor_index.erase( unique( neighbor_index.begin(), neighbor_index.end() ), neighbor_index.end() );
        }

        void find_all_edges()
        {
            std::cout << "Finding all frontier edges" << std::endl;
            // For all unknown points in the occupancy grid, find the value of the neighbors
            // Starting one row up and on space in so that we dont have an issues with the first point
                for (double x = map_width + 1; x < (map_width * map_height) - map_width; x++)
                {
                    if (map_data[x] == -1)
                    {
                        // If there is an unknown cell then check neighboring cells to find potential frontier edge (free cell)
                        neighborhood(x);

                        sort( neighbor_index.begin(), neighbor_index.end() );
                        neighbor_index.erase( unique( neighbor_index.begin(), neighbor_index.end() ), neighbor_index.end() );

                        for(int i = 0; i < neighbor_index.size(); i++)
                        {
                            // Am I accidentally adding values multiple times here??
                            if (map_data[neighbor_index[i]] == 0)
                            {
                                // If were actually at an edge, mark it
                                mark_edge = neighbor_index[i];
                                FE_tb3_1_map.data[mark_edge] = 10;
                                edge_vec.push_back(mark_edge);

                                num_edges++;
                                mark_edge = 0;

                            }
                        }

                    }
                } 

        }

        bool check_edges(int curr_cell, int next_cell)
        {
            // std::cout << "Checking to make sure there arent any repeat edges" << std::endl;
            if (curr_cell == next_cell)
            {
                return false;
            }
            else
            {
                return true;
            }
        }

        void find_regions()
        {
            std::cout << "Finding frontier regions " << std::endl;
            // std::vector<unsigned int> temp_group;

            for (int q = 0; q < num_edges; q++)
            {
                unique_flag = check_edges(edge_vec[q], edge_vec[q+1]);

                if (unique_flag == true)
                {
                    // std::cout << "Finding regions around cell " << edge_vec[q] << std::endl;
                    // std::cout << "Evaluating against " << edge_vec[q+1] << std::endl;

                    neighborhood(edge_vec[q]);

                    for(int i = 0; i < neighbor_index.size(); i++)
                    {
                        // std::cout << "Neighbor index is " << neighbor_index[i] << std::endl;

                        if (neighbor_index[i] == edge_vec[q+1])
                        {
                            edge_index = edge_vec[q]; 
                            // next_edge_index = edge_vec[q+1];
                            region_map.data[edge_index] = 110 + (20*region_c);
                            region_map.data[next_edge_index] = 110 + (20*region_c);
                            temp_group.push_back(edge_vec[q]);
                            // temp_group.push_back(edge_vec[q+1]); // Just added this, maybe I should get rid of it

                            // std::cout << "Adding this value to temp group " << temp_group[group_c] << std::endl;

                            sort( temp_group.begin(), temp_group.end() );
                            temp_group .erase( unique( temp_group.begin(), temp_group.end() ), temp_group.end() );

                            group_c++;
                        }
                    }

                    if (group_c == prev_group_c) // then we didnt add any edges to our region
                    {
                        if (group_c < 4) // frontier region too small
                        {
                        }
                        
                        else
                        {
                            // std::cout << "Size of group is " << group_c << std::endl;

                            ///////////////////////////////////////
                            // Write something that says if we move to close to wall, skip
                            //////////////////////////////////////

                            centroid = group_c / 2;
                            // centroid_index = temp_group[centroid];
                            // centroid = 0;
                            centroid_index = temp_group[centroid];

                            // std::cout << "Centroid is number " << centroid << " out of " << group_c << std::endl;
                            // std::cout << "Centroid index is " << centroid_index << std::endl;

                            centroids.push_back(centroid_index);
                            region_c++;

                            // std::cout << "Number of regions is now " << region_c << std::endl;
                        }

                        // std::cout << "Number of regions total is " << region_c-1 << std::endl;
                        // std::cout << "And number of centorids is " << centroids.size() << std::endl;

                        // Reset group array and counter
                        group_c = 0;
                        // std::vector<unsigned int> temp_group;
                        // std::cout << "Value in current temp group are " << temp_group[0] << std::endl;

                        // for (int g = 0; g < temp_group.size(); g++)
                        // {
                        //     std::cout << temp_group[g] << std::endl;
                        // } 

                        temp_group.clear();
                        // std::cout << "Make sure we clear the temp_group " << temp_group[0] <<std::endl;
                    }
                    else
                    {
                        prev_group_c = group_c;
                    }
                }

                else
                {
                    // std::cout << "Got a duplicate cell" << std::endl;
                }

            }

        }

        void find_transform()
        {
            std::cout << "Locating the robot's position..." << std::endl;
            // Find current location and move to the nearest centroid
            // Get robot pose
            transformS = tfBuffer.lookupTransform(map_frame, body_frame, ros::Time(0), ros::Duration(3.0));

            transformS.header.stamp = ros::Time();
            transformS.header.frame_id = body_frame;
            transformS.child_frame_id = map_frame;

            robot_pose_.position.x = transformS.transform.translation.x;
            robot_pose_.position.y = transformS.transform.translation.y;
            robot_pose_.position.z = 0.0;
            robot_pose_.orientation.x = transformS.transform.rotation.x;
            robot_pose_.orientation.y = transformS.transform.rotation.y;
            robot_pose_.orientation.z = transformS.transform.rotation.z;
            robot_pose_.orientation.w = transformS.transform.rotation.w;

            std::cout << "Robot pose is  " << robot_pose_.position.x << " , " << robot_pose_.position.y << std::endl;

            // robot_cent_x = robot_pose_.position.x;
            // robot_cent_y = robot_pose_.position.y;
        }

        // void mark_centroids()
        // {
        //     // std::cout << "Publishing markers" << std::endl;
        //     visualization_msgs::MarkerArray centroid_arr;
        //     marker_pub.publish(centroid_arr);

        //     centroid_arr.markers.resize(centroid_Xpts.size());

        //     for (int i = 0; i < centroid_Xpts.size(); i++) 
        //     {
        //         centroid_arr.markers[i].header.frame_id = "tb3_1/map";
        //         centroid_arr.markers[i].header.stamp = ros::Time();
        //         centroid_arr.markers[i].ns = "centroid";
        //         centroid_arr.markers[i].id = i;

        //         centroid_arr.markers[i].type = visualization_msgs::Marker::CYLINDER;
        //         centroid_arr.markers[i].action = visualization_msgs::Marker::ADD;
        //         centroid_arr.markers[i].lifetime = ros::Duration(10);

        //         centroid_arr.markers[i].pose.position.x = centroid_Xpts[i];
        //         centroid_arr.markers[i].pose.position.y = centroid_Ypts[i];
        //         centroid_arr.markers[i].pose.position.z = 0;
        //         centroid_arr.markers[i].pose.orientation.x = 0.0;
        //         centroid_arr.markers[i].pose.orientation.y = 0.0;
        //         centroid_arr.markers[i].pose.orientation.z = 0.0;
        //         centroid_arr.markers[i].pose.orientation.w = 1.0;

        //         centroid_arr.markers[i].scale.x = 0.1;
        //         centroid_arr.markers[i].scale.y = 0.1;
        //         centroid_arr.markers[i].scale.z = 0.1;

        //         centroid_arr.markers[i].color.a = 1.0;
        //         centroid_arr.markers[i].color.r = 1.0;
        //         centroid_arr.markers[i].color.g = 0.0;
        //         centroid_arr.markers[i].color.b = 1.0;
        //     }

        //     marker_pub.publish(centroid_arr);
        // }

        void centroid_index_to_point(double &last_cent_x, double &last_cent_y, double &last_2cent_x, double &last_2cent_y)
        {
            std::cout << "Calculating centroids" << std::endl;
            dist = 0;
            for (int t = 0; t < region_c-1; t++)
            {
                point.x = (centroids[t] % region_map.info.width)*region_map.info.resolution + region_map.info.origin.position.x;
                point.y = floor(centroids[t] / region_map.info.width)*region_map.info.resolution + region_map.info.origin.position.y;

                // std::cout << "Diff with last centroid " << fabs(point.x - last_cent_x) << " , " << fabs(point.y - last_cent_y) << std::endl;
                // std::cout << "Diff with 2nd last centroid " << fabs(point.x - last_2cent_x) << " , " << fabs(point.y - last_2cent_y) << std::endl;

                if((fabs(point.x - last_cent_x) < 0.1) && (fabs(point.y - last_cent_y) < 0.1))
                {
                    std::cout << "GOT THE SAME CENTROID, PASS " << std::endl;
                    centroids.erase(centroids.begin() + (t-1));
                }
                else if((fabs(point.x - last_2cent_x) < 0.1) && (fabs(point.y - last_2cent_y) < 0.1))
                {
                    std::cout << "Got the same centroid again, pass " << std::endl;
                }
                // else if((fabs(point.x) < 0.1) && (fabs(point.y) < 0.1))
                // {
                //     // std::cout << "Too close to the origin, why? " << std::endl;
                // }
                // But why are we getting the map origin as a centroid...?
                // else if((fabs(point.x - robot_cent_x) < 0.25) || (fabs(point.y - robot_cent_y) < 0.25))
                // {
                //     // std::cout << "Got the same centroid again, pass " << std::endl;
                // }
                else if((point.x < map_1_msg.info.origin.position.x + 0.05) && (point.y < map_1_msg.info.origin.position.y + 0.05))
                {
                    // std::cout << "Why are we even near the map origin? " << std::endl;
                }
                else
                {
                    // std::cout << "Centroid point is " << point.x << " , " << point.y << std::endl;
                    centroid_Xpts.push_back(point.x);
                    centroid_Ypts.push_back(point.y);

                    // mark_centroids();

                    double delta_x = point.x - robot_pose_.position.x; 
                    double delta_y = point.y - robot_pose_.position.y; 
                    double sum = (pow(delta_x ,2)) + (pow(delta_y ,2));
                    dist = pow( sum , 0.5 );

                    // std::cout << "Distance is " << dist << std::endl;
                    dist_arr.push_back(dist);
                }
            }
        }

        void find_closest_centroid(std::vector<double> &dist_arr)
        {
            std::cout << "Determining the closest centroid out of this many " << dist_arr.size() << std::endl;
            // Find spot to move to that is close
            smallest = 9999999.0;
            for(int u = 0; u < region_c-1; u++)
            {
                // std::cout << "Current distance value is " << dist_arr[u] << " at index " << u << std::endl;

                if (dist_arr[u] < 0.2)
                {
                    // std::cout << "Index that is too small " << dist_arr[u] << std::endl;
                }
                else if (dist_arr[u] < smallest)
                {
                    smallest = dist_arr[u];
                    // std::cout << "Smallest DISTANCE value is " << smallest << std::endl;
                    move_to_pt = u;
                    // std::cout << "Index we need to move to is ... (should be less than 10) " << move_to_pt << std::endl;
                }
                else
                {
                    // std::cout << "This distance is bigger then 0.1 and the smallest value " << std::endl;
                }
            }
        }

        void main_loop()
        {
            // typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
            MoveBaseClient ac("tb3_1/move_base", true);

            //wait for the action server to come up
            while(!ac.waitForServer(ros::Duration(5.0)))
            {
                ROS_INFO("Waiting for the move_base action server to come up");
            }
            ROS_INFO("Connected to move base server");

            tf2_ros::TransformListener tfListener(tfBuffer);
            ros::Rate loop_rate(1);

            while (ros::ok())
            {
                ros::spinOnce();
                
                if (start_flag == true)
                {
                    // Map definitions for the edges and region maps
                    FE_tb3_1_map.header.frame_id = "edges_map_1";
                    FE_tb3_1_map.info.resolution = map_1_msg.info.resolution;
                    FE_tb3_1_map.info.width = map_1_msg.info.width;
                    FE_tb3_1_map.info.height = map_1_msg.info.height;
                    FE_tb3_1_map.info.origin.position.x = map_1_msg.info.origin.position.x;
                    FE_tb3_1_map.info.origin.position.y = map_1_msg.info.origin.position.y ;
                    FE_tb3_1_map.info.origin.position.z = map_1_msg.info.origin.position.z;
                    FE_tb3_1_map.info.origin.orientation.w = map_1_msg.info.origin.orientation.w;
                    FE_tb3_1_map.data = map_1_msg.data;

                    if (map_1_msg.data.size()!=0)
                    {

                        // Store the map data in a vector so we can read that shit
                        int map_size = map_1_msg.data.size();
                        // map_data.resize(0);
                        map_data.clear();
                        for (int q = 0; q < map_size; q++)
                        {
                            map_data.push_back(map_1_msg.data[q]);
                        }

                        region_map.header.frame_id = "region_map_1";
                        region_map.info.resolution = map_1_msg.info.resolution;
                        region_map.info.width = map_1_msg.info.width;
                        region_map.info.height = map_1_msg.info.height;
                        region_map.info.origin.position.x = map_1_msg.info.origin.position.x;
                        region_map.info.origin.position.y = map_1_msg.info.origin.position.y ;
                        region_map.info.origin.position.z = map_1_msg.info.origin.position.z;
                        region_map.info.origin.orientation.w = map_1_msg.info.origin.orientation.w;
                        region_map.data = map_1_msg.data;

                        // Store the map data in a vector so we can read that shit
                        int region_map_size = map_1_msg.data.size();
                        // region_map_data.resize(0);
                        region_map_data.clear();
                        for (int q = 0; q < region_map_size; q++)
                        {
                            region_map_data.push_back(map_1_msg.data[q]);
                        }

                        // Start with inital number of edges is 1 and initalize vector to store edge index
                        num_edges = 0;
                        // edge_vec.resize(0);
                        // edge_vec.clear();
                        map_width = FE_tb3_1_map.info.width;
                        map_height = FE_tb3_1_map.info.height;
                        int region_map_width = region_map.info.width;
                        int region_map_height = region_map.info.height;

                        // std::cout << "Map width and height " << map_width << " , " << map_height << std::endl;
                        // std::cout << "Region map width and height " << region_map_width << " , " << region_map_height << std::endl;

                        find_all_edges();

                        sort( edge_vec.begin(), edge_vec.end() );
                        edge_vec.erase( unique( edge_vec.begin(), edge_vec.end() ), edge_vec.end() );

                        tb3_1_FE_map_pub.publish(FE_tb3_1_map);
                        // std::cout << "Number of edges is " << num_edges << " or " << edge_vec.size() << std::endl;

                        //////////////////////////////////////////////////////////////////////////////////////////////////////////
                        // ALL GOOD AT GETTING EDGES
                        // NEED TO FIX HOW WERE CREATING FRONTIERS
                        //////////////////////////////////////////////////////////////////////////////////////////////////////////

                        find_regions();
                        region_map_pub.publish(region_map);

                        find_transform();

                        centroid_index_to_point(last_cent_x, last_cent_y, last_2cent_x, last_2cent_y);
                        // std::cout << "Number of centroids should match number of regions " << centroids.size() << " =? " << region_c << std::endl;

                        find_closest_centroid(dist_arr);

                        std::cout << "Centroid we gonna go is " << centroid_Xpts[move_to_pt] << " , " <<  centroid_Ypts[move_to_pt] << std::endl;
                        std::cout << "At a distance of " << smallest << std::endl;

                        std::cout << "Last centroid values " <<  last_cent_x << " , " << last_cent_y << std::endl;
                        std::cout << "Last LAST centroid values " <<  last_2cent_x << " , " << last_2cent_y << std::endl;

                        last_2cent_x = last_cent_x;
                        last_2cent_y = last_cent_y;
                        last_cent_x = centroid_Xpts[move_to_pt];
                        last_cent_y = centroid_Ypts[move_to_pt];

                        // // STORE LAST CENTROID POINT AND SAY IF WE JUST WENT THERE, DONT GO AGAIN

                        goal.target_pose.header.frame_id = "tb3_1/map"; // Needs to be AN ACTUAL FRAME
                        // goal.target_pose.header.frame_id = "map"; // Needs to be AN ACTUAL FRAME
                        goal.target_pose.header.stamp = ros::Time();
                        goal.target_pose.pose.position.x = centroid_Xpts[move_to_pt];
                        goal.target_pose.pose.position.y = centroid_Ypts[move_to_pt];
                        goal.target_pose.pose.orientation.w = 1.0;

                        // std::cout << "Goal is " << goal.target_pose.pose.position.x << " , " <<  goal.target_pose.pose.position.y << std::endl;
                        ROS_INFO("Sending goal");
                        ac.sendGoal(goal);

                        // Wait for the action to return
                        ac.waitForResult(ros::Duration(60));

                        if (ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
                        {
                            ROS_INFO("You have reached the goal!");
                        }
                        else
                        {
                            ROS_INFO("The base failed for some reason");
                        }

                        std::cout << "" << std::endl;
                    }

                    else
                    {
                        ROS_INFO("Waiting for first map");
                    }

                    // Reset variables 
                    num_edges = 0, group_c = 0, prev_group_c = 0, region_c = 1, centroid = 0, centroid_index = 0, move_to_pt = 0;
                    map_width = 0, map_height = 0, edge_size = 0, mark_edge = 0, edge_index = 0, next_edge_index = 0;
                    dist = 0.0, smallest = 9999999.0;
                    // last_cent_x = 0.0, last_cent_y = 0.0, last_2cent_x = 0.0, last_2cent_y = 0.0, 

                    edge_vec.clear();
                    neighbor_index.clear();
                    neighbor_value.clear();
                    map_data.clear();
                    region_map_data.clear();
                    centroids.clear();
                    temp_group.clear();
                    centroid_Xpts.clear();
                    centroid_Ypts.clear();
                    dist_arr.clear();
                    std::cout << "Reset all values and cleared all arrays" << std::endl;
                }

                loop_rate.sleep();
            }
        }

    private:
        ros::NodeHandle nh;
        ros::Publisher tb3_1_FE_map_pub, region_map_pub, marker_pub;
        ros::Subscriber map_1_sub;
        ros::ServiceServer start_srv;
        nav_msgs::OccupancyGrid map_1_msg;
        nav_msgs::OccupancyGrid FE_tb3_1_map, region_map;
        tf2_ros::Buffer tfBuffer;
        geometry_msgs::Pose robot_pose_;
        geometry_msgs::Point point;
        geometry_msgs::TransformStamped transformS;
        move_base_msgs::MoveBaseGoal goal_init;
        move_base_msgs::MoveBaseGoal goal;
        std::string map_frame = "tb3_1/map";
        std::string body_frame = "tb3_1/base_footprint";
        typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
        std::vector<signed int> edge_vec, neighbor_index, neighbor_value, map_data, region_map_data;
        std::vector<unsigned int> centroids,temp_group;
        std::vector<double> centroid_Xpts, centroid_Ypts, dist_arr;
        int num_edges = 0, group_c = 0, prev_group_c = 0, region_c = 1, centroid = 0, centroid_index = 0, move_to_pt = 0;
        int map_width = 0, map_height = 0, edge_size = 0, mark_edge = 0, edge_index = 0, next_edge_index = 0;
        double dist = 0.0, last_cent_x = 0.0, last_cent_y = 0.0, last_2cent_x = 0.0, last_2cent_y = 0.0, smallest = 9999999.0;
        bool unique_flag = true;
        bool start_flag = false;
};

int main(int argc, char * argv[])
{
    ros::init(argc, argv, "tb3_1_FE_node");
    FrontExpl FE;
    FE.main_loop();
    ros::spin();

    return 0;
}