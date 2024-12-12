#include "DataWriter.h"

DataWriter_args::DataWriter_args():Thread_args(){}

DataWriter_args::~DataWriter_args(){}


DataWriter::DataWriter():Tool(){}


bool DataWriter::Initialise(std::string configfile, DataModel &data){
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  m_util=new Utilities();
  args=new DataWriter_args();
  
  m_util->CreateThread("test", &Thread, args);

  ExportConfiguration();
  return true;
}


bool DataWriter::Execute(){

  return true;
}


bool DataWriter::Finalise(){

  m_util->KillThread(args);

  delete args;
  args=0;

  delete m_util;
  m_util=0;

  return true;
}

void DataWriter::Thread(Thread_args* arg){

  DataWriter_args* args=reinterpret_cast<DataWriter_args*>(arg);

}
