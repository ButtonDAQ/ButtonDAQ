#include "Configuration.h"

Configuration_args::Configuration_args():Thread_args(){}

Configuration_args::~Configuration_args(){}


Configuration::Configuration():Tool(){}


bool Configuration::Initialise(std::string configfile, DataModel &data){
  
  InitialiseTool(data);
  
  m_configfile=configfile;
  
  InitialiseConfiguration(m_configfile);
  ExportConfiguration();
  
  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;
  
  m_data->change_config=false;
  m_data->load_config=false;
  m_data->run_configuration=0;  // test run configuration
  
  // m_util=new Utilities();
  // args=new Configuration_args();
  
  //m_util->CreateThread("test", &Thread, args);

  return true;
}


bool Configuration::Execute(){

  if(m_data->change_config)  m_data->change_config=false;
  
  if(m_data->load_config){

    if(!LoadConfig()) return false;
    
    m_data->load_config=false;
    m_data->change_config=true;
  }
    

  return true;
}


bool Configuration::Finalise(){

  //m_util->KillThread(args);

  //  delete args;
  //args=0;

  //delete m_util;
  //m_util=0;

  return true;
}

void Configuration::Thread(Thread_args* arg){

  Configuration_args* args=reinterpret_cast<Configuration_args*>(arg);

}

bool Configuration::LoadConfig(){
  
  //m_data->services->SendLog("Loading configuration "+std::to_string(m_data->run_configuration)+" from DB" , 1);
  Log("Loading configuration "+std::to_string(m_data->run_configuration)+" from DB" , v_message);
  std::string config_json="";
  if(m_data->services->GetRunDeviceConfig(config_json, m_data->run_configuration)){
    
    m_data->vars.JsonParser(config_json);
    
    InitialiseConfiguration(m_configfile);
    if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;
    ExportConfiguration();
    
    /*
    std::clog<<m_tool_name<<"::LoadConfig got updated m_variables:\n======"<<std::endl;
    m_variables.Print();
    std::clog<<"======="<<std::endl;
    */
    
    
  } else {
    
    m_data->services->SendLog("ERROR "+m_tool_name+": Failed to load config from DB with error '"+config_json+"'" , v_error);
    bool ok = m_data->services->SendAlarm("ERROR "+m_tool_name+": Failed to load config from DB with error '"+config_json+"'");
    return false;
    
  }
  
  return true;
}
