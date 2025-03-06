#include "Trigger.h"

Trigger_args::Trigger_args():Thread_args(){}

Trigger_args::~Trigger_args(){}


Trigger::Trigger():Tool(){}


bool Trigger::Initialise(std::string configfile, DataModel &data){
  InitialiseTool(data);
  m_configfile=configfile;
  InitialiseConfiguration(configfile);

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  m_util=new Utilities();

  m_threadnum=0;
  CreateThread();
  
  m_freethreads=1;
  
  ExportConfiguration();
  return true;
}


bool Trigger::Execute(){
  
  if(m_data->change_config){
    InitialiseConfiguration(m_configfile);

    if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;
    if(!m_variables.Get("threashold",threashold)) threashold=20;
    if(!m_variables.Get("window_size",window_size)) window_size=200;
    if(!m_variables.Get("jump",jump)) jump=2000;
    
    ExportConfiguration();
  }
  
  return true;
}


bool Trigger::Finalise(){

  for(unsigned int i=0;i<args.size();i++) m_util->KillThread(args.at(i));
  
  args.clear();
  
  delete m_util;
  m_util=0;

  return true;
}

void Trigger::CreateThread(){

  Trigger_args* tmp_args=new Trigger_args();
  tmp_args->m_data = m_data;
  tmp_args->sorted_readout = &(m_data->sorted_readout);
  tmp_args->sorted_readout_mutex = &(m_data->sorted_readout_mutex);
  tmp_args->triggered_readout = &(m_data->triggered_readout);
  tmp_args->triggered_readout_mutex = &(m_data->triggered_readout_mutex);
  tmp_args->trigger_channels = &trigger_channels;
  tmp_args->threashold = &threashold;
  tmp_args->window_size = &window_size;
  tmp_args->jump = &jump;
  args.push_back(tmp_args);
  
  std::stringstream tmp;
  tmp<<"T"<<m_threadnum;

  m_util->CreateThread(tmp.str(), &Thread, args.at(args.size()-1));
  m_threadnum++;
  m_data->thread_num++;
  
}

 void Trigger::DeleteThread(int pos){

   m_util->KillThread(args.at(pos));
   delete args.at(pos);
   args.at(pos)=0;
   args.erase(args.begin()+(pos-1));

 }

void Trigger::Thread(Thread_args* arg){

  Trigger_args* args=reinterpret_cast<Trigger_args*>(arg);

  args->sorted_readout_mutex->lock();
  if(!args->sorted_readout->size()){
    args->sorted_readout_mutex->unlock();
    usleep(100);
    return;
  }
  //  printf("l1\n");  
  std::swap(*args->sorted_readout, args->in_progress);
  args->sorted_readout_mutex->unlock();
  // printf("l2\n");  
  
  for(unsigned int i=0; i<args->in_progress.size(); i++){
    // printf("l3\n");  
    
    Job* tmp_job= new Job("triggering");
    Trigger_args* tmp_args = new Trigger_args;
    tmp_args->m_data=args->m_data;
    tmp_args->time_slice =  std::move(args->in_progress.front());
    args->in_progress.pop();
    tmp_args->triggered_readout = args->triggered_readout;
    tmp_args->triggered_readout_mutex = args->triggered_readout_mutex;
    tmp_args->trigger_channels = args->trigger_channels;
    tmp_args->threashold = args->threashold;
    tmp_args->window_size = args->window_size;
    tmp_args->jump = args->jump;
    tmp_job->data=tmp_args;
    tmp_job->func=TriggerData;
    tmp_job->fail_func=FailTrigger;
    if(!args->m_data->job_queue.AddJob(tmp_job)) printf("ERROR!!!\n");
    // printf("l4\n");  
    
  }
  // printf("l5\n");  

  
  
}


bool Trigger::TriggerData(void* data){

  //printf("d1\n");
  Trigger_args* args=reinterpret_cast<Trigger_args*>(data);

  //run trigger algos
  //nhits
  //trigger channels
  //time??
 //printf("d2\n");
 args->count = new short[100000000];
 //printf("d3\n");
  for(unsigned int i=0; i <args->time_slice->hits.size(); i++){

    if(args->trigger_channels->count(args->time_slice->hits.at(i).channel)) args->time_slice->triggers.emplace(args->time_slice->triggers.end(), (*args->trigger_channels)[args->time_slice->hits.at(i).channel], args->time_slice->hits.at(i).time);
    else{
      //printf("a=%lu\n",args->time_slice->hits.at(i).time.bits());
      //printf("b=%lu\n",args->time_slice->time.bits());
      //printf("c=%lu\n",(args->time_slice->hits.at(i).time.bits() - args->time_slice->time.bits())>>9);
      //printf("channel = %u\n", args->time_slice->hits[i].channel);

      args->count[(args->time_slice->hits.at(i).time.bits() - args->time_slice->time.bits())>>9]++;
    }
  }
 //printf("d4\n");
  for(unsigned int i=1; i<100000000; i++){
    args->count[i]+=args->count[i-1];
  }
 //printf("d5\n");
  for(unsigned int i= (*args->window_size)+1; i<100000000-(*args->window_size); i++){
    if(args->count[i]-args->count[i-(*args->window_size)] >= (*args->threashold)){
      args->time_slice->triggers.emplace(args->time_slice->triggers.end(), TriggerType::nhits, (i-(*args->window_size) <<9) + args->time_slice->time.bits());
      i+=(*args->jump);
    }
  }
 //printf("d6\n");

  ////
   args->triggered_readout_mutex->lock();
   args->triggered_readout->emplace(std::move(args->time_slice)); //Ben this is bad and will lead to an unsorted queue dont use a queue;
   args->triggered_readout_mutex->unlock(); 
   //printf("d7\n");
   
   delete[] args->count;
   args->count=0;
   delete args;
   args=0;
   data=0;
   
   
  //printf("d8\n");

  return true;
}

void Trigger::FailTrigger(void* data){
  // printf("e1\n");
  Trigger_args* args=reinterpret_cast<Trigger_args*>(data);
  delete args;
  args=0;
  data=0;
  // printf("e2\n");
  
}


