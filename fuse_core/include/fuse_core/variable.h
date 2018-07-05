/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2018, Locus Robotics
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef FUSE_CORE_VARIABLE_H
#define FUSE_CORE_VARIABLE_H

#include <fuse_core/uuid.h>
#include <fuse_core/macros.h>

#include <boost/core/demangle.hpp>
#include <ceres/local_parameterization.h>

#include <ostream>
#include <string>
#include <typeinfo>


namespace fuse_core
{

/**
 * @brief The Variable interface definition.
 *
 * A Variable defines some semantically meaningful group of one or more individual scale values. Each variable is
 * treated as a block by the optimization engine, as the values of all of its dimensions are likely to be involved
 * in the same constraints. Some common examples of variable groupings are a 2D point (x, y), 3D point (x, y, z), or
 * camera calibration parameters (fx, fy, cx, cy).
 *
 * To support the Ceres optimization engine, the Variable must hold the scalar values of each dimension in a
 * _contiguous_ memory space, and must provide access to that memory location via the Variable::data() methods.
 *
 * Some Variables may require special update rules, either because they are over-parameterized, as is the case with
 * 3D rotations represented as quaternions, or because the update of the individual dimensions exhibit some nonlinear
 * properties, as is the case with rotations in general (e.g. 2D rotations have a discontinuity around &pi;). To
 * support these situations, Ceres uses an optional "local parameterization". See the Ceres documentation for more
 * details. http://ceres-solver.org/nnls_modeling.html#localparameterization
 */
class Variable
{
public:
  SMART_PTR_ALIASES_ONLY(Variable);

  /**
   * @brief Constructor
   */
  Variable() = default;

  /**
   * @brief Destructor
   */
  virtual ~Variable() = default;

  /**
   * @brief Returns a unique name for this variable type.
   *
   * The variable type string must be unique for each class. As such, the fully-qualified class name is an excellent
   * choice for the type string.
   */
  virtual std::string type() const { return boost::core::demangle(typeid(*this).name()); }

  /**
   * @brief Returns a UUID for this variable.
   *
   * The implemented UUID generation should be deterministic such that a variable with the same metadata will always
   * return the same UUID. Identical UUIDs produced by sensors will be treated as the same variable by the optimizer,
   * and different UUIDs will be treated as different variables. So, two derived variables representing robot poses with
   * the same timestamp but different UUIDs will incorrectly be treated as different variables, and two robot poses with
   * different timestamps but the same UUID will be incorrectly treated as the same variable.
   *
   * One method of producing UUIDs that adhere to this requirement is to use the boost::uuid::name_generator() function.
   * The type() string can be used to generate a UUID namespace for all variables of a given derived type, and the
   * variable metadata of consequence can be converted into a carefully-formatted string or byte array and provided to
   * the generator to create the UUID for a specific variable instance.
   */
  virtual UUID uuid() const = 0;

  /**
   * @brief Returns the number of elements of this variable.
   *
   * In most cases, this will be the number of degrees of freedom this variable represents. For example, a 2D pose has
   * an x, y, and theta value, so the size will be 3. A notable exception is a 3D rotation represented as a quaternion.
   * It only has 3 degrees of freedom, but it is represented as four elements, (w, x, y, z), so it's size will be 4.
   */
  virtual size_t size() const = 0;

  /**
   * @brief Read-only access to the variable data
   *
   * The data elements must be contiguous (such as a C-style array double[3] or std::vector<double>), and it must
   * contain at least Variable::size() elements. Only Variable::size() elements will be accessed externally. This
   * interface is provided for integration with Ceres, which uses raw pointers.
   */
  virtual const double* data() const = 0;

  /**
   * @brief Read-write access to the variable data
   *
   * The data elements must be contiguous (such as a C-style array double[3] or std::vector<double>), and it must
   * contain at least Variable::size() elements. Only Variable::size() elements will be accessed externally. This
   * interface is provided for integration with Ceres, which uses raw pointers.
   */
  virtual double* data() = 0;

  /**
   * @brief Print a human-readable description of the variable to the provided stream.
   *
   * @param[out] stream The stream to write to. Defaults to stdout.
   */
  virtual void print(std::ostream& stream = std::cout) const = 0;

  /**
   * @brief Perform a deep copy of the Variable and return a unique pointer to the copy
   *
   * Unique pointers can be implicitly upgraded to shared pointers if needed.
   *
   * This can/should be implemented as follows in all derived classes:
   * @code{.cpp}
   * return Derived::make_unique(*this);
   * @endcode
   *
   * @return A unique pointer to a new instance of the most-derived Variable
   */
  virtual Variable::UniquePtr clone() const = 0;

  /**
   * @brief Create a new Ceres local parameterization object to apply to updates of this variable
   *
   * If a local parameterization is not needed, a null pointer should be returned.
   *
   * The Ceres interface requires a raw pointer. Ceres will take ownership of the pointer and promises to properly
   * delete the local parameterization when it is done. Additionally, fuse promises that the Variable object will
   * outlive any generated local parameterization (i.e. the Ceres objects will be destroyed before the Variable
   * objects). This guarantee may allow optimizations for the creation of the local parameterization objects.
   *
   * @return A base pointer to an instance of a derived LocalParameterization
   */
  virtual ceres::LocalParameterization* localParameterization() const
  {
    return nullptr;
  }
};

/**
 * Stream operator implementation used for all derived Constraint classes.
 */
std::ostream& operator <<(std::ostream& stream, const Variable& variable);

}  // namespace fuse_core

#endif  // FUSE_CORE_VARIABLE_H