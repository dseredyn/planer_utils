// Copyright (c) 2015, Robot Control and Pattern Recognition Group,
// Institute of Control and Computation Engineering
// Warsaw University of Technology
//
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Warsaw University of Technology nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL <COPYright HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Dawid Seredynski
//

#include "planer_utils/utilities.h"
#include "planer_utils/random_uniform.h"
#include <boost/math/special_functions/bessel.hpp>
#include <sstream>
#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>

static const double PI(3.141592653589793);

    void publishJointState(ros::Publisher &joint_state_pub, const Eigen::VectorXd &q, const std::vector<std::string > &joint_names) {
        sensor_msgs::JointState js;
        js.header.stamp = ros::Time::now();
        int q_idx = 0;
        for (std::vector<std::string >::const_iterator it = joint_names.begin(); it != joint_names.end(); it++, q_idx++) {
            js.name.push_back(*it);
            js.position.push_back(q[q_idx]);
        }
        joint_state_pub.publish(js);
    }

    void publishJointState(ros::Publisher &joint_state_pub, const Eigen::VectorXd &q, const std::vector<std::string > &joint_names, const Eigen::VectorXd &ign_q, const std::vector<std::string > &ign_joint_names) {
        sensor_msgs::JointState js;
        js.header.stamp = ros::Time::now();
        int q_idx = 0;
        for (std::vector<std::string >::const_iterator it = joint_names.begin(); it != joint_names.end(); it++, q_idx++) {
            js.name.push_back(*it);
            js.position.push_back(q[q_idx]);
        }
        q_idx = 0;
        for (std::vector<std::string >::const_iterator it = ign_joint_names.begin(); it != ign_joint_names.end(); it++, q_idx++) {
            js.name.push_back(*it);
            js.position.push_back(ign_q[q_idx]);
        }
        joint_state_pub.publish(js);
    }


    void publishTransform(tf::TransformBroadcaster &br, const KDL::Frame &T_B_F, const std::string &frame_id, const std::string &base_frame_id) {
        tf::Transform transform;
        transform.setOrigin( tf::Vector3(T_B_F.p.x(), T_B_F.p.y(), T_B_F.p.z()) );
        tf::Quaternion q;
        double qx, qy, qz, qw;
        T_B_F.M.GetQuaternion(q[0], q[1], q[2], q[3]);
        transform.setRotation(q);
        br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), base_frame_id, frame_id));
    }

    int addRobotModelVis(MarkerPublisher &markers_pub, int m_id, const boost::shared_ptr<self_collision::CollisionModel> &col_model, const std::vector<KDL::Frame > &T) {
        for (self_collision::CollisionModel::VecPtrLink::const_iterator l_it = col_model->getLinks().begin(); l_it != col_model->getLinks().end(); l_it++) {
            KDL::Frame T_B_L = T[(*l_it)->index_];
            for (self_collision::Link::VecPtrCollision::const_iterator it = (*l_it)->collision_array.begin(); it != (*l_it)->collision_array.end(); it++) {
                KDL::Frame T_B_O = T_B_L * (*it)->origin;
                double cr, cg, cb, ca;
                (*it)->geometry->getColor(cr, cg, cb, ca);
                if ((*it)->geometry->getType() == self_collision::Geometry::CONVEX) {
                    self_collision::Convex *convex = static_cast<self_collision::Convex* >((*it)->geometry.get());

                    if (convex->visualisation_hint_ == "lines") {
                        const std::vector<KDL::Vector >& points = convex->getPoints();
                        const std::vector<int >& polygons = convex->getPolygons();
                        std::vector<KDL::Vector > pts;

	                    int poly_idx = 0;
	                    while (poly_idx < polygons.size())
	                    {
		                    int points_in_poly = polygons[poly_idx];
		                    for (int j=0; j<points_in_poly; j++)
		                    {
                                int idx1 = poly_idx + 1 + j;
                                int idx2 = poly_idx + 1 + (j+1)%points_in_poly;
			                    pts.push_back( points[polygons[idx1]] );
                                pts.push_back( points[polygons[idx2]] );
		                    }
		                    poly_idx += points_in_poly+1;
	                    }
                        m_id = markers_pub.addLineListMarker(m_id, pts, T_B_O, cr, cg, cb, ca, 0.02, "world");
                    }
                    else if (convex->visualisation_hint_.find("box") == 0) {
                        double dx, dy, dz;
                        std::istringstream(convex->visualisation_hint_.substr(4)) >> dx >> dy >> dz;
                        m_id = markers_pub.addSinglePointMarkerCube(m_id, T_B_O.p, cr, cg, cb, ca, dx, dy, dz, "world");
                    }
                }
                else if ((*it)->geometry->getType() == self_collision::Geometry::SPHERE) {
                    self_collision::Sphere *sphere = static_cast<self_collision::Sphere* >((*it)->geometry.get());
                    m_id = markers_pub.addSinglePointMarker(m_id, T_B_O.p, cr, cg, cb, ca, sphere->getRadius()*2, "world");
                }
                else if ((*it)->geometry->getType() == self_collision::Geometry::CAPSULE) {
                    self_collision::Capsule *capsule = static_cast<self_collision::Capsule* >((*it)->geometry.get());
                    m_id = markers_pub.addCapsule(m_id, T_B_O, cr, cg, cb, ca, capsule->getLength(), capsule->getRadius(), "world");
                }
            }
        }
        return m_id;
    }

    void getPointOnPath(const std::list<Eigen::VectorXd > &path, double f, Eigen::VectorXd &x) {

        if (path.size() == 0) {
            std::cout << "ERROR: getPointOnPath: path size is 0" << std::endl;
            return;
        }
        else if (path.size() == 1 || f < 0.0) {
            x = (*path.begin());
            return;
        }

        if (f > 1.0) {
            x = (*(--path.end()));
            return;
        }

        double length = 0.0;
        for (std::list<Eigen::VectorXd >::const_iterator it1 = path.begin(), it2=++path.begin(); it2 != path.end(); it1++, it2++) {
            double dist = ((*it1) - (*it2)).norm();
            length += dist;
        }

        double pos = length * f;

        for (std::list<Eigen::VectorXd >::const_iterator it1 = path.begin(), it2=++path.begin(); it2 != path.end(); it1++, it2++) {
            Eigen::VectorXd v = ((*it2) - (*it1));
            double dist = v.norm();
            if (pos - dist > 0) {
                pos -= dist;
            }
            else {
                x = (*it1) + pos * v / dist;
                return;
            }
        }
        x = (*(--path.end()));
    }

    double getPathLength(const std::list<Eigen::VectorXd > &path) {

        if (path.size() < 2) {
            return 0.0;
        }

        double length = 0.0;
        for (std::list<Eigen::VectorXd >::const_iterator it1 = path.begin(), it2=++path.begin(); it2 != path.end(); it1++, it2++) {
            double dist = ((*it1) - (*it2)).norm();
            length += dist;
        }

        return length;
    }

