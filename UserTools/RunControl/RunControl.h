#ifndef RunControl_H
#define RunControl_H

#include <string>
#include <iostream>

#include "Tool.h"
#include "DataModel.h"

/**
 * \struct RunControl_args_args
 *
 * This is a struct to place data you want your thread to access or exchange with it. The idea is the datainside is only used by the threa\d and so will be thread safe
 *
 * $Author: B.Richards $
 * $Date: 2019/05/28 10:44:00 $
 */
/*
struct RunControl_args:Thread_args{

  RunControl_args();
  ~RunControl_args();
  boost::posix_time::ptime* start_time;
  unsigned long* current_coarse_counter;

};
*/

/**
 * \class RunControl
 *
 * This is a template for a Tool that produces a single thread that can be assigned a function seperate to the main thread. Please fill out the descripton and author information.
*
* $Author: B.Richards $
* $Date: 2019/05/28 10:44:00 $
*/

class RunControl: public Tool {


 public:

  RunControl(); ///< Simple constructor
  bool Initialise(std::string configfile,DataModel &data); ///< Initialise Function for setting up Tool resorces. @param configfile The path and name of the dynamic configuration file to read in. @param data A reference to the transient data class used to pass information between Tools.
  bool Execute(); ///< Executre function used to perform Tool perpose. 
  bool Finalise(); ///< Finalise funciton used to clean up resorces.


 private:

  //  static void Thread(Thread_args* arg); ///< Function to be run by the thread in a loop. Make sure not to block in it
  
  std::string m_configfile;
  Utilities* m_util;  ///< Pointer to utilities class to help with threading
  //  RunControl_args* args; ///< thread args (also holds pointer to the thread)
  
  std::string RunStart(const char* key);
  std::string RunStop(const char* key);
  std::string SubRun(const char* key);

  void LoadConfig();

  int m_config_update_time_sec;
  int m_start_delay;

  bool m_run_start;
  bool m_run_stop;
  bool m_new_sub_run;
  bool m_stopping;
  std::string m_run_description;
  

  boost::posix_time::ptime* m_start_time;
  boost::posix_time::time_duration m_period_new_sub_run;
  boost::posix_time::time_duration m_lapse;
  boost::posix_time::ptime m_config_start;
  boost::posix_time::time_duration m_period_reconfigure;
  
};


#endif
