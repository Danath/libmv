// Copyright (c) 2009 libmv authors.
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

#ifndef LIBMV_MULTIVIEW_RECONSTRUCTION_H_
#define LIBMV_MULTIVIEW_RECONSTRUCTION_H_

#include <cstdio>
#include <list>
#include <map>

#include "libmv/correspondence/matches.h"
#include "libmv/logging/logging.h"
#include "libmv/numeric/numeric.h"
#include "libmv/multiview/camera.h"
#include "libmv/multiview/structure.h"

namespace libmv {
// TODO(julien) shoud a camera have the same ID then an image? 
typedef Matches::ImageID CameraID;
typedef Matches::TrackID StructureID;

// TODO(julien) clean this file (too many functions...)
// Use cases:
//
// Reconstruction is a subset of the tracks.  For a single track, there is a 3D
// point. For a single camera there are many points.
//
// For bundling: Must map between a single vector of parameters and the
// reconstruction. Can do this post-reconstruction if data is a pointer inside
// camera and structures to an eigen vector or array.
//
// Perhaps reconstruction error should be done separately; by binding 'error
// blocks' which combine camera, feature, and structure.
//
// For outputting to a blender script:
//
//   Iterate over (derived) cameras
//   Iterate over (derived) points
//
// For bundling:
// 
//   Iterate over features, and access corresponding structure / camera.
//
// For resection:
//
//   1. Find a image that has no reconstructed camera.
//   2. Find the points that have a corresponding Structure; store this map.
//   3. Recover camera via resection and the Feature<->Structure
//      correspondences.
//
//   For 1: Loop through each Image, see if there is a Camera.
//     
//     Need fast 'Image to Camera or NULL" on Reconstruction.
//
//   For 2: Loop through Tracks matching Features in this Image, checking for
//   Structure.
//
//     Need fast "Feature to Track or NULL" on Matches. (TracksInImagesFI)
//     Need fast "Track to Structure or NULL" query on Reconstruction.
//
// For intersection:
//
//   Find sets of Images that have Features with no corresponding Structure.
//   That is to say, for a set of Images that have Cameras reconstructed, find
//   tracks that are not yet reconstructed that are visible in these images.
//
//   To do this: Use TracksInImagesIterator over the set of Images. Then check
//   if that track is in the reconstruction.
//
//     Need fast "Track has Structure?" query.
//     Need fast "Image has Camera?" query.
//   

// Possible signatures:
//
//   ReconstructFromTwoViews(Matches, ID, ID, Matches *, Reconstruction *);
//   ReconstructFromThreeViews(Matches, ID, ID, ID, Matches *, Reconstruction *);
//   CameraResection(Matches, ID, Matches *, Reconstruction *);
//   MergeReconstructions(Matches, Reconstruction &, Reconstruction &, Matches *, Reconstruction *);
//   BundleAdjust(Matches, Reconstruction *);
//
// The reconstruction takes ownership of camera and structure.
class Reconstruction {
 public:
   
  Reconstruction() {}
  ~Reconstruction() {}

  void InsertCamera(CameraID id, Camera *camera) {
    std::map<CameraID, Camera *>::iterator it = cameras_.find(id);
    if (it != cameras_.end()) {
      delete it->second;
      it->second = camera;
    } else {
      cameras_[id] = camera;
    }
  }
  
  void InsertTrack(StructureID id, Structure *structure) {
    std::map<StructureID, Structure *>::iterator it = structures_.find(id);
    if (it != structures_.end()) {
      delete it->second;
      it->second = structure; 
    } else {
      structures_[id] = structure;
    }
  }
  
  bool ImageHasCamera(CameraID id) const {
    std::map<CameraID, Camera *>::const_iterator it = cameras_.find(id);
    return it != cameras_.end();
  }
  
  bool TrackHasStructure(StructureID id) const {
    std::map<StructureID, Structure *>::const_iterator it =
      structures_.find(id);
    return it != structures_.end();
  }
  
  Camera * GetCamera(CameraID id) const {
    std::map<CameraID, Camera *>::const_iterator it = cameras_.find(id);
    if (it != cameras_.end()) {
      return it->second;
    }
    return NULL;
  }
  
  Structure * GetStructure(StructureID id) const {
    std::map<StructureID, Structure *>::const_iterator it =
      structures_.find(id);
    if (it != structures_.end()) {
      return it->second;
    }
    return NULL;
  }
  
  void ClearCamerasMap() {
    std::map<CameraID, Camera *>::iterator it = cameras_.begin();
    for (; it != cameras_.end(); ++it) {
      delete it->second;
    }
    cameras_.clear();
  }
  void ClearStructuresMap() {
    std::map<StructureID, Structure *>::iterator it = structures_.begin();
    for (; it != structures_.end(); ++it) {
      delete it->second;
    }
    structures_.clear();
  }
  
