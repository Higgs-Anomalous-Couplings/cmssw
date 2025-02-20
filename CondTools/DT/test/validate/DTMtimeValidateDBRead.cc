
/*----------------------------------------------------------------------

Toy EDAnalyzer for testing purposes only.

----------------------------------------------------------------------*/

#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "CondTools/DT/test/validate/DTMtimeValidateDBRead.h"
#include "CondFormats/DTObjects/interface/DTMtime.h"
#include "CondFormats/DataRecord/interface/DTMtimeRcd.h"
#include "CondFormats/DTObjects/interface/DTRecoConditions.h"
#include "CondFormats/DataRecord/interface/DTRecoConditionsVdriftRcd.h"

DTMtimeValidateDBRead::DTMtimeValidateDBRead(edm::ParameterSet const& p)
    : dataFileName(p.getParameter<std::string>("chkFile")),
      elogFileName(p.getParameter<std::string>("logFile")),
      readLegacyVDriftDB(p.getParameter<bool>("readLegacyVDriftDB")),
      dtmtTimeToken_(esConsumes()),
      dtrecoCondToken_(esConsumes()) {}

DTMtimeValidateDBRead::DTMtimeValidateDBRead(int i) : dtmtTimeToken_(esConsumes()), dtrecoCondToken_(esConsumes()) {}

DTMtimeValidateDBRead::~DTMtimeValidateDBRead() {}

void DTMtimeValidateDBRead::analyze(const edm::Event& e, const edm::EventSetup& context) {
  using namespace edm::eventsetup;
  // Context is not used.
  std::cout << " I AM IN RUN NUMBER " << e.id().run() << std::endl;
  std::cout << " ---EVENT NUMBER " << e.id().event() << std::endl;
  std::stringstream run_fn;
  run_fn << "run" << e.id().run() << dataFileName;
  std::ifstream chkFile(run_fn.str().c_str());
  std::ofstream logFile(elogFileName.c_str(), std::ios_base::app);

  int whe;
  int sta;
  int sec;
  int qua;
  float mTime;
  float mTrms;
  float ckmt;
  float ckrms;
  if (readLegacyVDriftDB) {  //legacy format
    auto mT = context.getHandle(dtmtTimeToken_);
    std::cout << mT->version() << std::endl;
    std::cout << std::distance(mT->begin(), mT->end()) << " data in the container" << std::endl;

    int status;
    DTMtime::const_iterator iter = mT->begin();
    DTMtime::const_iterator iend = mT->end();
    while (iter != iend) {
      const DTMtimeId& mTId = iter->first;
      const DTMtimeData& mTData = iter->second;
      status = mT->get(mTId.wheelId, mTId.stationId, mTId.sectorId, mTId.slId, mTime, mTrms, DTTimeUnits::counts);
      if (status)
        logFile << "ERROR while getting sl Mtime " << mTId.wheelId << " " << mTId.stationId << " " << mTId.sectorId
                << " " << mTId.slId << " , status = " << status << std::endl;
      if ((fabs(mTData.mTime - mTime) > 0.01) || (fabs(mTData.mTrms - mTrms) > 0.0001))
        logFile << "MISMATCH WHEN READING sl Mtime " << mTId.wheelId << " " << mTId.stationId << " " << mTId.sectorId
                << " " << mTId.slId << " : " << mTData.mTime << " " << mTData.mTrms << " -> " << mTime << " " << mTrms
                << std::endl;
      iter++;
    }

    while (chkFile >> whe >> sta >> sec >> qua >> ckmt >> ckrms) {
      status = mT->get(whe, sta, sec, qua, mTime, mTrms, DTTimeUnits::counts);
      if ((fabs(ckmt - mTime) > 0.01) || (fabs(ckrms - mTrms) > 0.0001))
        logFile << "MISMATCH IN WRITING AND READING sl Mtime " << whe << " " << sta << " " << sec << " " << qua << " : "
                << ckmt << " " << ckrms << " -> " << mTime << " " << mTrms << std::endl;
    }
  } else {
    //hVdrift;
    //context.get<DTRecoConditionsVdriftRcd>().get(hVdrift);
    const DTRecoConditions* vDriftMap_ = &context.getData(dtrecoCondToken_);
    // Consistency check: no parametrization is implemented for the time being
    int version = vDriftMap_->version();
    if (version != 1) {
      throw cms::Exception("Configuration") << "only version 1 is presently supported for VDriftDB";
    }
    while (chkFile >> whe >> sta >> sec >> qua >> ckmt >> ckrms) {
      mTime = vDriftMap_->get(DTWireId(whe, sta, sec, 1, 0, 0));
      if ((fabs(ckmt - mTime) > 0.01))
        logFile << "MISMATCH IN WRITING AND READING sl Mtime " << whe << " " << sta << " " << sec << " " << qua << " : "
                << ckmt << " " << ckrms << " -> " << mTime << " --- " << std::endl;
    }
  }
}

void DTMtimeValidateDBRead::endJob() {
  std::ifstream logFile(elogFileName.c_str());
  char* line = new char[1000];
  int errors = 0;
  std::cout << "Mtime validation result:" << std::endl;
  while (logFile.getline(line, 1000)) {
    std::cout << line << std::endl;
    errors++;
  }
  if (!errors) {
    std::cout << " ********************************* " << std::endl;
    std::cout << " ***                           *** " << std::endl;
    std::cout << " ***      NO ERRORS FOUND      *** " << std::endl;
    std::cout << " ***                           *** " << std::endl;
    std::cout << " ********************************* " << std::endl;
  }
  return;
}

DEFINE_FWK_MODULE(DTMtimeValidateDBRead);
