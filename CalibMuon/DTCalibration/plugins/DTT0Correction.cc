/*
 *  See header file for a description of this class.
 */

#include "DTT0Correction.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "Geometry/Records/interface/MuonGeometryRecord.h"
#include "Geometry/DTGeometry/interface/DTGeometry.h"
#include "Geometry/DTGeometry/interface/DTSuperLayer.h"

#include "CondFormats/DTObjects/interface/DTT0.h"
#include "CondFormats/DataRecord/interface/DTT0Rcd.h"

#include "CalibMuon/DTCalibration/interface/DTCalibDBUtils.h"

#include "CalibMuon/DTCalibration/interface/DTT0CorrectionFactory.h"
#include "CalibMuon/DTCalibration/interface/DTT0BaseCorrection.h"

#include <iostream>
#include <fstream>

using namespace edm;
using namespace std;

DTT0Correction::DTT0Correction(const ParameterSet& pset)
    : correctionAlgo_{DTT0CorrectionFactory::get()->create(pset.getParameter<string>("correctionAlgo"),
                                                           pset.getParameter<ParameterSet>("correctionAlgoConfig"))},
      dtGeomToken_(esConsumes()),
      t0Token_(esConsumes()) {
  LogVerbatim("Calibration") << "[DTT0Correction] Constructor called" << endl;
}

DTT0Correction::~DTT0Correction() { LogVerbatim("Calibration") << "[DTT0Correction] Destructor called" << endl; }

void DTT0Correction::beginRun(const edm::Run& run, const edm::EventSetup& setup) {
  // Get t0 record from DB
  ESHandle<DTT0> t0H;
  t0H = setup.getHandle(t0Token_);
  t0Map_ = &setup.getData(t0Token_);
  LogVerbatim("Calibration") << "[DTT0Correction]: T0 version: " << t0H->version() << endl;

  // Get geometry from Event Setup
  muonGeom_ = setup.getHandle(dtGeomToken_);

  // Pass EventSetup to correction Algo
  correctionAlgo_->setES(setup);
}

void DTT0Correction::endJob() {
  // Create the object to be written to DB
  DTT0* t0NewMap = new DTT0();

  // Loop over all channels
  for (vector<const DTSuperLayer*>::const_iterator sl = muonGeom_->superLayers().begin();
       sl != muonGeom_->superLayers().end();
       ++sl) {
    for (vector<const DTLayer*>::const_iterator layer = (*sl)->layers().begin(); layer != (*sl)->layers().end();
         ++layer) {
      // Access layer topology
      const DTTopology& dtTopo = (*layer)->specificTopology();
      const int firstWire = dtTopo.firstChannel();
      const int lastWire = dtTopo.lastChannel();
      //const int nWires = dtTopo.channels();

      //Loop on wires
      for (int wire = firstWire; wire <= lastWire; ++wire) {
        DTWireId wireId((*layer)->id(), wire);

        // Get old value from DB
        float t0Mean, t0RMS;
        int status = t0Map_->get(wireId, t0Mean, t0RMS, DTTimeUnits::counts);

        // Compute new t0 for this wire
        try {
          dtCalibration::DTT0Data t0Corr = correctionAlgo_->correction(wireId);
          float t0MeanNew = t0Corr.mean;
          float t0RMSNew = t0Corr.rms;
          t0NewMap->set(wireId, t0MeanNew, t0RMSNew, DTTimeUnits::counts);

          LogVerbatim("Calibration") << "New t0 for: " << wireId << " mean from " << t0Mean << " to " << t0MeanNew
                                     << " rms from " << t0RMS << " to " << t0RMSNew << endl;
        } catch (cms::Exception& e) {
          LogError("Calibration") << e.explainSelf();
          // Set db to the old value, if it was there in the first place
          if (!status) {
            t0NewMap->set(wireId, t0Mean, t0RMS, DTTimeUnits::counts);
            LogVerbatim("Calibration") << "Keep old t0 for: " << wireId << " mean " << t0Mean << " rms " << t0RMS
                                       << endl;
          }
          continue;
        }
      }  // End of loop on wires
    }    // End of loop on layers
  }      // End of loop on superlayers

  //Write object to DB
  LogVerbatim("Calibration") << "[DTT0Correction]: Writing t0 object to DB!" << endl;
  string record = "DTT0Rcd";
  DTCalibDBUtils::writeToDB<DTT0>(record, t0NewMap);
}
