#!/bin/bash
source /cvmfs/cms.cern.ch/cmsset_default.sh
cd /cvmfs/cms.cern.ch/el9_amd64_gcc12/cms/cmssw/CMSSW_13_3_2/src/
eval `scramv1 runtime -sh`
cd -
chmod 755 NetScopeStandaloneDat2Root
chmod 755 add_branches_TimingDAQ.py
cp /eos/uscms/store/group/cmstestbeam/2024_05_SNSPD_FCFD_ETL/LecroyScope/RecoData/ConversionRECO/converted_run$1.root .
cp /eos/uscms/store/group/cmstestbeam/2024_05_SNSPD_FCFD_ETL/Tracks/v1/Run$1_CMSTiming_FastTriggerStream_converted.root .
cp /eos/uscms//store/group/cmstestbeam/2024_05_SNSPD_FCFD_ETL//ConfigInfo/Runs/info_$1.json .
ls
./NetScopeStandaloneDat2Root --input_file=converted_run$1.root --pixel_input_file=/eos/uscms//store/group/cmstestbeam/2024_05_SNSPD_FCFD_ETL/Tracks/v1/Run$1_CMSTiming_FastTriggerStream_converted.root  --config=LecroyScope_v11.config --output_file=out_run$1.root --save_meas --correctForTimeOffsets=true
python3 add_branches_TimingDAQ.py $1 9999 out_run$1.root
ls
xrdcp -fs out_run$1_info.root root://cmseos.fnal.gov//store/group/cmstestbeam/2024_05_SNSPD_FCFD_ETL/LecroyScope/RecoData/TimingDAQRECO/RecoWithTracks/v11//run$1_info.root
rm *$1*.root
rm info_$1.json
date
