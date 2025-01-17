#ifndef ANGLE3DFROMVERTEXQWEIGHTED_CXX
#define ANGLE3DFROMVERTEXQWEIGHTED_CXX

#include <iostream>
#include "ubreco/ShowerReco/ShowerReco3D/Base/ShowerRecoModuleBase.h"
/**
   \class ShowerRecoModuleBase
   User defined class ShowerRecoModuleBase ... these comments are used to generate
   doxygen documentation!
 */

#include <math.h>
#include <sstream>
#include <algorithm>

#include "larcore/Geometry/WireReadout.h"
#include "larcore/CoreUtils/ServiceUtil.h" // lar::providerFrom<>()
#include "lardata/Utilities/GeometryUtilities.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

namespace showerreco {
  
  class Angle3DFromVtxQweighted : public ShowerRecoModuleBase {
    
  public:
    
    /// Default constructor
    Angle3DFromVtxQweighted(const fhicl::ParameterSet& pset); 
    
    /// Default destructor
    ~Angle3DFromVtxQweighted(){};
    
    void configure(const fhicl::ParameterSet& pset);
    
    void do_reconstruction(util::GeometryUtilities const& gser,
                           const ::protoshower::ProtoShower &, Shower_t &);
    
  private:
    
    double _wire2cm, _time2cm;
    
  };
  
  Angle3DFromVtxQweighted::Angle3DFromVtxQweighted(const fhicl::ParameterSet& pset)
  {
    configure(pset);
    _name = "Angle3DFromVtxQweighted";

    auto const& channelMap = art::ServiceHandle<geo::WireReadout>()->Get();
    auto const clockData = art::ServiceHandle<detinfo::DetectorClocksService>()->DataForJob();
    auto const detp = art::ServiceHandle<detinfo::DetectorPropertiesService>()->DataForJob(clockData);
    _wire2cm = channelMap.Plane(geo::PlaneID{0,0,0}).WirePitch();
    _time2cm = sampling_rate(clockData) / 1000.0 * detp.DriftVelocity( detp.Efield(), detp.Temperature() );
  }

  void Angle3DFromVtxQweighted::configure(const fhicl::ParameterSet& pset)
  {
    _verbose   = pset.get<bool>("verbose",false);
    return;
  }
  
  void Angle3DFromVtxQweighted::do_reconstruction(util::GeometryUtilities const& geomH,
                                                  const ::protoshower::ProtoShower & proto_shower,
						  Shower_t& resultShower) {
    
    //if the module does not have 2D cluster info -> fail the reconstruction
    if (!proto_shower.hasVertex()){
      std::stringstream ss;
      ss << "Fail @ algo " << this->name() << " due to missing Vertex";
      throw ShowerRecoException(ss.str());
    }

    //if the module does not have 2D cluster info -> fail the reconstruction
    if (!proto_shower.hasCluster2D()){
      std::stringstream ss;
      ss << "Fail @ algo " << this->name() << " due to missing 2D cluster";
      throw ShowerRecoException(ss.str());
    }

    if (proto_shower.hasVertex() == false){
      std::stringstream ss;
      ss << "Fail @ algo " << this->name() << " due to number of vertices != one!";
      throw ShowerRecoException(ss.str());
    }
    
    // get the proto-shower 3D vertex
    auto const vtx = geo::vect::toPoint(proto_shower.vertex());

    std::vector<double> dir3D = {0,0,0};

    auto & clusters = proto_shower.clusters();

    // planes with largest number of hits used to get 3D direction
    std::vector<int> planeHits(3,0);
    std::vector<::util::PxPoint> planeDir(3);

    // we want an energy for each plane
    auto const& channelMap = art::ServiceHandle<geo::WireReadout>()->Get();
    for (size_t n = 0; n < clusters.size(); n++) {
      
      // get the hits associated with this cluster
      auto const& hits = clusters.at(n)._hits;
      
      // get the plane associated with this cluster
      auto const& pl = clusters.at(n)._plane;

      // project vertex onto this plane
      //auto const& vtx2D = geomH.Get2DPointProjectionCM(vtx,pl);
      // get wire for yz pointx
      auto wire = channelMap.Plane(geo::PlaneID(0, 0, pl)).WireCoordinate(vtx) * _wire2cm;
      auto time = vtx.X();
      util::PxPoint vtx2D(pl,wire,time);

      if (_verbose){
        std::cout << "3D vertex : [ " << vtx.X() << ", " << vtx.Y() << ", " << vtx.Z() << " ]" << std::endl;
	std::cout << "2D projection of vtx on plane " << pl << " @ [w,t] -> [ " << vtx2D.w << ", " << vtx2D.t << "]" <<  std::endl;
      }

      // get the charge-averaged 2D vector pointing from vtx in shower direction
      ::util::PxPoint weightedDir;
      weightedDir.w = 0;
      weightedDir.t = 0;
      double Qtot = 0;
      for (auto const& hit : hits){
	weightedDir.w += (hit.w - vtx2D.w) * hit.charge;
	weightedDir.t += (hit.t - vtx2D.t) * hit.charge;
	Qtot += hit.charge;
      }

      if (_verbose){
	std::cout << "Qtot is " << Qtot << std::endl;
      }

      weightedDir.w /= Qtot;
      weightedDir.t /= Qtot;

      planeHits[pl] = (int)hits.size();
      planeDir[pl]  = weightedDir;

    }// for all planes

    int pl_max = 0;
    int n_max  = 0;
    int pl_mid = 0;
    int pl_min = 0;
    int n_min  = 1000;
    for (size_t pl=0; pl < planeHits.size(); pl++){
      if (planeHits[pl] > n_max){
	pl_max = pl;
	n_max  = planeHits[pl];
      }
      if (planeHits[pl] < n_min){
	pl_min = pl;
	n_min  = planeHits[pl];
      }
    }

    if (pl_max == pl_min) {
      std::stringstream ss;
      ss << "Fail @ algo " << this->name() << " due to PL max and PL min being the same!";
      throw ShowerRecoException(ss.str());
    }

    // find the medium plane
    std::vector<int> planes = {0,1,2};
    planes.erase( std::find( planes.begin(), planes.end(), pl_max) );
    planes.erase( std::find( planes.begin(), planes.end(), pl_min) );
    pl_mid = planes[0];

    double slope_max, slope_mid;
    double angle_max, angle_mid;
    slope_max = planeDir[pl_max].t / planeDir[pl_max].w;
    angle_max = atan(slope_max);
    angle_max = atan2( planeDir[pl_max].t , planeDir[pl_max].w );
    slope_mid = planeDir[pl_mid].t / planeDir[pl_mid].w;
    angle_mid = atan(slope_mid);
    angle_mid = atan2( planeDir[pl_mid].t , planeDir[pl_mid].w );
    
    double theta, phi;
    geomH.Get3DaxisN(pl_max, pl_mid,
		      angle_max, angle_mid,
		      phi, theta);

    theta *= (3.1415/180.);
    phi   *= (3.1415/180.);

    if (_verbose)
      std::cout << "theta : " << theta << " \t phi : " << phi << std::endl;
    
    resultShower.fDCosStart[1] = sin(theta);
    resultShower.fDCosStart[0] = cos(theta) * sin(phi);
    resultShower.fDCosStart[2] = cos(theta) * cos(phi);
    
    return;
  }
  
  DEFINE_ART_CLASS_TOOL(Angle3DFromVtxQweighted)
} //showerreco

#endif
