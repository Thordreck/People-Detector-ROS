//standards
#include <iostream>
#include <float.h>

//People detector
#include "LaserDetector.h"
#include "LegsDetector.h"

//ROS
#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <laser_geometry/laser_geometry.h>

using namespace std;


LaserDetector::LaserDetector(string hypotheses_filename, int num_hypotheses /*= 100*/, double threshold /*= 0.15*/, int list_size /*= 500*/) 
 : MAX_LASER_DIST_(5), MAX_LEGS_DIST_(0.65), threshold_(threshold), list_size_(list_size)
{
	//Open hypotheses file
	f_hypotheses_ = fopen(hypotheses_filename.c_str(),"r");
	if (f_hypotheses_ == NULL)
		ROS_ERROR("ERROR opening hypothesis file %s", hypotheses_filename.c_str());
	ld_.load(f_hypotheses_, num_hypotheses);
	return;
}

LaserDetector::~LaserDetector()
{
    	fclose( f_hypotheses_ );
	return;
}
  
  /**
   * \brief getting range messages from your system and classify them
   * \param msg input laser scan
   */
void LaserDetector::scan_message(vector<tf::Point>& legs_points, const sensor_msgs::LaserScan::ConstPtr &msg)
{

	sensor_msgs::PointCloud cloud;
  	projector_.projectLaser(*msg, cloud);

	// This is a list of segments.
   	// For more information have a look at ../common/dynamictable.h/hxx
    	dyntab_segments *list_segments=NULL;
    	int num_readings = msg->ranges.size();

    	double *angles = new double[num_readings]; // array of angles
    	double *ranges = new double[num_readings]; // array of measurements
    
     	for (unsigned int i = 0; i < num_readings; i++)
    	{
      		ranges[i] = msg->ranges[i];
      		angles[i] = msg->angle_min + i * msg->angle_increment;
    	}

    	list_segments = new dyntab_segments (list_size_);
    	// segment the scan
    	ld_.segmentScan(threshold_, num_readings, angles, ranges, list_segments);
		
    	// Classiy segments
    	for (int i=0; i < list_segments->num(); i++) 
    	{
      		Segment *s = list_segments->getElement(i);

      		// discard segments with less than three points
      		if ( s->num() < 3 ) 
        		s->type = -1;
      		else 
        		ld_.classify(s); 
	}

	tf::Point legs_point;
	//Associate segments corresponding to the same person
    	for (int i=0; i < list_segments->num(); i++) 
    	{
		Segment *s = list_segments->getElement(i);
		Segment *next_s = NULL;

      		if (s->type == 1 && s->beams->getFirst()->range <= MAX_LASER_DIST_)
		{
			for(int j=i+1; j < list_segments->num(); j++)
			{
				next_s = list_segments->getElement(j);
				if (next_s->type == 1)
					break;
			}
			bool combined = combineSegments(s, next_s, cloud,legs_point);
			if(!next_s || next_s->type != 1)
				break;
			i = (combined) ? next_s->getElement(0)->segment_id : next_s->getElement(0)->segment_id -1;
		}
		legs_points.push_back(legs_point);
	}

    	// delete the list of segments 
    	list_segments->setAutoDelete(true);
    	delete list_segments;
    	list_segments = NULL;

    	// free memory
    	delete [] ranges;
    	delete [] angles;

    	return;
}

//Combines segments if they belong to the same person.
//Saves the result on tf::Point. Returns true if segments were actually combined
bool LaserDetector::combineSegments(Segment *s, Segment *next_s, sensor_msgs::PointCloud &cloud,tf::Point &legs_point)
{
	bool combined = false;

	int left_leg_index = s->beams->getLast()->position;
	tf::Point left_leg(cloud.points[left_leg_index].x, cloud.points[left_leg_index].y,cloud.points[left_leg_index].z);

	if(next_s && next_s->type == 1)
	{	
		int right_leg_index = s->beams->getFirst()->position;
		tf::Point right_leg(cloud.points[right_leg_index].x, cloud.points[right_leg_index].y,cloud.points[right_leg_index].z);

		if(left_leg.distance(right_leg) <= MAX_LEGS_DIST_)
		{
			combined = true;
			left_leg += right_leg;
			left_leg /=2;
		}
	}
	legs_point = left_leg;
	return combined;
}
//end LaserDetector


