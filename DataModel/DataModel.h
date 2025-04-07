#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <map>
#include <string>
#include <vector>
#include <queue>

//#include "TTree.h"

#include "Store.h"
#include "BoostStore.h"
#include "DAQDataModelBase.h"
#include "DAQLogging.h"
#include "DAQUtilities.h"
#include "TimeSlice.h"


#include <zmq.hpp>

/**
 * \class DataModel
 *
 * This class Is a transient data model class for your Tools within the ToolChain. If Tools need to comunicate they pass all data objects through the data model. There fore inter tool data objects should be deffined in this class. 
 *
 *
 * $Author: B.Richards $ 
 * $Date: 2019/05/26 18:34:00 $
 * Contact: b.richards@qmul.ac.uk
 *
 */

using namespace ToolFramework;

class DataModel : public DAQDataModelBase {


 public:
  
  DataModel(); ///< Simple constructor 

  bool run_start     = false;
  bool run_stop      = false;
  bool change_config = false;
  bool running;
  bool load_config;
  bool sub_run;

  boost::posix_time::ptime start_time;
  unsigned long run_number;
  unsigned long sub_run_number;
  unsigned int run_configuration;
  unsigned long readout_num;  
  
  unsigned int thread_num;
  unsigned int thread_cap;
  
  JobQueue job_queue;
  
  std::mutex monitoring_store_mtx;
  Store monitoring_store;
  

  //TTree* GetTTree(std::string name);
  //void AddTTree(std::string name,TTree *tree);
  //void DeleteTTree(std::string name,TTree *tree);
  
  // Describes which digitizers channels are enabled. Set by Digitizer, used by
  // Reformatter to sync channels data at the beginning of the readout.
  std::vector<uint16_t> enabled_digitizer_channels;

  // Readout of the digitizer data in the CAEN data format
  std::unique_ptr<std::list<std::unique_ptr<std::vector<Hit>>>> raw_readout;
  std::mutex raw_readout_mutex;

  // Readout reformatted in terms of timeslices and hits
  std::queue<std::unique_ptr<TimeSlice>> readout;
  std::mutex readout_mutex;

  std::queue<std::unique_ptr<TimeSlice>> sorted_readout;
  std::mutex sorted_readout_mutex;

  std::queue<std::unique_ptr<TimeSlice>> triggered_readout;
  std::mutex triggered_readout_mutex;

  std::queue<std::unique_ptr<TimeSlice>> final_readout;
  std::mutex final_readout_mutex;

  std::queue<std::unique_ptr<TimeSlice>> monitoring_readout;
  std::mutex monitoring_readout_mutex;

  
  std::vector<size_t> channel_hits;
  

private:


  
  //std::map<std::string,TTree*> m_trees; 
  
  
  
};



#endif
