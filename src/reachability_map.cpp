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

#include <kdl/frames.hpp>
#include "Eigen/Dense"

#include "planer_utils/reachability_map.h"
#include "planer_utils/random_uniform.h"

    ReachabilityMap::ReachabilityMap(double voxel_size, int dim) :
        voxel_size_(voxel_size),
        dim_(dim),
        ep_min_(dim),
        ep_max_(dim)
    {
        if (dim_ == 2) {
            for (int y = -1; y <= 1; y++ ) {
                for (int x = -1; x <= 1; x++ ) {
                    if (x == 0 && y == 0) {
                        continue;
                    }
                    std::vector<int > vec;
                    vec.push_back(x);
                    vec.push_back(y);
                    neighbours_.push_back(vec);
                }
            }
        }
        else if (dim_ == 3) {
            for (int z = -1; z <= 1; z++ ) {
                for (int y = -1; y <= 1; y++ ) {
                    for (int x = -1; x <= 1; x++ ) {
                        if (x == 0 && y == 0 && z == 0) {
                            continue;
                        }
                        std::vector<int > vec;
                        vec.push_back(x);
                        vec.push_back(y);
                        vec.push_back(z);
                        neighbours_.push_back(vec);
                    }
                }
            }
        }
        else {
            std::cout << "ERROR: ReachabilityMap: dim should be 2 or 3" << std::endl;
        }
    }

    ReachabilityMap::~ReachabilityMap() {
    }

    void ReachabilityMap::generate(const boost::shared_ptr<KinematicModel> &kin_model, const boost::shared_ptr<self_collision::CollisionModel> &col_model, const std::string &effector_name, int ndof, const Eigen::VectorXd &lower_limit, const Eigen::VectorXd &upper_limit) {
        std::list<Eigen::VectorXd > ep_B_list;
        for (int dim_idx = 0; dim_idx < dim_; dim_idx++) {
            ep_min_(dim_idx) = 1000000.0;
            ep_max_(dim_idx) = -1000000.0;
        }

        std::set<int> excluded_link_idx;
        excluded_link_idx.insert(col_model->getLinkIndex("env_link"));

        int effector_idx = col_model->getLinkIndex(effector_name);
        std::vector<KDL::Frame > links_fk(col_model->getLinksCount());

        for (int i = 0; i < 100000; i++) {
            Eigen::VectorXd tmp_q(ndof);
            for (int q_idx = 0; q_idx < ndof; q_idx++) {
                tmp_q(q_idx) = randomUniform(lower_limit(q_idx), upper_limit(q_idx));
            }

            // calculate forward kinematics for all links
            for (int l_idx = 0; l_idx < col_model->getLinksCount(); l_idx++) {
                kin_model->calculateFk(links_fk[l_idx], col_model->getLinkName(l_idx), tmp_q);
            }

            if (self_collision::checkCollision(col_model, links_fk, excluded_link_idx)) {
                continue;
            }

            const KDL::Frame &T_B_E = links_fk[effector_idx];
            Eigen::VectorXd x(dim_);
            for (int dim_idx = 0; dim_idx < dim_; dim_idx++) {
                x(dim_idx) = T_B_E.p[dim_idx];
                if (ep_min_(dim_idx) > x(dim_idx)) {
                    ep_min_(dim_idx) = x(dim_idx);
                }
                if (ep_max_(dim_idx) < x(dim_idx)) {
                    ep_max_(dim_idx) = x(dim_idx);
                }
            }
            ep_B_list.push_back(x);
        }

        steps_.clear();
        int map_size = 1;
        for (int dim_idx = 0; dim_idx < dim_; dim_idx++) {
            int steps = static_cast<int>( ceil( ( ep_max_(dim_idx) - ep_min_(dim_idx) ) / voxel_size_ ) );
            steps_.push_back( steps );
            map_size *= steps;
        }

        r_map_.resize(map_size, 0);
        p_map_.resize(map_size, 0);

        max_value_ = 0;
        for (std::list<Eigen::VectorXd >::const_iterator it = ep_B_list.begin(); it != ep_B_list.end(); it++) {
            int idx = getIndex( (*it) );
            if (idx < 0) {
                std::cout << "ERROR: ReachabilityMap::generate: idx < 0" << std::endl;
            }
            r_map_[idx]++;
            if (r_map_[idx] > max_value_) {
                max_value_ = r_map_[idx];
            }
        }
    }

    void ReachabilityMap::generate(const Eigen::VectorXd &lower_bound, const Eigen::VectorXd &upper_bound) {
        ep_min_ = lower_bound;
        ep_max_ = upper_bound;

        steps_.clear();
        int map_size = 1;
        for (int dim_idx = 0; dim_idx < dim_; dim_idx++) {
            int steps = static_cast<int>( ceil( ( ep_max_(dim_idx) - ep_min_(dim_idx) ) / voxel_size_ ) );
            steps_.push_back( steps );
            map_size *= steps;
        }

        r_map_.resize(map_size, 0);
        p_map_.resize(map_size, 0);
        max_value_ = 0;
    }



    double ReachabilityMap::getValue(const Eigen::VectorXd &x) const {
        int idx = getIndex(x);
        if (idx < 0) {
            return 0;
        }
        // TODO: check what happens if the score is below 0
        return static_cast<double >(r_map_[idx] - p_map_[idx]) / static_cast<double >(max_value_);
    }

    void ReachabilityMap::setValue(const Eigen::VectorXd &x, int value) {
        int idx = getIndex(x);
        if (idx < 0) {
            return;
        }
        r_map_[idx] = value;
        if (r_map_[idx] > max_value_) {
            max_value_ = r_map_[idx];
        }
    }

    void ReachabilityMap::clear() {
        for (int idx = 0; idx < r_map_.size(); idx++) {
            r_map_[idx] = 0;
        }
        max_value_ = 0;
    }

    void ReachabilityMap::getNeighbourIndices(const std::vector<int> &d, std::list<int> &n_indices) {
        n_indices.clear();
        for (int dim_idx = 0; dim_idx < dim_; dim_idx++) {
            if (d[dim_idx] > 0) {
                int total_idx = 0;
                for (int dim_idx2 = 0; dim_idx2 < dim_; dim_idx2++) {
                    if (dim_idx == dim_idx2) {
                        total_idx = total_idx * steps_[dim_idx2] + d[dim_idx2] - 1;
                    }
                    else {
                        total_idx = total_idx * steps_[dim_idx2] + d[dim_idx2];
                    }
                }
                n_indices.push_back(total_idx);
            }
            if (d[dim_idx] < steps_[dim_idx]-1) {
                int total_idx = 0;
                for (int dim_idx2 = 0; dim_idx2 < dim_; dim_idx2++) {
                    if (dim_idx == dim_idx2) {
                        total_idx = total_idx * steps_[dim_idx2] + d[dim_idx2] + 1;
                    }
                    else {
                        total_idx = total_idx * steps_[dim_idx2] + d[dim_idx2];
                    }
                }
                n_indices.push_back(total_idx);
            }
            
        }

/*        if (d0 > 0) {
            n_indices.push_back(((d0-1) * steps_[1] + d1) * steps_[2] + d2);
        }
        if (d0 < steps_[0]-1) {
            n_indices.push_back(((d0+1) * steps_[1] + d1) * steps_[2] + d2);
        }
*/
    }

    void ReachabilityMap::grow() {
        std::vector<int > map_copy(r_map_);
        for (int idx = 0; idx < map_copy.size(); idx++) {
            if (map_copy[idx] > 1) {
                map_copy[idx] = 1;
            }
        }

        std::vector<int > d(dim_);

        if (dim_ == 2) {
            for (int d0=0; d0<steps_[0]; d0++) {
                for (int d1=0; d1<steps_[1]; d1++) {
                    int idx = d0 * steps_[1] + d1;
                    if (map_copy[idx] == 1) {
                        d[0] = d0;
                        d[1] = d1;
                        std::list<int > n_indices;
                        getNeighbourIndices(d, n_indices);
                        for (std::list<int >::const_iterator it = n_indices.begin(); it != n_indices.end(); it++) {
                            if (map_copy[(*it)] == 0) {
                                map_copy[(*it)] = 2;
                            }
                        }
                    }
                }
            }
        }
        else if (dim_ == 3) {
            for (int d0=0; d0<steps_[0]; d0++) {
                for (int d1=0; d1<steps_[1]; d1++) {
                    for (int d2=0; d2<steps_[2]; d2++) {
                        int idx = (d0 * steps_[1] + d1) * steps_[2] + d2;
                        if (map_copy[idx] == 1) {
                            d[0] = d0;
                            d[1] = d1;
                            d[2] = d2;
                            std::list<int > n_indices;
                            getNeighbourIndices(d, n_indices);
                            for (std::list<int >::const_iterator it = n_indices.begin(); it != n_indices.end(); it++) {
                                if (map_copy[(*it)] == 0) {
                                    map_copy[(*it)] = 2;
                                }
                            }
                        }
                    }
                }
            }
        }
        else {
            std::cout << "ReachabilityMap::grow not implemented for " << dim_ << " dimensions" << std::endl;
            return;
        }

        for (int idx = 0; idx < map_copy.size(); idx++) {
            if (map_copy[idx] > 1) {
                map_copy[idx] = 1;
            }
            r_map_[idx] += map_copy[idx];
            if (r_map_[idx] > max_value_) {
                max_value_ = r_map_[idx];
            }
        }
    }

    void ReachabilityMap::addMap(const ReachabilityMap &map) {
        for (int idx = 0; idx < r_map_.size(); idx++) {
            r_map_[idx] += map.r_map_[idx];
            if (r_map_[idx] > max_value_) {
                max_value_ = r_map_[idx];
            }
        }
    }

    void ReachabilityMap::addMap(const boost::shared_ptr<ReachabilityMap > &pmap) {
        addMap( (*(pmap.get())) );
    }

    double ReachabilityMap::getMaxValue() const {
        return static_cast<double >(max_value_);
    }

    void ReachabilityMap::addPenalty(const Eigen::VectorXd &x) {
        int idx = getIndex(x);
        if (idx >= 0) {
            p_map_[idx] += max_value_;
        }
    }

    void ReachabilityMap::resetPenalty() {
        for (int idx = 0; idx < p_map_.size(); idx++) {
            p_map_[idx] = 0;
        }
    }

    int ReachabilityMap::getIndex(const Eigen::VectorXd &x) const {
        int total_idx = 0;
        for (int dim_idx = 0; dim_idx < dim_; dim_idx++) {
            int idx = static_cast<int >( floor( (x(dim_idx) - ep_min_(dim_idx)) / voxel_size_ ) );
            if (idx < 0 || idx >= steps_[dim_idx]) {
                return -1;
            }
            total_idx = total_idx * steps_[dim_idx] + idx;
        }
        return total_idx;
    }

