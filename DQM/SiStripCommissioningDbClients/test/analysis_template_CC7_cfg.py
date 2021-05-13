import FWCore.ParameterSet.Config as cms
import os,sys,getopt,glob,cx_Oracle,subprocess

conn_str = os.path.expandvars("$CONFDB")
conn     = cx_Oracle.connect(conn_str)
e        = conn.cursor()
e.execute('select RUNMODE from run where runnumber = RUNNUMBER')
runmode = e.fetchall()
runtype = -1;
for result in runmode:
    runtype = int(result[0]);
conn.close()

process = cms.Process("SiStripCommissioningOfflineDbClient")
### de-activate the message logger for calibration runs --> to big logs because of fit in root
if runmode != 3 and runmode != 33: 
    process.load("DQM.SiStripCommon.MessageLogger_cfi")
process.load("DQM.SiStripCommon.DaqMonitorROOTBackEnd_cfi")

process.load("OnlineDB.SiStripConfigDb.SiStripConfigDb_cfi")
process.SiStripConfigDb.UsingDb = True                                            # true means use database (not xml files)
process.SiStripConfigDb.ConfDb  = 'overwritten/by@confdb'                         # database connection account ( or use CONFDB env. var.)
process.SiStripConfigDb.Partitions.PrimaryPartition.PartitionName = 'DBPART'      # database partition (or use ENV_CMS_TK_PARTITION env. var.)
process.SiStripConfigDb.Partitions.PrimaryPartition.RunNumber     = RUNNUMBER     # specify run number ("0" means use major/minor versions, which are by default set to "current state")
process.SiStripConfigDb.TNS_ADMIN = '/etc'                                        # location of tnsnames.ora, needed at P5, not in TAC
#process.SiStripConfigDb.Partitions.PrimaryPartition.ForceCurrentState = cms.untracked.bool(True)

process.source = cms.Source("EmptySource") 
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(2) ) 

process.load("Geometry.CMSCommonData.cmsIdealGeometryXML_cfi")
process.load("Geometry.TrackerNumberingBuilder.trackerTopology_cfi")
process.load("Geometry.TrackerGeometryBuilder.trackerParameters_cfi")

process.load("DQM.SiStripCommissioningDbClients.OfflineDbClient_cff")
process.db_client.FilePath         = cms.untracked.string('DATALOCATION')
process.db_client.RunNumber        = cms.untracked.uint32(RUNNUMBER)
process.db_client.UseClientFile    = cms.untracked.bool(CLIENTFLAG)
process.db_client.UploadHwConfig   = cms.untracked.bool(DBUPDATE)
process.db_client.UploadAnalyses   = cms.untracked.bool(ANALUPDATE)
process.db_client.DisableDevices   = cms.untracked.bool(DISABLEDEVICES)
process.db_client.DisableBadStrips = cms.untracked.bool(DISABLEBADSTRIPS)
process.db_client.SaveClientFile   = cms.untracked.bool(SAVECLIENTFILE)
if runtype == 15: ## only needed for spy-channel
    process.db_client.PartitionName    = cms.string("DBPART")

process.db_client.ApvTimingParameters.SkipFecUpdate = cms.bool(True)
process.db_client.ApvTimingParameters.SkipFedUpdate = cms.bool(False)
process.db_client.ApvTimingParameters.TargetDelay = cms.int32(-1)

process.db_client.OptoScanParameters.SkipGainUpdate = cms.bool(False)

process.db_client.CalibrationParameters.targetRiseTime     = cms.double(50);
process.db_client.CalibrationParameters.targetDecayTime    = cms.double(90);
process.db_client.CalibrationParameters.tuneSimultaneously = cms.bool(True);

process.db_client.PedestalsParameters.KeepStripsDisabled = cms.bool(True)

process.db_client.DaqScopeModeParameters.DisableBadStrips =  cms.bool(False)
process.db_client.DaqScopeModeParameters.KeepStripsDisabled = cms.bool(True)
process.db_client.DaqScopeModeParameters.SkipPedestalUpdate = cms.bool(False)
process.db_client.DaqScopeModeParameters.SkipTickUpdate = cms.bool(False)

process.p = cms.Path(process.db_client)
