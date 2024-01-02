#include <iostream>
#include <vector>
#include <queue>
#include <cmath>

#include <ros/ros.h>

#include <geometry_msgs/Point.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Twist.h>

#include <nav_msgs/Odometry.h>
#include <nav_msgs/Path.h>
#include <nav_msgs/OccupancyGrid.h>

#include <geometry_msgs/TransformStamped.h>
#include <tf2_msgs/TFMessage.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <water/path.h>

class graph{
public:

    graph(){

    }

    graph(nav_msgs::OccupancyGrid map){
        this->map = map;	
    }

    nav_msgs::Path astar_path(geometry_msgs::Pose start, geometry_msgs::Pose end){

        
        long long int width = this->map.info.width;
        long long int height = this->map.info.height;

        nav_msgs::Path path;
        std::vector<long long int> parent(width * height, -1);
        std::vector<bool> explored(width * height, false);

        for (int t = 0; t < width*height; ++t){
            parent[t] = t;
        }

        std::vector<double> g_cost(width * height, INFINITY);
        std::vector<double> f_cost(width * height, INFINITY);

        long long int start_node = this->coordinates_to_node(start);
        long long int end_node = this->coordinates_to_node(end);

        double g = gcost(start_node, start_node);

        g_cost[start_node] = g;
        f_cost[start_node] = g + this->heuristic_cost(start_node, end_node);

        std::priority_queue<std::tuple<double, double, long long int>> open_set; // -fcost, gcost, node
        open_set.push(std::make_tuple(-f_cost[start_node], g_cost[start_node], start_node));

        while (!open_set.empty()){
            std::tuple<double, double, long long int> processing_node = open_set.top();
            if (std::get<2>(processing_node) == end_node){
                path.header.stamp = ros::Time::now();
                long long int current_node = end_node;
                long long int seq = 1;
                while (parent[current_node] != current_node){
                    geometry_msgs::PoseStamped pose;
                    pose.header.frame_id = "map";
                    pose.header.stamp = ros::Time::now();
                    pose.header.seq = seq++;
                    pose.pose = this->node_to_coordinates(current_node);
                    current_node = parent[current_node];
                    path.poses.push_back(pose);
                }return path;
            }

            
            open_set.pop();
            explored[std::get<2>(processing_node)] = true;
            std::vector<long long int> neighbours = this->neighbours(std::get<2>(processing_node), 1);
            for (long long int neighbour: neighbours){
                double potential_g = std::get<1>(processing_node) + this->gcost(std::get<2>(processing_node), neighbour);
                if (potential_g < g_cost[neighbour]){
                    g_cost[neighbour] = potential_g;
                    f_cost[neighbour] = potential_g + this->heuristic_cost(neighbour, end_node);
                    parent[neighbour] = std::get<2>(processing_node);
                    if (explored[neighbour]) continue;
                    open_set.push(std::make_tuple(-f_cost[neighbour], potential_g, neighbour));
                }
            }
        }return path;
    }

    void fill_outlines(int depth){

        std::vector<bool> is_duplicate(this->map.info.width * this->map.info.height, false);
        for (long long int y = 0; y < this->map.info.height; ++y){
            for (long long int x = 0; x < this->map.info.width; ++x){
                long long int n = y * this->map.info.width + x;
                if (this->map.data[n] == 100 && !is_duplicate[n]){
                    for (long long int dx = -depth; dx <= depth; dx += 2*depth){
                        for (long long int dy = -depth; dy <= depth; dy += 2*depth){
                            long long int deep_n = x + dx + this->map.info.width * (y + dy);
                            this->map.data[deep_n] = 100;
                            is_duplicate[deep_n] = true;
                        }
                    }
                }
            }
        }
    }



private:
    nav_msgs::OccupancyGrid map;

    std::vector<long long int> neighbours(long long int node, int depth){
        std::vector<long long int> nodes;

        long long int nodex = node%(this->map.info.width);
        long long int nodey = node/(this->map.info.width);
        
        for (int dx = -depth; dx <= depth; ++dx){
            for (int dy = -depth; dy <= depth; ++dy){
                if (dx == 0 && dy == 0) continue;
                long long int x = nodex + dx;
                long long int y = nodey + dy;
                if (this->is_within_map(x, y)){
                    long long int node_ = x + y * this->map.info.width;
                    if (this->map.data[node] == 100) continue;
                    nodes.push_back(node_);
                }
            }
        }return nodes;
    }

    bool is_within_map(int nodex, int nodey){
        if (nodex >= this->map.info.width || nodex < 0) return false;
        if (nodey >= this->map.info.height || nodey < 0) return false;
        return true;
    }

