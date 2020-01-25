#include "ros_img_processor_node.h"

//OpenCV
#include "opencv2/opencv.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"

//std
#include <iostream>
#include <cstdlib>
#include <vector>

//constants
const int GAUSSIAN_BLUR_SIZE = 7;
const double GAUSSIAN_BLUR_SIGMA = 2;
const double CANNY_EDGE_TH = 150;
const double HOUGH_ACCUM_RESOLUTION = 2;
const double MIN_CIRCLE_DIST = 40;
const double HOUGH_ACCUM_TH = 80;
const int MIN_RADIUS = 20;
const int MAX_RADIUS = 100;

RosImgProcessorNode::RosImgProcessorNode() :
    nh_(ros::this_node::getName()),
    img_tp_(nh_)
{
	//loop rate [hz], Could be set from a yaml file
	rate_=10;

	//sets publishers
	image_pub_ = img_tp_.advertise("image_out", 100);
	marker_publisher_ = nh_.advertise<visualization_msgs::Marker>( "arrow_marker", 1 );

	//sets subscribers
	image_subs_ = img_tp_.subscribe("image_in", 1, &RosImgProcessorNode::imageCallback, this);
	camera_info_subs_ = nh_.subscribe("camera_info_in", 100, &RosImgProcessorNode::cameraInfoCallback, this);
}

RosImgProcessorNode::~RosImgProcessorNode()
{
    //
}

void RosImgProcessorNode::process()
{
    cv::Rect_<int> box;

    //check if new image is there
    if ( cv_img_ptr_in_ != nullptr )
    {
        // copy the input image to the out one
        cv_img_out_.image = cv_img_ptr_in_->image;

		// find the ball

    cv::Mat gray_image;
    std::vector<cv::Vec3f> circles;
    cv::Point center;
    int radius;

    //clear previous circles
    circles.clear();

    // If input image is RGB, convert it to gray
    cv::cvtColor(cv_img_out_.image, gray_image, CV_BGR2GRAY);

    //Reduce the noise so we avoid false circle detection
    cv::GaussianBlur( gray_image, gray_image, cv::Size(GAUSSIAN_BLUR_SIZE, GAUSSIAN_BLUR_SIZE), GAUSSIAN_BLUR_SIGMA );

    //Apply the Hough Transform to find the circles
    cv::HoughCircles( gray_image, circles, CV_HOUGH_GRADIENT, HOUGH_ACCUM_RESOLUTION, MIN_CIRCLE_DIST, CANNY_EDGE_TH, HOUGH_ACCUM_TH, MIN_RADIUS, MAX_RADIUS );

    //draw circles on the image
    for(unsigned int ii = 0; ii < circles.size(); ii++ )
    {
        if ( circles[ii][0] != -1 )
        {
                center = cv::Point(cvRound(circles[ii][0]), cvRound(circles[ii][1]));
                radius = cvRound(circles[ii][2]);
                cv::circle(cv_img_out_.image, center, 5, cv::Scalar(0,255,0), -1, 8, 0 );// circle center in green
                cv::circle(cv_img_out_.image, center, radius, cv::Scalar(255,0,0), 3, 8, 0 );// circle perimeter in red
        }


		// find the direction vector
			 direction_ << center.x,center.y,1;  // just to draw something with the arrow marker
       direction_ << matrixK_.inverse()*direction_;
       // draw a bounding box around the ball
        box.x = center.x-radius;
        box.y = center.y-radius;
        box.width = radius*2+1;
        box.height = radius*2+1;
        cv::rectangle(cv_img_out_.image, box, cv::Scalar(0,255,255), 3);
      }
    }

    //reset input image
    cv_img_ptr_in_ = nullptr;
}

void RosImgProcessorNode::publishImage()
{
	if( !cv_img_out_.image.data ) return;

    cv_img_out_.header.seq ++;
    cv_img_out_.header.stamp = ros::Time::now();
    cv_img_out_.header.frame_id = "camera";
    cv_img_out_.encoding = img_encoding_;
    image_pub_.publish(cv_img_out_.toImageMsg());
}

void RosImgProcessorNode::publishMarker()
{
	if( !cv_img_out_.image.data) return; //to assure that we enter here when an image is available

	visualization_msgs::Marker marker_msg;
	std::ptrdiff_t idx;
	Eigen::Matrix3d rotation;

	//from vector direction to quaternion
	rotation.block<3,1>(0,0) = direction_;
	direction_.minCoeff(&idx);
	switch(idx)
	{
		case 0:
			rotation.block<3,1>(0,1) << 0,direction_(2),-direction_(1);
			break;
		case 1:
			rotation.block<3,1>(0,1) << -direction_(2),0,direction_(0);
			break;
		case 2:
			rotation.block<3,1>(0,1) << direction_(1),-direction_(0),0;
			break;
		default:
			break;
	}
	rotation.block<3,1>(0,2) = rotation.block<3,1>(0,0).cross(rotation.block<3,1>(0,1));
	rotation.block<3,1>(0,0) = rotation.block<3,1>(0,0)/rotation.block<3,1>(0,0).norm();
	rotation.block<3,1>(0,1) = rotation.block<3,1>(0,1)/rotation.block<3,1>(0,1).norm();
	rotation.block<3,1>(0,2) = rotation.block<3,1>(0,2)/rotation.block<3,1>(0,2).norm();
	//std::cout << "rotation: " << std::endl << rotation << std::endl;
	Eigen::Quaterniond quaternion(rotation);

	// fill the arrow message
	marker_msg.header.stamp = ros::Time::now();
	marker_msg.header.frame_id = "camera";
	marker_msg.ns = "direction_marker";
	marker_msg.id = 1;
	marker_msg.action = visualization_msgs::Marker::ADD;
	marker_msg.type = visualization_msgs::Marker::ARROW;
	marker_msg.pose.position.x = 0;
	marker_msg.pose.position.y = 0;
	marker_msg.pose.position.z = 0;
	marker_msg.pose.orientation.x = quaternion.x();
	marker_msg.pose.orientation.y = quaternion.y();
	marker_msg.pose.orientation.z = quaternion.z();
	marker_msg.pose.orientation.w = quaternion.w();
	marker_msg.scale.x = 0.8;
	marker_msg.scale.y = 0.02;
	marker_msg.scale.z = 0.02;
	marker_msg.color.r = 1.0;
	marker_msg.color.g = 0.0;
	marker_msg.color.b = 1.0;
	marker_msg.color.a = 1.0;
	marker_msg.lifetime = ros::Duration(0.2);

	//publish marker
	marker_publisher_.publish(marker_msg);
}

double RosImgProcessorNode::getRate() const
{
    return rate_;
}

void RosImgProcessorNode::imageCallback(const sensor_msgs::ImageConstPtr& _msg)
{
    try
    {
        img_encoding_ = _msg->encoding;//get image encodings
        cv_img_ptr_in_ = cv_bridge::toCvCopy(_msg, _msg->encoding);//get image
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("RosImgProcessorNode::image_callback(): cv_bridge exception: %s", e.what());
        return;
    }
}

void RosImgProcessorNode::cameraInfoCallback(const sensor_msgs::CameraInfo & _msg)
{
	matrixK_  << _msg.K[0],_msg.K[1],_msg.K[2],
                 _msg.K[3],_msg.K[4],_msg.K[5],
                 _msg.K[6],_msg.K[7],_msg.K[8];
	//std::cout << "matrixK: " << std::endl << matrixK_ << std::endl;
}
