
#ifndef DQM_SiStripCommissioningClients_PedsOnlyHistosUsingDb_H
#define DQM_SiStripCommissioningClients_PedsOnlyHistosUsingDb_H
#include "DQM/SiStripCommissioningDbClients/interface/CommissioningHistosUsingDb.h"

#include "DQM/SiStripCommissioningClients/interface/PedsOnlyHistograms.h"

class PedsOnlyHistosUsingDb : public CommissioningHistosUsingDb, public PedsOnlyHistograms {
public:
  PedsOnlyHistosUsingDb(const edm::ParameterSet& pset,
                        DQMStore*,
                        SiStripConfigDb* const,
                        edm::ESGetToken<TrackerTopology, TrackerTopologyRcd> tTopoToken);

  ~PedsOnlyHistosUsingDb() override;

  void uploadConfigurations() override;

private:

  float highThreshold_; // higher threshold for the zero suppression                                                                                                                                  
  float lowThreshold_;  // lower threshold for the zero suppression                                                                                                                                   
  uint32_t pedshift_;

  void update(SiStripConfigDb::FedDescriptionsRange);

  void create(SiStripConfigDb::AnalysisDescriptionsV&, Analysis) override;
};

#endif  // DQM_SiStripCommissioningClients_PedsOnlyHistosUsingDb_H
