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
#include "Hit.h"


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

  //TTree* GetTTree(std::string name);
  //void AddTTree(std::string name,TTree *tree);
  //void DeleteTTree(std::string name,TTree *tree);
  
  std::queue<TimeSlice*> pre_sort_queue;
  std::map<trigger_type, std::queue<TimeSlice*> > trigger_queues;

  // Describes which digitizers channels are enabled. Set by Digitizer, used by
  // Reformatter to sync channels data at the beginning of the readout.
  std::vector<uint16_t> enabled_digitizer_channels;

  // Readout of the digitizer data in the CAEN data format
  std::unique_ptr<std::list<std::unique_ptr<std::vector<Hit>>>> raw_readout;
  std::mutex raw_readout_mutex;

  // Readout reformatted in terms of timeslices and hits
  std::queue<std::unique_ptr<TimeSlice>> readout;
  std::mutex readout_mutex;

private:


  
  //std::map<std::string,TTree*> m_trees; 
  
  
  
};



#endif
