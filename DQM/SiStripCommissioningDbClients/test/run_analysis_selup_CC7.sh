#!/bin/bash

#  ./run_analysis.sh 

#### Options
#    1: <run_number>
#    2: <HWUPLOAD> --> upload for conditions
#    3: <AnalysisUpload> --> analysis upload
#    4: <DB_PARTITION> --> database partition
#    5: <UseClientFile> --> use client file
#    6: <DisableDevices> 
#    7: <saveClientFile> --> save client file
#    8: <run the edm conversion> --> perform the re-packing at the same time

# tests on env
if [ -n "`uname -n | grep vmepc`" -a "`whoami`" == "trackerpro" ] ; then
    :
elif [ -n "`uname -n | grep srv-s2b17-29-01`" -a "`whoami`" == "trackerpro" ] ; then
    :
elif [ -n "`uname -n | grep srv-s2b17-30-01`" -a "`whoami`" == "trackerpro" ] ; then
    :
elif [ -n "`uname -n | grep cmstracker029`" -a "`whoami`" == "xdaqtk" ] ; then
    :
elif [ -n "`uname -n | grep cmstracker040`" -a "`whoami`" == "xdaqtk" ] ; then
    :
else
    echo "You are not running as trackerpro (on vmepcs) or xdaqtk (on cmstracker029, cmstracker040).";
    echo "This can cause problems during file moving, like loss of raw data.";
    echo "You don't want that to happen, probably, so please login as the appropriate user and try again.";
    exit 0;
fi

#remove lock file
function cleanup() {
    if [ -f ${DATALOC}/lock_${DBPARTITIONNAME}_${RUNNUMBER}.txt ]; then
	rm -f ${DATALOC}/lock_${DBPARTITIONNAME}_${RUNNUMBER}.txt
    fi
}
# clean up if killed
trap 'cleanup; exit' SIGTERM SIGHUP SIGINT SIGQUIT

