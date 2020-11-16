/* Apache License, Version 2.0 */

/**
 * This file contains default values for several items like
 * vertex coordinates, export parameters, MTL values etc.
 */

#pragma once

#include <array>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "IO_wavefront_obj.h"

namespace blender::io::obj {

using array_float_3 = std::array<float, 3>;

/**
 * This matches #OBJCurve's member functions, except that all the numbers and names are known
 * constants. Used to store expected values of NURBS Curve sobjects.
 */
class NurbsObject {
 private:
  std::string nurbs_name_;
  std::vector<std::vector<array_float_3>> coordinates_;

 public:
  NurbsObject(const std::string nurbs_name,
              const std::vector<std::vector<array_float_3>> coordinates)
      : nurbs_name_(nurbs_name), coordinates_(coordinates)
  {
  }

  int total_splines() const
  {
    return coordinates_.size();
  }

  int total_spline_vertices(const int spline_index) const
  {
    if (spline_index >= coordinates_.size()) {
      ADD_FAILURE();
      return 0;
    }
    return coordinates_[spline_index].size();
  }

  const float *vertex_coordinates(const int spline_index, const int vertex_index) const
  {
    return coordinates_[spline_index][vertex_index].data();
  }
};

struct OBJExportParamsDefault {
  OBJExportParams params;
  OBJExportParamsDefault()
  {
    params.filepath[0] = '\0';
    params.export_animation = false;
    params.start_frame = 0;
    params.end_frame = 1;

    params.forward_axis = OBJ_AXIS_NEGATIVE_Z_FORWARD;
    params.up_axis = OBJ_AXIS_Y_UP;
    params.scaling_factor = 1.f;

    params.export_eval_mode = DAG_EVAL_VIEWPORT;
    params.export_selected_objects = false;
    params.export_uv = true;
    params.export_normals = true;
    params.export_materials = true;
    params.export_triangulated_mesh = false;
    params.export_curves_as_nurbs = false;

    params.export_object_groups = false;
    params.export_material_groups = false;
    params.export_vertex_groups = false;
    params.export_smooth_groups = true;
    params.smooth_groups_bitflags = false;
  }
};

const std::vector<std::vector<array_float_3>> coordinates_NurbsCurve{
    {{9.947419, 0.000000, 0.000000},
     {9.447419, 0.000000, 1.000000},
     {7.447419, 0.000000, 1.000000},
     {6.947419, 0.000000, 0.000000}}};
const std::vector<std::vector<array_float_3>> coordinates_NurbsCircle{
    {{11.463165, 0.000000, -1.000000},
     {12.463165, 0.000000, -1.000000},
     {12.463165, 0.000000, 0.000000},
     {12.463165, 0.000000, 1.000000},
     {11.463165, 0.000000, 1.000000},
     {10.463165, 0.000000, 1.000000},
     {10.463165, 0.000000, 0.000000},
     {10.463165, 0.000000, -1.000000}}};
const std::vector<std::vector<array_float_3>> coordinates_NurbsPath{
    {{17.690557, 0.000000, 0.000000},
     {16.690557, 0.000000, 0.000000},
     {15.690557, 0.000000, 0.000000},
     {14.690557, 0.000000, 0.000000},
     {13.690557, 0.000000, 0.000000}},
    {{17.188307, 0.000000, 0.000000},
     {16.688307, 0.000000, 1.000000},
     {14.688307, 0.000000, 1.000000},
     {14.188307, 0.000000, 0.000000}}};

const std::map<std::string, std::unique_ptr<NurbsObject>> all_nurbs_truth = []() {
  std::map<std::string, std::unique_ptr<NurbsObject>> all_nurbs;
  all_nurbs.emplace("NurbsCurve",
                    std::make_unique<NurbsObject>("NurbsCurve", coordinates_NurbsCurve));
  all_nurbs.emplace("NurbsCircle",
                    std::make_unique<NurbsObject>("NurbsCircle", coordinates_NurbsCircle));
  all_nurbs.emplace("NurbsPath",
                    std::make_unique<NurbsObject>("NurbsPath", coordinates_NurbsPath));
  return all_nurbs;
}();
}  // namespace blender::io::obj