void printFrameKDL(const KDL::Frame &f) {
    double qx, qy, qz, qw, px(f.p.x()), py(f.p.y()), pz(f.p.z());
    f.M.GetQuaternion(qx, qy, qz, qw);
    std::cout << "KDL::Frame(KDL::Rotation::Quaternion(" << qx << ", " << qy << ", " << qz << ", " << qw << "), KDL::Vector(" << px << ", " << py << ", " << pz << "))" << std::endl;
}

double getAngle(const KDL::Vector &v1, const KDL::Vector &v2) {
    return std::atan2((v1*v2).Norm(), KDL::dot(v1,v2));
}

void EigenTfToKDL(const Eigen::Isometry3d &tf, KDL::Frame &kdlT) {
    kdlT = KDL::Frame( KDL::Rotation(tf(0,0),tf(0,1),tf(0,2), tf(1,0), tf(1,1), tf(1,2), tf(2,0), tf(2,1), tf(2,2)), KDL::Vector(tf(0,3), tf(1,3), tf(2,3)) );
}

void KDLToEigenTf(const KDL::Frame &kdlT, Eigen::Isometry3d &tf) {
    double qx, qy, qz, qw;
    kdlT.M.GetQuaternion(qx, qy, qz, qw);
    tf.fromPositionOrientationScale(Eigen::Vector3d(kdlT.p.x(), kdlT.p.y(), kdlT.p.z()), Eigen::Quaterniond(qw, qx, qy, qz), Eigen::Vector3d(1.0, 1.0, 1.0));
}

std::ostream& operator<< (std::ostream& stream, const KDL::Frame& f) {
    double qx, qy, qz, qw;
    f.M.GetQuaternion(qx, qy, qz, qw);
    stream << f.p.x() << " " << f.p.y() << " " << f.p.z() << " " << qx << " " << qy << " " << qz << " " << qw;
    return stream;
}

std::istream& operator>> (std::istream& stream, KDL::Frame& f) {
    double x, y, z, qx, qy, qz, qw;
    stream >> x >> y >> z >> qx >> qy >> qz >> qw;
    f = KDL::Frame(KDL::Rotation::Quaternion(qx, qy, qz, qw), KDL::Vector(x, y, z));
    return stream;
}

std::string frameKdl2string(const KDL::Frame &f) {
    std::ostringstream strs;
    strs << f;
    return strs.str();
}

KDL::Frame string2frameKdl(const std::string &str) {
    std::istringstream strs(str);
    KDL::Frame ret;
    strs >> ret;
    return ret;
}

