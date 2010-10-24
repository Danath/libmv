// Copyright (c) 2010 libmv authors.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
#include <list>

#include "libmv/logging/logging.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/reconstruction.h"
#include "libmv/multiview/test_data_sets.h"
#include "libmv/numeric/numeric.h"
#include "libmv/multiview/random_sample.h"
#include "testing/testing.h"

namespace libmv {
namespace {

void GenerateMatchesFromNViewDataSet(const NViewDataSet &d,
                                     int noutliers,
                                     Matches *matches,
                                     std::list<Feature *> *list_features) {
  Matches::TrackID track_id;
  vector<int> wrong_matches;
  for (size_t n = 0; n < d.n; ++n) {
    //LOG(INFO) << "n -> "<< d.x[n]<< std::endl;
    // Generates wrong matches
    UniformSample(noutliers, d.X.cols(), &wrong_matches);
    //LOG(INFO) << "Features :"<<d.x[n].transpose()<<"\n";
    for (size_t p = 0; p < d.x[n].cols(); ++p) {
      PointFeature * feature = new PointFeature(d.x[n](0, p), d.x[n](1, p));
      list_features->push_back(feature);
      track_id = p;
      if (p < noutliers) {
        track_id = wrong_matches[p];
      }
      matches->Insert(n, track_id, feature);
    }
  }
}

TEST(EuclideanReconstruction, TestSynthetic6FullViews) {
  int nviews = 6;
  int npoints = 100;
  int noutliers = 0.4*npoints;// 30% of outliers
  NViewDataSet d = NRealisticCameras(nviews, npoints);
  
  // These are the expected precision of the results
  // Dont expect much since for now
  //  - this is an incremental approach
  //  - the 3D structure is not retriangulated when new views are estimated
  //  - there is no optimization!
  const double kPrecisionOrientationMatrix = 5e-2;
  const double kPrecisionPosition          = 5e-2;
  
  Reconstruction reconstruction;
  // Create the matches
  Matches matches;
  std::list<Feature *> list_features;
  GenerateMatchesFromNViewDataSet(d, noutliers, &matches, &list_features);
  
  // We fix the gauge by setting the pose of the initial camera to the true pose
  PinholeCamera * camera0 = new PinholeCamera(d.K[0], d.R[0], d.t[0]);
  reconstruction.InsertCamera(0, camera0);
            
  LOG(INFO) << "Proceed Initial Motion Estimation" << std::endl;
  // Proceed Initial Motion Estimation
  ReconstructFromTwoCalibratedViews(matches, 0, 1,
                                    d.K[0], d.K[1],
                                    &matches, &reconstruction);
  PinholeCamera * camera = NULL;
  EXPECT_EQ(reconstruction.GetNumberCameras(), 2);
  camera = dynamic_cast<PinholeCamera *>(reconstruction.GetCamera(0));
  EXPECT_TRUE(camera != NULL);  
  EXPECT_MATRIX_PROP(camera->orientation_matrix(), d.R[0], 1e-8);
  EXPECT_MATRIX_PROP(camera->position(), d.t[0], 1e-8);
  
  double rms = RootMeanSquareError(d.x[0], d.X, camera->projection_matrix());
  LOG(INFO) << "RMSE Camera 0 =" << rms << std::endl;
  
  camera = dynamic_cast<PinholeCamera *>(reconstruction.GetCamera(1));
  EXPECT_TRUE(camera != NULL);
  
  // This is a monocular reconstruction
  // We fix the scale
  Mat3 dR = d.R[0].transpose()*d.R[1];
  Vec3 dt = d.R[0].transpose() * (d.t[1] - d.t[0]);  
  double dt_norm_real = dt.norm();
  dt = camera0->orientation_matrix().transpose() * 
    (camera->position() - camera0->position());
  dt *= dt_norm_real/dt.norm();
  camera->set_position(camera0->orientation_matrix() * dt
    + camera0->position());
  
  EXPECT_MATRIX_PROP(camera->orientation_matrix(), 
                     d.R[1],
                     kPrecisionOrientationMatrix);
  EXPECT_MATRIX_PROP(camera->position(), d.t[1], kPrecisionPosition);
  rms = RootMeanSquareError(d.x[1], d.X, camera->projection_matrix());
  LOG(INFO) << "RMSE Camera 1 =" << rms << std::endl;
  
  LOG(INFO) << "Proceed Initial Intersection" << std::endl;
  // Proceed Initial Intersection 
  size_t minimum_num_views_batch = 2;
  PointStructureTriangulation(matches, 1, 
                              minimum_num_views_batch, 
                              &reconstruction); 
  
  size_t minimum_num_views_incremental = 3;  
  Mat3 R;
  Vec3 t;
  // Checks the incremental reconstruction
  for (int i = 2; i < nviews; ++i) {   
    LOG(INFO) << "Proceed Incremental Resection" << std::endl;
    // Proceed Incremental Resection 
    EuclideanCameraResection(matches, i, d.K[i],
                             &matches, &reconstruction);    
    
    EXPECT_EQ(reconstruction.GetNumberCameras(), i+1);
    camera = dynamic_cast<PinholeCamera *>(reconstruction.GetCamera(i));
    EXPECT_TRUE(camera != NULL);
    EXPECT_MATRIX_PROP(camera->orientation_matrix(), 
                       d.R[i], 
                       kPrecisionOrientationMatrix);
    EXPECT_MATRIX_PROP(camera->position(), d.t[i], kPrecisionPosition);
    
    LOG(INFO) << "Proceed Incremental Intersection" << std::endl;
    // Proceed Incremental Intersection 
    PointStructureTriangulation(matches, i, 
                                minimum_num_views_incremental, 
                                &reconstruction);

    rms = RootMeanSquareError(d.x[i], d.X, camera->projection_matrix());
    LOG(INFO) << "RMSE Camera " << i << " =" << rms << std::endl;
    // TODO(julien) Check the 3D structure coordinates and inliers
  }
  reconstruction.ClearCamerasMap();
  reconstruction.ClearStructuresMap();
  std::list<Feature *>::iterator features_iter = list_features.begin();
  for (; features_iter != list_features.end(); ++features_iter)
    delete *features_iter;
  list_features.clear();
}
}  // namespace
}  // namespace libmv