    long long int coordinates_to_node(geometry_msgs::Pose coordinates){
        long long int xnode = (coordinates.position.x - this->map.info.origin.position.x) / this->map.info.resolution;
        long long int ynode = (coordinates.position.y - this->map.info.origin.position.y) / this->map.info.resolution;
        long long int node = xnode + ynode * this->map.info.width;
        return node;
    }

    geometry_msgs::Pose node_to_coordinates(long long int node){
        geometry_msgs::Pose pose;
        
        pose.position.x = node%(this->map.info.width) * this->map.info.resolution + this->map.info.origin.position.x;
        pose.position.y = node/(this->map.info.width) * this->map.info.resolution + this->map.info.origin.position.y;
        pose.position.z = map.info.origin.position.z;

        pose.orientation.x = 0;
        pose.orientation.y = 0;
        pose.orientation.z = 0;
        pose.orientation.w = 1;

        return pose;
    }

    double heuristic_cost(long long int node, long long int end){
        return digna(node, end);
    }

    double gcost(long long start, long long int node){
        return digna(start, node);
    }

    double digna(int node1, int node2){
        if (node1 == node2) return 0.0;

        long long int width = this->map.info.width;
        //long long int height = this->map.info.height;

        long long int dx = abs(node1%width - node2%width);
        long long int dy = abs(node1/width - node2/width);

        return std::min(dx, dy) * sqrt(2) + abs(dx - dy);
    }
};

class start{
public:

    start(int argc, char **argv){
        ros::init(argc, argv, "auto_nav");
        ros::NodeHandle node;

        this->tf_sub = node.subscribe("/tf", 100, &start::get_tf, this);
        this->odom_sub = node.subscribe("/odom", 100, &start::get_odom, this);
        this->map_sub = node.subscribe("/map", 100, &start::get_map, this);
        //this->goal_sub = node.subscribe("/move_base_simple/goal", 100, &start::get_goal, this);

        this->coordinates_pub = node.advertise<geometry_msgs::Pose>("/bot_coordinates", 100, true);
        this->path_pub = node.advertise<nav_msgs::Path>("/path", 100, true);
        this->path_service = node.advertiseService("path_service", &start::process_path_service, this);
    }

    void run(int rate_){
        ros::Rate rate(rate_);
        ros::AsyncSpinner spinner(10);
        spinner.start();

        while (ros::ok()){

            if (!this->got_tf || !this->got_odom){
                std::cout << "trying to get tf and odom" << std::endl;
                continue;
            }

            tf2::doTransform(this->bot_odom, this->bot_coordinates, this->odom_to_map);
            //std::cout << this->bot_coordinates.position.x << ' ' << this->bot_coordinates.position.y << std::endl;
            this->coordinates_pub.publish(this->bot_coordinates);
            rate.sleep();
        }
    }

private:

    graph occupancy_matrix;

    geometry_msgs::Pose bot_odom;
    geometry_msgs::Pose bot_coordinates;
    geometry_msgs::TransformStamped odom_to_map;

    bool got_tf = false;
    bool got_odom = false;
    bool got_map = false;

    ros::Subscriber tf_sub;
    ros::Subscriber odom_sub;
    ros::Subscriber map_sub;
    //ros::Subscriber goal_sub;

    ros::ServiceServer path_service;

    ros::Publisher path_pub;
    ros::Publisher coordinates_pub;
    
    void get_tf(const tf2_msgs::TFMessage::ConstPtr &transforms){
        if (transforms->transforms[0].child_frame_id == "odom"){
            this->odom_to_map = transforms->transforms[0];
            this->got_tf = true;
        }
    }

    void get_odom(const nav_msgs::Odometry::ConstPtr &odom){
        this->bot_odom = odom->pose.pose;
        this->got_odom = true;
    }

//	void get_goal(const geometry_msgs::PoseStamped::ConstPtr &goal){
//		if (!this->got_map) return ;
//		nav_msgs::Path path = this->occupancy_matrix.astar_path(this->bot_coordinates, goal->pose);
//		path.header.frame_id = "map";
//		path.header.stamp = ros::Time::now();
//		path.header.seq = 0;
//		this->path_pub.publish(path);
//		std::cout << "published a path" << std::endl;
//	}

    void get_map(const nav_msgs::OccupancyGrid::ConstPtr &grid){
        this->occupancy_matrix = graph(*grid);
        this->occupancy_matrix.fill_outlines(3);
        this->got_map = true;
    }

    bool process_path_service(water::path::Request &req, water::path::Response &res){
        geometry_msgs::PoseStamped goal = req.goal_pose;
        nav_msgs::Path path = this->occupancy_matrix.astar_path(this->bot_coordinates, goal.pose);
        res.path = path;
        return true;
    }
};

int main(int argc, char **argv){

    start control(argc, argv);
    control.run(10);

    return 0;
}


