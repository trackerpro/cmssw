#include "DQM/SiStripCommissioningClients/interface/SiStripCommissioningOfflineClient.h"
#include "DataFormats/SiStripCommon/interface/SiStripEnumsAndStrings.h"
#include "DataFormats/SiStripCommon/interface/SiStripHistoTitle.h"
#include "DataFormats/SiStripCommon/interface/SiStripFecKey.h"
#include "DQM/SiStripCommissioningClients/interface/FastFedCablingHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/FedCablingHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/ApvTimingHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/OptoScanHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/VpspScanHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/PedestalsHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/PedsOnlyHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/PedsFullNoiseHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/NoiseHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/SamplingHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/CalibrationHistograms.h"
#include "DQM/SiStripCommissioningClients/interface/DaqScopeModeHistograms.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "classlib/utils/RegexpMatch.h"
#include "classlib/utils/Regexp.h"
#include "classlib/utils/StringOps.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <dirent.h>
#include <cerrno>
#include "TProfile.h"
#include "TKey.h"
#include <cstdint>

using namespace sistrip;

// -----------------------------------------------------------------------------
//
SiStripCommissioningOfflineClient::SiStripCommissioningOfflineClient(const edm::ParameterSet& pset)
    : bei_(edm::Service<DQMStore>().operator->()),
      histos_(nullptr),
      outputFileName_(pset.getUntrackedParameter<std::string>("OutputRootFile", "")),
      collateHistos_(!pset.getUntrackedParameter<bool>("UseClientFile", false)),
      analyzeHistos_(pset.getUntrackedParameter<bool>("AnalyzeHistos", true)),
      xmlFile_((pset.getUntrackedParameter<edm::FileInPath>("SummaryXmlFile", edm::FileInPath())).fullPath()),
      createSummaryPlots_(false),
      clientHistos_(false),
      uploadToDb_(false),
      runType_(sistrip::UNKNOWN_RUN_TYPE),
      runNumber_(0),
      partitionName_(pset.existsAs<std::string>("PartitionName") ? pset.getParameter<std::string>("PartitionName")
                                                                 : ""),
      map_(),
      plots_(),
      parameters_(pset) {
  LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                         << " Constructing object...";
  setInputFiles(inputFiles_,
                pset.getUntrackedParameter<std::string>("FilePath"),
                pset.existsAs<std::string>("PartitionName") ? pset.getParameter<std::string>("PartitionName") : "",
                pset.getUntrackedParameter<uint32_t>("RunNumber"),
                collateHistos_);
}

// -----------------------------------------------------------------------------
//
SiStripCommissioningOfflineClient::~SiStripCommissioningOfflineClient() {
  LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                         << " Destructing object...";
}

