#ifndef ros_img_processor_node_H
#define ros_img_processor_node_H

//std C++
#include <iostream>

//Eigen
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>

//ROS headers for image I/O
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/CameraInfo.h>
#include <visualization_msgs/Marker.h>

/** \brief Simple Image Processor
 *
 * Simple Image Processor with opencv calls
 *
 */
class RosImgProcessorNode
{
    protected:
        //ros node handle
        ros::NodeHandle nh_;

        //image transport
        image_transport::ImageTransport img_tp_;

        // subscribers to the image and camera info topics
        image_transport::Subscriber image_subs_;
        ros::Subscriber camera_info_subs_;

        //publishers
        image_transport::Publisher image_pub_;
		    ros::Publisher marker_publisher_;

        //pointer to received (in) and published (out) images
        cv_bridge::CvImagePtr cv_img_ptr_in_;
        cv_bridge::CvImage cv_img_out_;

		//Camera matrix
		Eigen::Matrix3d matrixK_;
		Eigen::Vector3d direction_;

        //image encoding label
        std::string img_encoding_;

        //wished process rate, [hz]
        double rate_;

    protected:
        // callbacks
        void imageCallback(const sensor_msgs::ImageConstPtr& _msg);
        void cameraInfoCallback(const sensor_msgs::CameraInfo & _msg);

    public:
        // Constructor
        RosImgProcessorNode();

        // Destructor
        ~RosImgProcessorNode();

        // Process input image
        void process();

        // Publish output image
        void publishImage();

		// Publish direction marker
        void publishMarker();

 		// Returns rate_
        double getRate() const;
};
#endif
