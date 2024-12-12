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

  //TTree* GetTTree(std::string name);
  //void AddTTree(std::string name,TTree *tree);
  //void DeleteTTree(std::string name,TTree *tree);
  
  std::queue<TimeSlice*> pre_sort_queue;
  std::map<trigger_type, std::queue<TimeSlice*> > trigger_queues;
  std::queue<TimeSlice*> read_out_queue;

private:


  
  //std::map<std::string,TTree*> m_trees; 
  
  
  
};



#endif