// -----------------------------------------------------------------------------
//
void SiStripCommissioningOfflineClient::beginRun(const edm::Run& run, const edm::EventSetup& setup) {

  LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                         << " Analyzing root file(s)...";

  // Check for null pointer
  if (!bei_) {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " NULL pointer to DQMStore!"
                                << " Aborting...";
    return;
  }

  // Check if .root file can be opened
  std::vector<std::string>::const_iterator ifile = inputFiles_.begin();
  for (; ifile != inputFiles_.end(); ifile++) {
    std::ifstream root_file;
    root_file.open(ifile->c_str());
    if (!root_file.is_open()) {
      edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                  << " The input root file \"" << *ifile << "\" could not be opened!"
                                  << " Please check the path and filename!";
    } 
    else {
      root_file.close();
      std::string::size_type found = ifile->find(sistrip::dqmClientFileName_);
      if (found != std::string::npos && clientHistos_) {
        edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                    << " The input root files appear to be a mixture"
                                    << " of \"Source\" and \"Client\" files!"
                                    << " Aborting...";
        return;
      }
      if (found != std::string::npos && inputFiles_.size() != 1) {
        edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                    << " There appear to be multiple input \"Client\" root files!"
                                    << " Aborting...";
        return;
      }
      if (found != std::string::npos) {
        clientHistos_ = true;
      }
    }
  }

  if (clientHistos_ && inputFiles_.size() == 1) {
    edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                   << " Collated histograms found in input root file \"" << inputFiles_[0] << "\"";
  }

  // Check if .xml file can be opened
  if (!xmlFile_.empty()) {
    std::ifstream xml_file;
    xml_file.open(xmlFile_.c_str());
    if (!xml_file.is_open()) {
      edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                  << " The SummaryPlot XML file \"" << xmlFile_ << "\" could not be opened!"
                                  << " Please check the path and filename!"
                                  << " Aborting...";
      return;
    } else {
      createSummaryPlots_ = true;
      xml_file.close();
    }
  }

  // Open root file(s) and create ME's
  if (inputFiles_.empty()) {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " No input root files specified!";
    return;
  }

  edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                 << " Opening root files. This may take some time!...";

  std::vector<std::string>::const_iterator jfile = inputFiles_.begin();
  for (; jfile != inputFiles_.end(); jfile++) {
    LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                           << " Opening root file \"" << *jfile << "\"... (This may take some time.)";
    if (clientHistos_) { //
      openDQMStore(*jfile,std::string(sistrip::dqmRoot_)+"/"+std::string(sistrip::collate_));
    } else {
      openDQMStore(*jfile,std::string(sistrip::dqmRoot_)+"/SiStrip");
    }
    LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                           << " Opened root file \"" << *jfile << "\"!";
  }
  edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                 << " Opened " << inputFiles_.size() << " root files!";


  // Retrieve list of histograms
  auto allmes = bei_->getAllContents("");
  std::vector<std::string> contents_original;
  for(auto const & me : allmes) {
    contents_original.push_back(me->getFullname());
  }

  // Adjust the naming covention to obey to the expect nomenclature for CMSSW < 11_0_X
  std::vector<std::string> contents;
  for(auto const & me : contents_original) {
    size_t pos = me.find_last_of("/");
    if(pos != std::string::npos){
      std::string path = me.substr(0,pos);
      TString path_tmp (path);
      if(std::find(contents.begin(),contents.end(),path) == contents.end()){
	contents.push_back(path+":"+me.substr(pos+1,std::string::npos));
      }
      else{
	contents.push_back(","+me.substr(pos+1,std::string::npos));
      }      
    }
  }
  contents_original.clear();

  // If using client file, remove "source" histograms from list                                                                                                                                       
  if ( clientHistos_ ) {
    std::vector<std::string> temp;
    std::vector<std::string>::iterator istr = contents.begin();
    for ( ; istr != contents.end(); istr++ ) {
      if ( istr->find(sistrip::collate_) != std::string::npos ) {
        temp.push_back( *istr );
      }
    }
    contents.clear();
    contents = temp;
  }

  // Some debug
  LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                         << " Found " << contents.size() << " directories containing MonitorElements";

  // Some more debug
  if (false) {
    std::stringstream ss;
    ss << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
       << " Directories found: " << std::endl;
    std::vector<std::string>::iterator istr = contents.begin();
    for (; istr != contents.end(); istr++) {
      ss << " " << *istr << std::endl;
    }
    LogTrace(mlDqmClient_) << ss.str();
  }

  // Extract run type from contents
  runType_ = CommissioningHistograms::runType(bei_, contents);

  // Extract run number from contents
  runNumber_ = CommissioningHistograms::runNumber(bei_, contents);

  // Copy custom information to the collated structure
  CommissioningHistograms::copyCustomInformation(bei_, contents);

  // Check runType
  if (runType_ == sistrip::UNKNOWN_RUN_TYPE) {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " Unknown commissioning runType: " << SiStripEnumsAndStrings::runType(runType_)
                                << " and run number is " << runNumber_;
    return;
  } else {
    edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                   << " Run type is " << SiStripEnumsAndStrings::runType(runType_)
                                   << " and run number is " << runNumber_;
  }

  // Open and parse "summary plot" xml file
  if (createSummaryPlots_) {
    edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                   << " Parsing summary plot XML file...";
    SummaryPlotXmlParser xml_file;
    xml_file.parseXML(xmlFile_);
    plots_ = xml_file.summaryPlots(runType_);
    edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                   << " Parsed summary plot XML file and found " << plots_.size() << " plots defined!";
  } else {
    edm::LogWarning(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                  << " Null string for SummaryPlotXmlFile!"
                                  << " No summary plots will be created!";
  }

  // Some debug
  std::stringstream ss;
  ss << "[SiStripCommissioningOfflineClient::" << __func__ << "]" << std::endl << " Input root files      : ";
  if (inputFiles_.empty()) {
    ss << "(none)";
  } else {
    std::vector<std::string>::const_iterator ifile = inputFiles_.begin();
    for (; ifile != inputFiles_.end(); ifile++) {
      if (ifile != inputFiles_.begin()) {
        ss << std::setw(25) << std::setfill(' ') << ": ";
      }
      ss << "\"" << *ifile << "\"" << std::endl;
    }
  }
  ss << " Run type              : \"" << SiStripEnumsAndStrings::runType(runType_) << "\"" << std::endl
     << " Run number            :  " << runNumber_ << std::endl
     << " Summary plot XML file : ";
  if (xmlFile_.empty()) {
    ss << "(none)";
  } else {
    ss << "\"" << xmlFile_ << "\"";
  }
  edm::LogVerbatim(mlDqmClient_) << ss.str();

  // Virtual method that creates CommissioningHistogram object
  LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                         << " Creating CommissioningHistogram object...";
  createHistos(parameters_, setup);
  if (histos_) {
    LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                           << " Created CommissioningHistogram object!";
  } else {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " NULL pointer to CommissioningHistogram object!"
                                << " Aborting...";
    return;
  }

  // Perform collation
  if (histos_) {
    histos_->extractHistograms(contents);
  }

  // Perform analysis
  if (analyzeHistos_) {
    LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                           << " Analyzing histograms...";
    if (histos_) {
      histos_->histoAnalysis(true);
    }
    LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                           << " Analyzed histograms!";
  } else {
    edm::LogWarning(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                  << " No histogram analysis performed!";
  }
  
  // Create summary plots
  if (createSummaryPlots_) {
    edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                   << " Generating summary plots...";
    std::vector<SummaryPlot>::const_iterator iplot = plots_.begin();
    for (; iplot != plots_.end(); iplot++) {
      if (histos_) {
        histos_->createSummaryHisto(iplot->monitorable(), iplot->presentation(), iplot->level(), iplot->granularity());
      }
    }
    edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                   << " Generated summary plots!";
  } else {
    edm::LogWarning(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                  << " No summary plots generated!";
  }

  // Save client root file
  if (histos_) {
    bool save = parameters_.getUntrackedParameter<bool>("SaveClientFile", true);
    if (save) {
      if (runType_ != sistrip::CALIBRATION_SCAN and runType_ != sistrip::CALIBRATION_SCAN_DECO) {
        if (runType_ != sistrip::DAQ_SCOPE_MODE)
          histos_->save(outputFileName_, runNumber_);
        else
          histos_->save(outputFileName_, runNumber_, partitionName_);
      } else {
        CalibrationHistograms* histo = dynamic_cast<CalibrationHistograms*>(histos_);
        histo->save(outputFileName_, runNumber_);
      }
    } else {
      edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                     << " Client file not saved!";
    }
  }

  // Virtual method to trigger the database upload
  uploadToConfigDb();

  // Print analyses
  if (histos_) {
    histos_->printAnalyses();
    histos_->printSummary();
  }

  // Remove all ME/CME objects
  if (histos_) {
    histos_->remove();
  }

  edm::LogVerbatim(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                 << " Finished analyzing root file(s)...";
}

// -----------------------------------------------------------------------------
//
void SiStripCommissioningOfflineClient::analyze(const edm::Event& event, const edm::EventSetup& setup) {
  if (!(event.id().event() % 10)) {
    LogTrace(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                           << " Empty event loop! User can kill job...";
  }
}

// -----------------------------------------------------------------------------
//
void SiStripCommissioningOfflineClient::endJob() {}

// -----------------------------------------------------------------------------
//
void SiStripCommissioningOfflineClient::createHistos(const edm::ParameterSet& pset, const edm::EventSetup& setup) {
  // Check pointer
  if (histos_) {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " CommissioningHistogram object already exists!"
                                << " Aborting...";
    return;
  }

  // Check pointer to BEI
  if (!bei_) {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " NULL pointer to DQMStore!";
    return;
  }

  // Create "commissioning histograms" object
  if (runType_ == sistrip::FAST_CABLING) {
    histos_ = new FastFedCablingHistograms(pset, bei_);
  } else if (runType_ == sistrip::FED_CABLING) {
    histos_ = new FedCablingHistograms(pset, bei_);
  } else if (runType_ == sistrip::APV_TIMING) {
    histos_ = new ApvTimingHistograms(pset, bei_);
  } else if (runType_ == sistrip::OPTO_SCAN) {
    histos_ = new OptoScanHistograms(pset, bei_);
  } else if (runType_ == sistrip::VPSP_SCAN) {
    histos_ = new VpspScanHistograms(pset, bei_);
  } else if (runType_ == sistrip::PEDESTALS) {
    histos_ = new PedestalsHistograms(pset, bei_);
  } else if (runType_ == sistrip::PEDS_FULL_NOISE) {
    histos_ = new PedsFullNoiseHistograms(pset, bei_);
  } else if (runType_ == sistrip::PEDS_ONLY) {
    histos_ = new PedsOnlyHistograms(pset, bei_);
  } else if (runType_ == sistrip::NOISE) {
    histos_ = new NoiseHistograms(pset, bei_);
  } else if (runType_ == sistrip::APV_LATENCY || runType_ == sistrip::FINE_DELAY) {
    histos_ = new SamplingHistograms(pset, bei_, runType_);
  } else if (runType_ == sistrip::CALIBRATION || runType_ == sistrip::CALIBRATION_DECO ||
             runType_ == sistrip::CALIBRATION_SCAN || runType_ == sistrip::CALIBRATION_SCAN_DECO) {
    histos_ = new CalibrationHistograms(pset, bei_, runType_);
  } else if (runType_ == sistrip::DAQ_SCOPE_MODE) {
    histos_ = new DaqScopeModeHistograms(pset, bei_);
  } else if (runType_ == sistrip::UNDEFINED_RUN_TYPE) {
    histos_ = nullptr;
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " Undefined run type!";
    return;
  } else if (runType_ == sistrip::UNKNOWN_RUN_TYPE) {
    histos_ = nullptr;
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " Unknown run type!";
    return;
  }
  histos_->configure(pset, setup);
}

// -----------------------------------------------------------------------------
//
void SiStripCommissioningOfflineClient::setInputFiles(std::vector<std::string>& files,
                                                      const std::string path,
                                                      const std::string partitionName,
                                                      uint32_t run_number,
                                                      bool collate_histos) {
  std::string runStr;
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(8) << run_number;
  runStr = ss.str();

  std::string nameStr = "";
  if (!collate_histos) {
    nameStr = "SiStripCommissioningClient_";
  } else {
    nameStr = "SiStripCommissioningSource_";
  }

  LogTrace("TEST") << " runStr " << runStr;

  // Open directory
  DIR* dp;
  struct dirent* dirp;
  if ((dp = opendir(path.c_str())) == nullptr) {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " Error locating directory \"" << path << "\". No such directory!";
    return;
  }

  // Find compatible files
  while ((dirp = readdir(dp)) != nullptr) {
    std::string fileName(dirp->d_name);
    bool goodName = (fileName.find(nameStr) != std::string::npos);
    bool goodRun = (fileName.find(runStr) != std::string::npos);
    bool rootFile = (fileName.find(".root") != std::string::npos);
    bool goodPartition = true;
    if (not partitionName.empty()) {
      goodPartition = (fileName.find(partitionName) != std::string::npos);
    }

    //bool rootFile = ( fileName.rfind(".root",5) == fileName.size()-5 );
    if (goodName && goodRun && rootFile && goodPartition) {
      std::string entry = path;
      entry += "/";
      entry += fileName;
      files.push_back(entry);
    }
  }
  closedir(dp);

  // Some debug
  if (!collate_histos && files.size() > 1) {
    std::stringstream ss;
    ss << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
       << " Found more than one client file!";
    std::vector<std::string>::const_iterator ifile = files.begin();
    std::vector<std::string>::const_iterator jfile = files.end();
    for (; ifile != jfile; ++ifile) {
      ss << std::endl << *ifile;
    }
    edm::LogError(mlDqmClient_) << ss.str();
  } else if (files.empty()) {
    edm::LogError(mlDqmClient_) << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
                                << " No input files found!";
  }
}


bool SiStripCommissioningOfflineClient::openDQMStore(std::string const& filename,
						     std::string const& motherdir /* ="" */,
						     const bool & fileMustExist /* =true */) {

  std::unique_ptr<TFile> f;
  try {
    f.reset(TFile::Open(filename.c_str()));
    if (!f.get() || f->IsZombie()){
      std::stringstream ss;
      ss << "[SiStripCommissioningOfflineClient::" << __func__ << "]"
	 << " Failed to open file "<< filename;
    }
  } 
  catch (std::exception&) {
    if (fileMustExist)
      throw;
    else 
      return false;
  }
  readDQMStoreDirectory(f.get(),motherdir);
  f->Close();
  return true;
}

int SiStripCommissioningOfflineClient::readDQMStoreDirectory(TFile* file,
							     std::string const & curdir){
  
  unsigned int ntot = 0;
  unsigned int count = 0;

  // enter in the current directory
  file->cd(curdir.c_str());
  
  TKey* key;
  TIter next(gDirectory->GetListOfKeys());
  std::vector<TObject*> delayed;
  std::vector<std::string> delayedPath;

  while ((key = (TKey*) next())) {
    std::unique_ptr<TObject> obj(key->ReadObj());
    if (dynamic_cast<TDirectory*>(obj.get())) {
      std::string nextdir = curdir;
      nextdir  += "/" + std::string(obj->GetName());
      ntot += readDQMStoreDirectory(file,nextdir);
    } 
    else if (dynamic_cast<TObjString*>(obj.get())){
      delayed.push_back(obj.release());
      delayedPath.push_back(curdir);
    }
    else {
      if (extractDQMStoreObject(obj.get(), curdir))
	++count;
    }
  }
  
  
  if(!delayed.empty()){
    size_t ipos = 0;
    for(auto & element : delayed){      
      if (extractDQMStoreObject(element, delayedPath.at(ipos)))
	++count;
      ipos++;
    }
    for(auto & element : delayed){
      delete element;
    }
    delayed.clear();
    delayedPath.clear();
  }

  return ntot + count;
}


bool SiStripCommissioningOfflineClient::extractDQMStoreObject(TObject* obj, std::string const& dir) {

  MonitorElementData::Path path; 
  std::string fullpath = dir + "/" + std::string(obj->GetName());
  path.set(fullpath, MonitorElementData::Path::Type::DIR_AND_NAME);
  MonitorElement* me = bei_->findME(path);

  if (auto* h = dynamic_cast<TProfile*>(obj)) {
    if (!me) me = bei_->bookProfile(fullpath,(TProfile*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TProfile2D*>(obj)) {
    if (!me) me = bei_->bookProfile2D(fullpath, (TProfile2D*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TH1F*>(obj)) {
    if (!me) me = bei_->book1D(fullpath, (TH1F*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TH1S*>(obj)) {
    if (!me) me =  bei_->book1S(fullpath, (TH1S*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TH1D*>(obj)) {
    if (!me) me =  bei_->book1DD(fullpath, (TH1D*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TH2F*>(obj)) {
    if (!me) me = bei_->book2D(fullpath, (TH2F*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TH2S*>(obj)) {
    if (!me) me = bei_->book2S(fullpath, (TH2S*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TH2D*>(obj)) {
    if (!me) me = bei_->book2DD(fullpath, (TH2D*)h->Clone());
  } 
  else if (auto* h = dynamic_cast<TH3F*>(obj)) {
    if (!me) me = bei_->book3D(fullpath, (TH3F*)h->Clone());
  }
  else if (dynamic_cast<TObjString*>(obj)) {
    lat::RegexpMatch m;
    lat::Regexp const s_rxmeval{"^<(.*)>(i|f|s|e|t|qr)=(.*)</\\1>$"};
    if (!s_rxmeval.match(obj->GetName(), 0, 0, &m)) {
      if (strstr(obj->GetName(), "CMSSW")) {
	return true;
      } else if (strstr(obj->GetName(), "DQMPATCH")) {
	return true;
      } else {
	edm::LogWarning(mlDqmClient_) << "*** DQMStore: WARNING: cannot extract object '" << obj->GetName() << "' of type '"
				      << obj->IsA()->GetName() << "'\n";
	return false;
      }
    }
    
    std::string label = m.matchString(obj->GetName(), 1);
    std::string kind  = m.matchString(obj->GetName(), 2);
    std::string value = m.matchString(obj->GetName(), 3);

    if (kind == "i") {
      if (!me) me = bei_->bookInt(dir+"/"+label);
      me->Fill(atoll(value.c_str()));
    }
    else if (kind == "f") {
      if (!me) me = bei_->bookFloat(dir+"/"+label);
      me->Fill(atof(value.c_str()));
    }
    else if (kind == "s") {
      if (!me) me = bei_->bookString(dir+"/"+label, value);
      me->Fill(value);
    } 
    else if (kind == "e") {
      if (!me) {
	edm::LogWarning(mlDqmClient_) << "*** DQMStore: WARNING: no monitor element '" << label << "' in directory '" << dir
				      << "' to be marked as efficiency plot.\n";
	return false;
      }
      me->setEfficiencyFlag();
    } 
    else {
      edm::LogWarning(mlDqmClient_) << "*** DQMStore: WARNING: cannot extract object '" << obj->GetName() << "' of type '"
				    << obj->IsA()->GetName() << "'\n";
      return false;
    }
  } 
  else if (auto* n = dynamic_cast<TNamed*>(obj)) {
    // For old DQM data.
    std::string s;
    s.reserve(6 + strlen(n->GetTitle()) + 2 * strlen(n->GetName()));
    s += '<';
    s += n->GetName();
    s += '>';
    s += n->GetTitle();
    s += '<';
    s += '/';
    s += n->GetName();
    s += '>';
    TObjString os(s.c_str());
    return extractDQMStoreObject(&os, dir);
  } 
  else {
    edm::LogWarning(mlDqmClient_) << "*** DQMStore: WARNING: cannot extract object '" << obj->GetName() << "' of type '"
				  << obj->IsA()->GetName() << "' and with title '" << obj->GetTitle() << "'\n";
    return false;
  }

  return true;
}
