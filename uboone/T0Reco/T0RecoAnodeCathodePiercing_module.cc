////////////////////////////////////////////////////////////////////////
// Class:       T0RecoAnodeCathodePiercing
// Module Type: producer
// File:        T0RecoAnodeCathodePiercing_module.cc
//
// David Caratelli - davidc1@fnal.gov - July 13 2016
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// services etc...
#include "larcore/Geometry/Geometry.h"

// data-products
#include "lardataobj/RecoBase/Track.h"
#include "lardataobj/AnalysisBase/T0.h"
#include "lardata/Utilities/AssociationUtil.h"

// ROOT
#include "TVector3.h"

// C++
#include <memory>
#include <iostream>

class T0RecoAnodeCathodePiercing;

class T0RecoAnodeCathodePiercing : public art::EDProducer {
public:
  explicit T0RecoAnodeCathodePiercing(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  T0RecoAnodeCathodePiercing(T0RecoAnodeCathodePiercing const &) = delete;
  T0RecoAnodeCathodePiercing(T0RecoAnodeCathodePiercing &&) = delete;
  T0RecoAnodeCathodePiercing & operator = (T0RecoAnodeCathodePiercing const &) = delete;
  T0RecoAnodeCathodePiercing & operator = (T0RecoAnodeCathodePiercing &&) = delete;

  // Required functions.
  void produce(art::Event & e) override;


private:

  // producer of 3D reconstructed track to be used
  std::string fTrackProducer;

  // set "resolution". How far away from the detector bounds
  // do we want to be to make a claim.
  double fResolution; // [cm]

  // drift velocity // cm / us
  double fDriftVelocity;

  // define top, bottom, front and back boundaries of TPC
  double _TOP, _BOTTOM, _FRONT, _BACK;

  // detector width [drift-coord]
  double _det_width; // [cm]

  // functions to be used throughot module
  bool   TrackExitsBottom    (const std::vector<TVector3>& sorted_trk);
  bool   TrackEntersSide     (const std::vector<TVector3>& sorted_trk);
  bool   Anode               (const std::vector<TVector3>& sorted_trk);
  void   SortTrackPoints     (const recob::Track& track,
							  std::vector<TVector3>& sorted_trk);
  double GetCrossingTimeCoord(const std::vector<TVector3>& sorted_trk);
  
  
};


T0RecoAnodeCathodePiercing::T0RecoAnodeCathodePiercing(fhicl::ParameterSet const & p)
// :
// Initialize member data here.
{
  produces< std::vector< anab::T0 > >();
  produces< art::Assns <recob::Track, anab::T0> >();

  fTrackProducer = p.get<std::string>("TrackProducer");
  fResolution    = p.get<double>     ("Resolution");
  fDriftVelocity = p.get<double>     ("DriftVelocity");
  
  // get boundaries based on detector bounds
  auto const* geom = lar::providerFrom<geo::Geometry>();

  _TOP    =   geom->DetHalfHeight() - fResolution;
  _BOTTOM = - geom->DetHalfHeight() + fResolution;
  _FRONT  =   fResolution;
  _BACK   =   geom->DetLength() - fResolution;
  
  _det_width = geom->DetHalfWidth() * 2;

}

void T0RecoAnodeCathodePiercing::produce(art::Event & e)
{

  // produce OpFlash data-product to be filled within module
  std::unique_ptr< std::vector<anab::T0> > T0_v(new std::vector<anab::T0>);
  std::unique_ptr< art::Assns <recob::Track, anab::T0> > assn_v( new art::Assns<recob::Track, anab::T0>);

  // load tracks previously created for which T0 reconstruction should occur
  art::Handle<std::vector<recob::Track> > track_h;
  e.getByLabel(fTrackProducer,track_h);

  // make sure hits look good
  if(!track_h.isValid()) {
    std::cerr<<"\033[93m[ERROR]\033[00m ... could not locate Track!"<<std::endl;
    throw std::exception();
  }

  std::vector<art::Ptr<recob::Track> > TrkVec;
  art::fill_ptr_vector(TrkVec, track_h);

  /*
  // convert to vector of unique pointers
  std::unique_ptr< std::vector<recob::Track> > Track_v;
  for (auto track : *track_h)
    Track_v->push_back( track );
  */

  // loop through reconstructed tracks
  for (auto& track : TrkVec){
    
    // get sorted points for the track object [assuming downwards going]
    std::vector<TVector3> sorted_trk;
    SortTrackPoints(*track,sorted_trk);

    // check if the track is of good quality:
    // must exit through the bottom [indicates track reconstruction has gone 'till the end]
    if (TrackExitsBottom(sorted_trk) == false)
      continue;
    if (TrackEntersSide(sorted_trk) == false)
      continue;

    // made it this far -> the track is good to be used
    // figure out if it pierces the anode or cathode
    bool anode = Anode(sorted_trk);
    
    // get the X coordinate of the point piercing the anode/cathode
    double trkX = GetCrossingTimeCoord(sorted_trk);

    // reconstruct track T0 w.r.t. trigger time
    double trkT = 0;
    if (anode)
      trkT = trkX / fDriftVelocity;
    else
      trkT = (trkX - _det_width) / fDriftVelocity;

    // create T0 object with this information!
    anab::T0 t0(trkT, 0, 0);
    
   T0_v->emplace_back(t0);
   util::CreateAssn(*this, e, *T0_v, track, *assn_v);

  }// for all reconstructed tracks
  
  e.put(std::move(T0_v));
  e.put(std::move(assn_v));

}


bool   T0RecoAnodeCathodePiercing::TrackExitsBottom(const std::vector<TVector3>& sorted_trk)
{
  // check that the last point in the track
  // pierces the bottom boundary of the TPC
  if ( sorted_trk.at( sorted_trk.size() - 1).Y() < _BOTTOM )
    return true;
  return false;
}

bool   T0RecoAnodeCathodePiercing::TrackEntersSide(const std::vector<TVector3>& sorted_trk)
{
      
  // check that the top-most point
  // is not on the top of the TPC
  // nor on the front & back of the TPC

  auto const& top_pt = sorted_trk.at(0);
  
  // if highest point above the TOP -> false
  if (top_pt.Y() > _TOP)
    return false;
    
  // if highest point in Z close to front or back
  // -> FALSE
  if ( (top_pt.Z() < _FRONT) or (top_pt.Z() > _BACK) )
    return false;
    
  return true;
}

bool   T0RecoAnodeCathodePiercing::Anode(const std::vector<TVector3>& sorted_trk)
{

  // we know the track eneters either the
  // anode or cathode
  // at this point figure out
  // if if ENTERS the ANODE or CATHODE
  // ANODE: top point must be at lower X-coord
  // than bottom point
  // CATHODE: top point must be at larger X-coord
  // than bottom point
  // assume track has already been sorted
  // such that the 1st point is the most elevated in Y coord.
  // return TRUE if passes the ANODE
  
  auto const& top    = sorted_trk.at(0);
  auto const& bottom = sorted_trk.at( sorted_trk.size() - 1 );
    
  if (top.X() < bottom.X())
    return true;
  return false;
}

void   T0RecoAnodeCathodePiercing::SortTrackPoints(const recob::Track& track,
						   std::vector<TVector3>& sorted_trk)
{
  // vector to store 3D coordinates of
  // ordered track
  sorted_trk.clear();
    
  // take the reconstructed 3D track
  // and assuming it is downwards
  // going, sort points so that
  // the track starts at the top
  
  // which point is further up in Y coord?
  // start or end?
  auto const&N = track.NumberTrajectoryPoints();
  auto const&start = track.LocationAtPoint(0);
  auto const&end   = track.LocationAtPoint( N - 1 );
  // if points are ordered correctly
  if (start.Y() > end.Y()){
    for (size_t i=0; i < N; i++)
      sorted_trk.push_back( track.LocationAtPoint(i) );
  }
  // otherwise flip order
  else{
    for (size_t i=0; i < N; i++)
      sorted_trk.push_back( track.LocationAtPoint( N - i - 1) );
  }
}

double T0RecoAnodeCathodePiercing::GetCrossingTimeCoord(const std::vector<TVector3>& sorted_trk)
{
  // get the drift-coordinate value
  // associated with the point
  // along the track piercing the anode / cathode
  return sorted_trk.at(0).X();
}

DEFINE_ART_MODULE(T0RecoAnodeCathodePiercing)