# tests for proper usage
function usage() {
    echo "Usage:  ./run_analysis.sh <run_number> <HWUPLOAD> <AnalysisUpload> <DB_partition_name> <UseClientFile> <DisableDevices> <saveClientFile> <convertEDMFile>"
    echo "  run_number        = run number"
    echo "  HWUpload          = set to true if you want to upload the HW config to the DB"
    echo "  AnalysisUpload    = set to true if you want to upload the analysis results to the DB"
    echo "  DB_partition_name = Name of the corresponding DB partition"
    echo "  UseClientFile     = set to true if you want to analyze an existing client file rather than the source file(s)"
    echo "  DisableDevices    = set to true if you want to disable devices in the DB (normally set False)"
    echo "  saveClientFile    = set to true if you want to write the client file to disk (normally set True)"
    echo "  runEDMConversion  = set to true if you want to run the re-packing step (normally set False)"
}
if [ $# -eq 0 ]; then
    usage
    exit 0
fi
if [ $1 = "usage" ]; then
    usage
    exit 0
fi
if [ $# -lt 6 ]; then
    echo "Not enough arguments specified!"
    usage
    exit 0
fi

### Re-packing of RAW DAQ files
CONVERTEDMFILE="False"
if [ $# -gt 7 ]; then
    CONVERTEDMFILE=$8
fi

# Parse command line parameters
RUNNUMBER=$1
HWUPLOAD=$2         ; HWUPLOAD=`echo $HWUPLOAD | tr 'ft' 'FT'`
ANALYSISUPLOAD=$3   ; ANALYSISUPLOAD=`echo $ANALYSISUPLOAD | tr 'ft' 'FT'`
DBPARTITIONNAME=$4
USECLIENTFILE=$5    ; USECLIENTFILE=`echo $USECLIENTFILE | tr 'ft' 'FT'`
DISABLEDEVICES=$6   ; DISABLEDEVICES=`echo $DISABLEDEVICES | tr 'ft' 'FT'`
DISABLEBADSTRIPS="False"
SAVECLIENTFILE=$7   ; SAVECLIENTFILE=`echo $SAVECLIENTFILE | tr 'ft' 'FT'`
CONVERTEDMFILE=`echo $CONVERTEDMFILE | tr 'ft' 'FT'`

# Settings for basic directories
BASEDIR=/opt/cmssw
echo "  CMSSW base directory     : "$BASEDIR
DATALOC=/opt/cmssw/Data
echo "  Temporary storage area   : "$DATALOC
SCRATCH=$BASEDIR/Data/$RUNNUMBER
echo "  Output storage directory : "$SCRATCH
TEMPLATEPY=/opt/cmssw/scripts/selectiveupload_template_CC7.py
echo "  Analysis template        : "$TEMPLATEPY
echo "  ConfDB account           : "$CONFDB
echo "  CONVERTEDMFILE           : "$CONVERTEDMFILE

if [ -f ${DATALOC}/lock_${DBPARTITIONNAME}_${RUNNUMBER}.txt ]; then
    echo "Analysis is already running for this run number! Check /opt/cmssw/Data/lock_DBPARTITIONNAME_RUNNUMBER.txt"
    exit 0
fi

# set up CMSSW environment
source $BASEDIR/scripts/setup_CC7.sh
cd /opt/cmssw/Stable/2019/CMSSW_10_6_0_pre1/src
eval `scram runtime -sh`

# make the output storage directory if it does not already exist
if [ ! -d $SCRATCH ]; then
  mkdir -p $SCRATCH
fi

# make the analysis config file to run
sed 's,RUNNUMBER,'$RUNNUMBER',g' $TEMPLATEPY \
  | sed 's,DBUPDATE,'$HWUPLOAD',g' \
  | sed 's,ANALUPDATE,'$ANALYSISUPLOAD',g' \
  | sed 's,DBPART,'$DBPARTITIONNAME',g' \
  | sed 's,CLIENTFLAG,'$USECLIENTFILE',g' \
  | sed 's,DATALOCATION,'$SCRATCH',g' \
  | sed 's,DISABLEDEVICES,'$DISABLEDEVICES',g' \
  | sed 's,DISABLEBADSTRIPS,'$DISABLEBADSTRIPS',g' \
  | sed 's,SAVECLIENTFILE,'$SAVECLIENTFILE',g' \
  > $SCRATCH/analysis_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py

# create a cfg to run the source step again
cat $BASEDIR/scripts/sourcefromraw_template_CC7_cfg.py \
  | sed 's,RUNNUMBER,'$RUNNUMBER',g' \
  | sed 's,DBPART,'$DBPARTITIONNAME',g' \
  > $SCRATCH/sourcefromraw_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py

# create a cfg to run the source step again on EDM files
cat $BASEDIR/scripts/sourcefromedm_template_CC7_cfg.py \
  | sed 's,RUNNUMBER,'$RUNNUMBER',g' \
  | sed 's,DBPART,'$DBPARTITIONNAME',g' \
  > $SCRATCH/sourcefromedm_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py

# create a cfg to convert RAW files to EDM files
cat $BASEDIR/scripts/conversion_template_CC7_cfg.py \
  | sed 's,RUNNUMBER,'$RUNNUMBER',g' \
  | sed 's,DBPART,'$DBPARTITIONNAME',g' \
  > $SCRATCH/conversion_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py

# run the analysis!
source /opt/trackerDAQ/config/oracle.env.bash
cd $SCRATCH
DATESUFF=`date +%s`  # to get consistent dates across logfiles

# run the source step if the source file does not exist
SOURCEEXISTS=False
SOURCEFILELIST=(`ls ./ | grep \`printf %08u $RUNNUMBER\` | grep Source`)
COUNT=0
REF=1
ZERO=0
for i in ${SOURCEFILELIST[*]}; do
    let COUNT++;
done

COUNT_TOTAL_PARTITION=0;
COUNT_CURRENT_PARTITION=0;
for SOURCEFILE in "${SOURCEFILELIST[@]}"; do
    if echo $SOURCEFILE | grep "_TM_"; then
	let COUNT_TOTAL_PARTITION++;
    fi	
    if echo $SOURCEFILE | grep "_TP_"; then
	let COUNT_TOTAL_PARTITION++;
    fi	
    if echo $SOURCEFILE | grep "_TI_"; then
	let COUNT_TOTAL_PARTITION++;
    fi	
    if echo $SOURCEFILE | grep "_TO_"; then
	let COUNT_TOTAL_PARTITION++;
    fi	
    
    if echo $SOURCEFILE | grep "${DBPARTITIONNAME}"; then
	let COUNT_CURRENT_PARTITION++;
    fi

done

echo "  COUNT SOURCE FILES",$COUNT," COUNT PARTITION",$COUNT_TOTAL_PARTITION," COUNT CURRENT PARTITION ",$COUNT_CURRENT_PARTITION

if [ $COUNT_TOTAL_PARTITION -eq 0 ]; then
    if [ $COUNT -eq $REF ]; then
	echo "  One source file found"
        SOURCEEXISTS=True
     elif [ $COUNT -lt $REF ]; then
	echo "  No source file/s found --> generate it"
     else
	echo "  More than one source file --> please delete and resubmit"
     fi
else
    if [ $COUNT_CURRENT_PARTITION -eq $REF ]; then
	echo "  One source file found"
	SOURCEEXISTS=True
    else
	echo "  No source file/s found --> generate it"
    fi    
fi

if [ $SOURCEEXISTS = False ] ; then
  echo "  Running the source step ... "
  EDMFILE=/raid/fff/run${RUNNUMBER}/run${RUNNUMBER}.root
  if [ -f $EDMFILE ] ; then
      echo "  Running on the EDM file ... "
      cmsRun sourcefromedm_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py
  elif [ $CONVERTEDMFILE = True ]; then
      echo "  Conversion into an EDM file "
      cmsRun conversion_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py
      echo "  Running on the EDM files ... " 
      cmsRun sourcefromedm_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py
      echo "  Checking if RAW files can be deleted ... "
      python /opt/cmssw/scripts/rawdeleter.py ${RUNNUMBER}
  else
      echo "  Running on the RAW files ... "
      cmsRun sourcefromraw_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py
  fi
  mv /tmp/SiStripCommissioningSource*$RUNNUMBER*.root ./
  echo "  Running analysis step ... "
else 
  echo "  Source file found ... proceeding to the analysis step  "
fi

echo "Running analysis script ..." > ${DATALOC}/lock_${DBPARTITIONNAME}_${RUNNUMBER}.txt

cmsRun analysis_${DBPARTITIONNAME}_${RUNNUMBER}_cfg.py > analysis_${DBPARTITIONNAME}_${RUNNUMBER}_${DATESUFF}.cout
echo "done."
mv debug.log   analysis_${DBPARTITIONNAME}_${RUNNUMBER}_${DATESUFF}.debug.log
mv info.log    analysis_${DBPARTITIONNAME}_${RUNNUMBER}_${DATESUFF}.info.log
mv warning.log analysis_${DBPARTITIONNAME}_${RUNNUMBER}_${DATESUFF}.warning.log
mv error.log   analysis_${DBPARTITIONNAME}_${RUNNUMBER}_${DATESUFF}.error.log
echo "  Please check the output  : ${SCRATCH}"/analysis_${DBPARTITIONNAME}_${RUNNUMBER}_${DATESUFF}.*

# mv client file to the output directory
if [ $USECLIENTFILE = False ] && [ $SAVECLIENTFILE = True ] ; then
  CLIENTFILE=$(ls /tmp | grep $RUNNUMBER | grep Client)
  if [ -n "$CLIENTFILE" ] ; then
    echo -n "  Moving the client file from /tmp ... "
    mv /tmp/$CLIENTFILE $SCRATCH
    echo " done "
  else
    echo "No client file found to copy!"
  fi
fi

rm ${DATALOC}/lock_${DBPARTITIONNAME}_${RUNNUMBER}.txt
