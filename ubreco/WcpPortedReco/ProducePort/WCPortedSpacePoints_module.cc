#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Persistency/Common/PtrMaker.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "lardata/Utilities/AssociationUtil.h"
#include "lardataobj/RecoBase/SpacePoint.h"

#include "ubreco/WcpPortedReco/ProducePort/SimpleSpacePoint.h"

#include "TTree.h"
#include "TBranch.h"
#include "TFile.h"

#include <memory>
#include <string>
#include <dirent.h>
#include <iostream>

namespace WCPsp {
  class WCPortedSpacePoints;
}

class WCPsp::WCPortedSpacePoints : public art::EDProducer {
public:
  explicit WCPortedSpacePoints(fhicl::ParameterSet const & p);
  WCPortedSpacePoints(WCPortedSpacePoints const &) = delete;
  WCPortedSpacePoints(WCPortedSpacePoints &&) = delete;
  WCPortedSpacePoints & operator = (WCPortedSpacePoints const &) = delete;
  WCPortedSpacePoints & operator = (WCPortedSpacePoints &&) = delete;

private:
  void produce(art::Event &e) override;


  std::string fInput;
  std::string fTreeName;
  bool fMainCluster;
  std::string fSpacePointLabel;
  short fTickOffset;
};

WCPsp::WCPortedSpacePoints::WCPortedSpacePoints(fhicl::ParameterSet const & p) : EDProducer{p}
{
  fInput = p.get<std::string>("PortInput");
  fTreeName = p.get<std::string>("TreeName");
  fMainCluster = p.get<bool>("MainCluster");
  fSpacePointLabel = p.get<std::string>("SpacePointLabel");
  fTickOffset = p.get<short>("TickOffset");

  produces<std::vector<SimpleSpacePoint>>();
}

void WCPsp::WCPortedSpacePoints::produce(art::Event &e){

  auto outputSpacePointVec = std::make_unique<std::vector<SimpleSpacePoint>>();

  std::cout << "Adding T_rec (WC no-trajectory-fitting neutrino cluster) spacepoints here:" << std::endl;

  std::string path(fInput);
  std::string file = (path);

  // overwriting things manually here
  // not using fhicl parameters for input file path and tree name

  file = "./WCPwork/nue_" + std::to_string((int) e.run()) + "_" + std::to_string((int) e.subRun()) + "_" + std::to_string((int) e.id().event()) + ".root";
  std::cout << "loading file: " << file << std::endl;

  try {
    TFile *fin = new TFile(file.c_str());
    TTree *tin = (TTree*)fin->Get("T_rec");

    float x, y, z, q;
    tin->SetBranchAddress("x", &x);
    tin->SetBranchAddress("y", &y);
    tin->SetBranchAddress("z", &z);
    tin->SetBranchAddress("q", &q);

    for(int i=0; i<tin->GetEntries(); i++){
      tin->GetEntry(i);
      SimpleSpacePoint xyzq = SimpleSpacePoint{x, y, z, q};
      outputSpacePointVec->emplace_back(xyzq);
    }

    fin->Close();
    std::cout << " space point vector size: "<<outputSpacePointVec->size()<<std::endl;
  } catch (std::exception &e) {
    std::cout << "File " << file << " not found, adding 0 spacepoints..." << std::endl;
  }
  
  e.put(std::move(outputSpacePointVec));
}

DEFINE_ART_MODULE(WCPsp::WCPortedSpacePoints)
