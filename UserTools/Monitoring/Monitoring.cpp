#include "Monitoring.h"

int parseLine(char* line){
  // This assumes that a digit will be found and the line ends in " Kb".
  int i = strlen(line);
  const char* p = line;
  while (*p <'0' || *p > '9') p++;
  line[i-3] = '\0';
  i = atoi(p);
  return i;
}

Monitoring_args::Monitoring_args():Thread_args(){

  sock=0;
}

Monitoring_args::~Monitoring_args(){

  delete sock;
  sock=0;
}


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
  args->monitoring_readout_mutex = &(m_data->monitoring_readout_mutex);
  args->monitoring_readout = &(m_data->monitoring_readout);
  args->sock = new zmq::socket_t(*(m_data->context), ZMQ_PUB);
  args->sock->bind("tcp://*:5656");
  
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
  
  struct sysinfo memInfo;
  sysinfo (&memInfo);
  /* 
     mem= (((float) memInfo.totalram - (float)memInfo.freeram)/((float)memInfo.totalram)) * 100;
  */

  //// Get mem
  FILE* file2 = fopen("/proc/self/status", "r");
  int result = -1;
  char line[128];
  
  while (fgets(line, 128, file2) != NULL){
    if (strncmp(line, "VmRSS:", 6) == 0){
      result = parseLine(line);
      break;
    }
  }
  fclose(file2);
  
  mem= (((float)result * 1000)/ ((float) memInfo.totalram))*100;
  while(mem > 100.0) mem= mem/1000.0;
  
  
  // get cpu
  float percent;
  FILE* file;
  unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;

  file = fopen("/proc/stat", "r");
  fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow, &totalSys, &totalIdle);
  fclose(file);
  
  if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
      totalSys < lastTotalSys || totalIdle < lastTotalIdle){
    //Overflow detection. Just skip this value.
    cpu = -1.0;
  }
  else{
    total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
      (totalSys - lastTotalSys);
    cpu = total;
    total += (totalIdle - lastTotalIdle);
    cpu /= total;
    cpu *= 100;
  }
  
  lastTotalUser = totalUser;
  lastTotalUserLow = totalUserLow;
  lastTotalSys = totalSys;
  lastTotalIdle = totalIdle;
  
  m_data->monitoring_store_mtx.lock();
  m_data->monitoring_store.Set("cpu",cpu);
  m_data->monitoring_store.Set("mem",mem);
  m_data->monitoring_store_mtx.unlock();
    
  std::stringstream tmp;
  std::string runinfo="";
  unsigned long part=0;
  unsigned long workers=0;
  m_data->vars.Get("Runinfo",runinfo);
  m_data->vars.Get("part",part);
  m_data->monitoring_store.Get("pool_threads",workers);
  
  tmp<< runinfo<<" buffers: unsorted| sorted| triggered| readout = "<<m_data->readout.size()<<"| "<<m_data->sorted_readout.size()<<"| "<<m_data->triggered_readout.size()<<"| "<<" (files="<<part<<") jobs:workers = "<<m_data->job_queue.size()<<":"<<workers<<" mem="<<mem<<"% cpu="<<cpu<<"%";;
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

    args->data->monitoring_store_mtx.lock();
    args->data->monitoring_store>>json;
    args->data->monitoring_store_mtx.unlock();
    args->data->services->SendMonitoringData(json);

    args->monitoring_readout_mutex->lock();
    if(args->monitoring_readout->size()) std::swap(*args->monitoring_readout, args->in_progress);
    args->monitoring_readout_mutex->unlock();

    for( size_t i=0; i <args->in_progress.size(); i++){
      args->in_progress.front()->Send(args->sock);
      args->in_progress.pop();
    }
    
    args->last = boost::posix_time::microsec_clock::universal_time();
    
  
}

bool Monitoring::LoadConfig(){

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;
  unsigned int period_sec=0;
  if(!m_variables.Get("period_sec",period_sec)) period_sec=60;
  args->period = boost::posix_time::seconds(period_sec);
  
  
  return true;

}
