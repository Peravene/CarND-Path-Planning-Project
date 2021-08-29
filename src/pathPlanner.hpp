#include "helpers.h"
#include "spline.h"

// start in lane 1
const auto startLaneIndex = ego;

// reference velocity to target [miles per hour]
const double targetVelocity = 49.5;

void straight(points &nextPoints, const egoVehicle &car) {
  double dist_inc = 0.5;
  for (int i = 0; i < 50; i++) {
    nextPoints.x.push_back(car.xy.x + (dist_inc * i) * cos(deg2rad(car.yaw)));
    nextPoints.y.push_back(car.xy.y + (dist_inc * i) * sin(deg2rad(car.yaw)));
  }
}

void stayInLane(points &nextPoints, const egoVehicle &car,
                const mapWaypoints &map) {
  // car is to fast
  double dist_inc = 0.3;
  for (int i = 0; i < 50; i++) {
    double next_s = car.sd.s + (i + 1) * dist_inc;
    // middle of middle lane. (lane_width=4m)
    double next_d = 6;

    auto xy = getXY(next_s, next_d, map.s, map.x, map.y);

    nextPoints.x.push_back(xy.x);
    nextPoints.y.push_back(xy.y);
  }
}

void stayInLaneWithSpline(points &nextPoints, const egoVehicle &car,
                          const mapWaypoints &map) {
  // anchor points
  path anchorPoints;
  // assert(car.previous_path_x.size() != car.previous_path_y.size());
  int prev_size = car.previous_path_x.size();

  // reference x,y,yaw state
  // either we will reference the starting point as where the car is or at the
  // previous paths end point
  pointXY reference;
  reference = car.xy;
  double ref_yaw = deg2rad(car.yaw);

  // if previous size is almost empty, use the car as starting reference
  pointXY previousPoint;
  if (prev_size < 2) {
    // use two points that make the path tangent to the car
    previousPoint = calcPreviousPoint(reference, car.yaw);
  }
  // use the previous path's end point end point as starting reference
  else {
    // Redefine reference state as previous path end point
    reference.x = car.previous_path_x[prev_size - 1];
    reference.y = car.previous_path_y[prev_size - 1];

    previousPoint.x = car.previous_path_x[prev_size - 2];
    previousPoint.y = car.previous_path_y[prev_size - 2];
    ref_yaw = calcYaw(reference, previousPoint);
  }

  // Use two points that make the path tangent to the previous path's end
  // point
  anchorPoints.xy.push_back(previousPoint);
  anchorPoints.xy.push_back(reference);

  // In frenet add evenly 30m spaced  points ahead of the starting reference
  auto offsetLat = getLaneDisplacement(startLaneIndex);
  double startS = 30;
  double stepS = 30;
  double endS = 90;
  for (int offsetLong = startS; offsetLong <= endS; offsetLong += stepS) {
    auto next_wp = getXY(car.sd.s + offsetLong, offsetLat, map.s, map.x, map.y);
    anchorPoints.xy.push_back(next_wp);
  }

  vector<double> x;
  vector<double> y;
  for (int i = 0; i < anchorPoints.xy.size(); i++) {
    // shift car reference angle to 0 degrees
    auto shift = calcTranslation(anchorPoints.xy[i], reference, true);
    anchorPoints.xy[i] = calcRotation(shift, 0 - ref_yaw);
    x.push_back(anchorPoints.xy[i].x);
    y.push_back(anchorPoints.xy[i].y);
  }

  // create a spline
  tk::spline s;
  // set (x,y) points to the spline
  s.set_points(x, y);

  // define the actual (x,y) points we will use for the planer
  // start with all of the previous path points from last time
  for (int i = 0; i < prev_size; i++) {
    nextPoints.x.push_back(car.previous_path_x[i]);
    nextPoints.y.push_back(car.previous_path_y[i]);
  }

  // calculate how to break up spline points so that we travel at our desired
  // reference velocity
  const double target_x = 30.0;
  double target_y = s(target_x);
  double target_dist = distance(0, 0, target_x, target_y);
  double N = target_dist / getTravelledDistance(targetVelocity);
  double steps = target_x / N;

  // Fill up the rest of our path planner after filling it with previous points,
  // here we will always output 50 points
  const int numberOfOutputPoints = 50;
  double x_add_on = 0;
  for (int i = 1; i <= numberOfOutputPoints - prev_size; i++) {
    x_add_on += steps;

    pointXY newPoint;
    newPoint.x = x_add_on;
    newPoint.y = s(newPoint.x);

    // rotate back to normal after rotating it earlier
    auto newPointTrans = calcRotation(newPoint, ref_yaw);
    newPointTrans = calcTranslation(newPointTrans, reference);

    nextPoints.x.push_back(newPointTrans.x);
    nextPoints.y.push_back(newPointTrans.y);
  }
}