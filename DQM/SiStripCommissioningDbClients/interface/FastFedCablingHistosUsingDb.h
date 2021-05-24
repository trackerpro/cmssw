
#ifndef DQM_SiStripCommissioningClients_FastFedCablingHistosUsingDb_H
#define DQM_SiStripCommissioningClients_FastFedCablingHistosUsingDb_H

#include "DQM/SiStripCommissioningDbClients/interface/CommissioningHistosUsingDb.h"
#include "DQM/SiStripCommissioningClients/interface/FastFedCablingHistograms.h"

class FastFedCablingHistosUsingDb : public CommissioningHistosUsingDb, public FastFedCablingHistograms {
public:
  FastFedCablingHistosUsingDb(const edm::ParameterSet& pset,
                              DQMStore*,
                              SiStripConfigDb* const,
                              edm::ESGetToken<TrackerTopology, TrackerTopologyRcd> tTopoToken);

  ~FastFedCablingHistosUsingDb() override;

  void addDcuDetIds() override;  // override

  void uploadConfigurations() override;

private:
  void update(SiStripConfigDb::FedConnectionsV&,
              SiStripConfigDb::FedDescriptionsRange,
              SiStripConfigDb::DeviceDescriptionsRange,
              SiStripConfigDb::DcuDetIdsRange);

  void update(SiStripConfigDb::FedDescriptionsRange);

  void create(SiStripConfigDb::AnalysisDescriptionsV&, Analysis) override;

  void connections(SiStripConfigDb::DeviceDescriptionsRange, SiStripConfigDb::DcuDetIdsRange);

  bool uploadFedDescription_;
};

#endif  // DQM_SiStripCommissioningClients_FastFedCablingHistosUsingDb_H
