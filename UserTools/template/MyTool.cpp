#include "MyTool.h"

MyTool::MyTool():Tool(){}


bool MyTool::Initialise(std::string configfile, DataModel &data){
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  ExportConfiguration();
  return true;
}


bool MyTool::Execute(){

  return true;
}


bool MyTool::Finalise(){

  return true;
}