  size_t GetNumberCameras() const    { return cameras_.size(); }
  size_t GetNumberStructures() const { return structures_.size(); }

  std::map<CameraID, Camera *>       & cameras()  { return cameras_; }
  std::map<StructureID, Structure *> & structures() { return structures_; }
  Matches                            & matches() { return matches_; }

  const std::map<CameraID, Camera *>        & cameras() const {
    return cameras_; }
  const std::map<StructureID, Structure *>  & structures() const  {
    return structures_; }
  const Matches                             & matches() const {
    return matches_; }
  
 private:
  std::map<CameraID, Camera *>        cameras_;
  std::map<StructureID, Structure *>  structures_;
  Matches                             matches_;
};

// Estimates the projection matrices of the two cameras using the fundamental
// matrix.
// The method:
//    selects common matches of the two images
//    robustly estimates the fundamental matrix
//    if the first image has no camera, it creates the camera and initializes
//       the projection matrix as the world frame
//    else, note that we also set the first projection matrix to the world
//        frame (for the time being)
//    estimates the projection matrix of the second camera from the fundamental
//      matrix
//    creates and adds it to the reconstruction
//    inserts only inliers matches into matches_inliers
// Returns true if the projection matrix has succeed
// Returns false if 
//    the number of common matches is less than 7
bool ReconstructFromTwoUncalibratedViews(const Matches &matches, 
                                         CameraID image_id1, 
                                         CameraID image_id2, 
                                         Matches *matches_inliers,
                                         Reconstruction *reconstruction);

// Estimates the poses of the two cameras using the fundamental and essential
// matrices.
// The method:
//    selects common matches of the two images
//    robustly estimates the fundamental matrix
//    estimates the essential matrix from the fundamental matrix
//    extracts the relative motion from the essential matrix
//    if the first image has no camera, it creates the camera and initializes
//      the pose to be the world frame
//    estimates the absolute pose of the second camera from the first pose and
//      the estimated motion.
//    creates and adds it to the reconstruction
//    inserts only inliers matches into matches_inliers
// Returns true if the pose estimation has succeed
// Returns false if 
//    the number of common matches is less than 7
//    there is no solution for the relative motion from the essential matrix
bool ReconstructFromTwoCalibratedViews(const Matches &matches, 
                                       CameraID image_id1, 
                                       CameraID image_id2, 
                                       const Mat3 &K1, 
                                       const Mat3 &K2, 
                                       Matches *matches_inliers,
                                       Reconstruction *reconstruction);

// Estimates the projection matrix of the camera using the already reconstructed
// structures.
// The method:
//    selects the tracks that have an already reconstructed structure
//    robustly estimates the camera projection matrix by resection (P)
//    creates and adds the new camera to reconstruction
//    inserts only inliers matches into matches_inliers
// Returns true if the resection has succeed
// Returns false if 
//    the number of reconstructed Tracks is less than 6
bool UncalibratedCameraResection(const Matches &matches, 
                                 CameraID image_id, 
                                 Matches *matches_inliers,
                                 Reconstruction *reconstruction);

// Estimates the pose of the camera using the already reconstructed structures.
// The method:
//    selects the tracks that have an already reconstructed structure
//    robustly estimates the camera extrinsic parameters (R,t) by resection
//    creates and adds the new camera to reconstruction
//    inserts only inliers matches into matches_inliers
// Returns true if the resection has succeed
// Returns false if 
//    the number of reconstructed Tracks is less than 5
bool CalibratedCameraResection(const Matches &matches, 
                               CameraID image_id, 
                               const Mat3 &K, 
                               Matches *matches_inliers,
                               Reconstruction *reconstruction);

// This method upgrade the reconstruction into a metric one.
// The method use the linear approach;
//  computes a metric reconstruction from a projective one by computing
//    the dual absolute quadric using linear constraints.
//  estimates the metric rectification H
//  upgrades the reconstruction using H
bool UpgradeToMetric(const Matches &matches, 
                     Reconstruction *reconstruction);

// This method performs an Euclidean Bundle Adjustment
// and returns the root mean square error.
double BundleAdjust(const Matches &matches, 
                    Reconstruction *reconstruction);

// This method selects an efficient order of images, with the first two are
// selected because they have a good baseline.
// The criterion is:  the homography error x number of common matches
// The oupout connected_graph_list contains a list of connected graphs
// (vectors), each vector contains the ImageID ordered by the criterion.
void SelectEfficientImageOrder(
  const Matches &matches, 
  std::list<vector<Matches::ImageID> >*connected_graph_list);

}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_RECONSTRUCTION_H_
