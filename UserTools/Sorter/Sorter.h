#ifndef Sorter_H
#define Sorter_H

#include <string>
#include <iostream>

#include "Tool.h"
#include <DataModel.h>

using namespace ToolFramework;

/**
 * \struct Sorter_args
 *
 * This is a struct to place data you want your thread to acess or exchange with it. The idea is the datainside is only used by the threa\
d and so will be thread safe
*
* $Author: B.Richards $
* $Date: 2019/05/28 10:44:00 $
*/

struct Sorter_args:Thread_args{

  Sorter_args();
  ~Sorter_args();
  DataModel* m_data;
  std::queue<std::unique_ptr<TimeSlice>>* readout;
  std::mutex* readout_mutex;
  std::queue<std::unique_ptr<TimeSlice>>* sorted_readout;
  std::mutex* sorted_readout_mutex;
  std::queue<std::unique_ptr<TimeSlice>> in_progress;
  std::unique_ptr<TimeSlice> time_slice;
  
};

/**
 * \class Sorter
 *
 * This is a template for a Tool that dynamically more or less threads, such that there is always 1 available thread.This can therefore be used to scale to your worklaod, however be carefull when using more than one of these tools and to apply upperlimits if necessary both locally within this tool and globally so that more threads than is practical are created causing massive inefficency. Please fill out the descripton and author information.
 *
 * $Author: B.Richards $
 * $Date: 2019/05/28 10:44:00 $
 */

class Sorter: public Tool {


 public:

  Sorter(); ///< Simple constructor
  bool Initialise(std::string configfile,DataModel &data); ///< Initialise Function for setting up Tool resorces. @param configfile The path and name of the dynamic configuration file to read in. @param data A reference to the transient data class used to pass information between Tools.
  bool Execute(); ///< Executre function used to perform Tool perpose. 
  bool Finalise(); ///< Finalise funciton used to clean up resorces.


 private:

  void CreateThread(); ///< Function to Create Thread
  void DeleteThread(int pos); ///< Function to delete thread @param pos is the position in the args vector below

  static void Thread(Thread_args* arg); ///< Function to be run by the thread in a loop. Make sure not to block in it
  Utilities* m_util; ///< Pointer to utilities class to help with threading
  std::vector<Sorter_args*> args; ///< Vector of thread args (also holds pointers to the threads)
  int m_freethreads; ///< Keeps track of free threads
  unsigned long m_threadnum; ///< Counter for unique naming of threads

  static bool SortData(void* data);
  static void FailSort(void* data);
  
};


#endif
