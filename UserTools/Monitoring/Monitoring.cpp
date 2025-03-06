#include "Monitoring.h"

Monitoring_args::Monitoring_args():Thread_args(){}

Monitoring_args::~Monitoring_args(){}


Monitoring::Monitoring():Tool(){}


bool Monitoring::Initialise(std::string configfile, DataModel &data){

  InitialiseTool(data);
  m_configfile=configfile;
  InitialiseConfiguration(configfile);
  //m_variables.Print();



  m_util=new Utilities();
  args=new Monitoring_args();
  args->last =  boost::posix_time::microsec_clock::universal_time();
  args->data = m_data;
  args->last2 =  boost::posix_time::microsec_clock::universal_time();
  args->period2 =  boost::posix_time::seconds(1);
  
  LoadConfig();

  
  m_util->CreateThread("test", &Thread, args);

  ExportConfiguration();
  
  return true;
}


bool Monitoring::Execute(){

  if(m_data->change_config){
    InitialiseConfiguration(m_configfile);
    LoadConfig();
    ExportConfiguration();
  }
  //if(m_data->run_start){
    
    //    for(std::map<std::string, unsigned int>::iterator it= args->data->hit_map.begin(); it!=args->data->hit_map.end(); it++){
    //  args->data->hit_map[it->first]=0; 
      
  // }
  // }
 
  std::stringstream tmp;
  std::string runinfo="";
  unsigned long part=0;
  unsigned long workers=0;
  m_data->vars.Get("Runinfo",runinfo);
  m_data->vars.Get("part",part);
  m_data->monitoring_store.Get("pool_threads",workers);
  
  tmp<< runinfo<<" buffers: unsorted| sorted| triggered| readout = "<<m_data->readout.size()<<"| "<<m_data->sorted_readout.size()<<"| "<<m_data->triggered_readout.size()<<"| "<<" (files="<<part<<") jobs:workers = "<<m_data->job_queue.size()<<":"<<workers;
  m_data->vars.Set("Status",tmp.str());
  
  return true;
}


bool Monitoring::Finalise(){

  m_util->KillThread(args);

  delete args;
  args=0;

  delete m_util;
  m_util=0;

  return true;
}

void Monitoring::Thread(Thread_args* arg){
  
  Monitoring_args* args=reinterpret_cast<Monitoring_args*>(arg);

  /*
  
  args->lapse2 = args->period2 -( boost::posix_time::microsec_clock::universal_time() - args->last2);
  
  if(args->lapse.is_negative() ){
    unsigned int tmp=0;
   
    for(std::map<std::string, unsigned int>::iterator it= args->data->hit_map.begin(); it!=args->data->hit_map.end(); it++){
      args->hit_rates.Get(it->first,tmp);
      tmp = it->second - tmp;
      args->hit_rates.Set(it->first,tmp);
      //      args->hit_rates.Print();
    }
    std::string json="";
    args->hit_rates>>json;

    args->data->services->SendMonitoringData(json,"hit_rates");
    
    args->last2 = boost::posix_time::microsec_clock::universal_time();
  
  }
  */
  
  args->lapse = args->period -( boost::posix_time::microsec_clock::universal_time() - args->last);
  //std::cout<< m_lapse<<std::endl;
  
  if(!args->lapse.is_negative() ){
    usleep(100);
    return;
  }
    //printf("in runstart lapse\n");
    
    std::string json="";
    Store tmp;
    for(unsigned int i=0; i<args->data->channel_hits.size(); i++){

      tmp.Set(std::to_string(i),(unsigned int)args->data->channel_hits.at(i));

    }
    tmp>>json;
    //    args->data->monitoring_store_mtx.lock();
    //args->data->monitoring_store>>json;
    //args->data->monitoring_store_mtx.unlock();
    args->data->services->SendMonitoringData(json);


    /// send data to monitoring toolcahin
    
    
    args->last = boost::posix_time::microsec_clock::universal_time();
    
  
}

bool Monitoring::LoadConfig(){

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;
  unsigned int period_sec=0;
  if(!m_variables.Get("period_sec",period_sec)) period_sec=60;
  args->period = boost::posix_time::seconds(period_sec);
  
  
  return true;

}