std::string double2string(double d) {
    std::ostringstream strs;
    strs << d;
    return strs.str();
}

double string2double(const std::string &str) {
    std::istringstream strs(str);
    double ret;
    strs >> ret;
    return ret;
}

int string2int(const std::string &str) {
    std::istringstream strs(str);
    int ret;
    strs >> ret;
    return ret;
}

/******************************************************************************************************************************************************************/

double triVariateIsotropicGaussianKernel(const Eigen::Vector3d &x, const Eigen::Vector3d &mean, double sigma) {
    static const double sqrt_2pi_pow_3 = std::sqrt(std::pow(2.0 * PI, 3.0));
    return std::exp( - ((x-mean).squaredNorm()) / (2.0 * sigma * sigma) ) / (sqrt_2pi_pow_3 * sigma);
}

double biVariateIsotropicGaussianKernel(const Eigen::Vector2d &x, const Eigen::Vector2d &mean, double sigma) {
    static const double sqrt_2pi_pow_2 = std::sqrt(std::pow(2.0 * PI, 2.0));
    return std::exp( - ((x-mean).squaredNorm()) / (2.0 * sigma * sigma) ) / (sqrt_2pi_pow_2 * sigma);
}

double uniVariateIsotropicGaussianKernel(double x, double mean, double sigma) {
    static const double sqrt_2pi = std::sqrt(2.0 * PI);
    return std::exp( - ((x-mean)*(x-mean)) / (2.0 * sigma * sigma) ) / (sqrt_2pi * sigma);
}

double misesFisherKernelConstant(double sigma, int dimensions) {
    double kappa = sigma;
    double p_div_2 = static_cast<double >(dimensions) / 2.0;
    double Cp = std::pow(kappa, p_div_2 - 1.0) / (std::pow(2.0 * PI, p_div_2) * boost::math::cyl_bessel_i(p_div_2 - 1.0, kappa));
    return Cp;
}

double misesFisherKernel(const Eigen::Vector3d &q, const Eigen::Vector3d &mean, double sigma, double Cp) {
    double dot_prod = q.dot(mean);
    return Cp * (std::exp(sigma * dot_prod) + std::exp(-sigma * dot_prod)) / 2.0;
}

int vonMisesFisherSample(const Eigen::Vector3d &mean, double pdf_mean, double sigma, double Cp, Eigen::Vector3d &x) {
    for (int i = 0; i < 100000; i++) {
        randomUnitSphere(x);
        double pdf = misesFisherKernel(x, mean, sigma, Cp);
        double rand_pdf = randomUniform(0.0, pdf_mean);
        if (rand_pdf < pdf) {
            return i;
        }
    }
    return -1;
}

double misesFisherKernel(const Eigen::Vector4d &q, const Eigen::Vector4d &mean, double sigma, double Cp) {
    double dot_prod = q.dot(mean);
    return Cp * (std::exp(sigma * dot_prod) + std::exp(-sigma * dot_prod)) / 2.0;
}

int vonMisesFisherSample(const Eigen::Vector4d &mean, double pdf_mean, double sigma, double Cp, Eigen::Vector4d &x) {
    for (int i = 0; i < 100000; i++) {
        randomUnitQuaternion(x);
        double pdf = misesFisherKernel(x, mean, sigma, Cp);
        double rand_pdf = randomUniform(0.0, pdf_mean);
        if (rand_pdf < pdf) {
            return i;
        }
    }
    return -1;
}

double orientationNormalKernel(const Eigen::Vector4d &q, const Eigen::Vector4d &mean, double sigma) {
    KDL::Rotation r_q(KDL::Rotation::Quaternion(q(0), q(1), q(2), q(3)));
    KDL::Rotation r_mean(KDL::Rotation::Quaternion(mean(0), mean(1), mean(2), mean(3)));

    KDL::Vector diff = KDL::diff(r_q, r_mean);
    return uniVariateIsotropicGaussianKernel( diff.Norm(), 0.0, sigma );
}

int orientationNormalSample(const Eigen::Vector4d &mean, double sigma, Eigen::Vector4d &x) {

    static boost::mt19937 rng(rand());
    boost::normal_distribution<> nd(0.0, sigma);
    boost::variate_generator<boost::mt19937&, 
                           boost::normal_distribution<> > var_nor(rng, nd);

    double angle = var_nor();

    Eigen::Vector3d vec3;
    randomUnitSphere(vec3);

    KDL::Rotation r_mean(KDL::Rotation::Quaternion(mean(0), mean(1), mean(2), mean(3)));
    KDL::Rotation r_q = KDL::addDelta(r_mean, KDL::Vector(vec3(0), vec3(1), vec3(2)), angle);

    r_q.GetQuaternion(x(0), x(1), x(2), x(3));

    return 1;
}


