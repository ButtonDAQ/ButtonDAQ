#include "WindowBuilder.h"

WindowBuilder_args::WindowBuilder_args():Thread_args(){}

WindowBuilder_args::~WindowBuilder_args(){}


WindowBuilder::WindowBuilder():Tool(){}


bool WindowBuilder::Initialise(std::string configfile, DataModel &data){
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  m_configfile=configfile;
  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  m_util=new Utilities();

  m_threadnum=0;
  CreateThread();
  
  m_freethreads=1;
  
  ExportConfiguration();
  return true;
}


bool WindowBuilder::Execute(){

  if(m_data->change_config){
    InitialiseConfiguration(m_configfile);  
    LoadConfig();
    ExportConfiguration();
  }
  
  
  return true;
}


bool WindowBuilder::Finalise(){

  for(unsigned int i=0;i<args.size();i++) m_util->KillThread(args.at(i));
  
  args.clear();
  
  delete m_util;
  m_util=0;

  return true;
}

void WindowBuilder::CreateThread(){

  WindowBuilder_args* tmp_args=new WindowBuilder_args();
  tmp_args->m_data = m_data;
  tmp_args->triggered_readout = &(m_data->triggered_readout);
  tmp_args->triggered_readout_mutex = &(m_data->triggered_readout_mutex);
  tmp_args->final_readout = &(m_data->final_readout);
  tmp_args->final_readout_mutex = &(m_data->sorted_readout_mutex);
  tmp_args->trigger_offset = &trigger_offset;
  tmp_args->pre_trigger = &pre_trigger;
  tmp_args->post_trigger = &post_trigger;
  args.push_back(tmp_args);
  
  std::stringstream tmp;
  tmp<<"T"<<m_threadnum;

  m_util->CreateThread(tmp.str(), &Thread, args.at(args.size()-1));
  m_threadnum++;
  m_data->thread_num++;
  
}

 void WindowBuilder::DeleteThread(int pos){

   m_util->KillThread(args.at(pos));
   delete args.at(pos);
   args.at(pos)=0;
   args.erase(args.begin()+(pos-1));

 }

void WindowBuilder::Thread(Thread_args* arg){

  WindowBuilder_args* args=reinterpret_cast<WindowBuilder_args*>(arg);

  args->triggered_readout_mutex->lock();
  if(!args->triggered_readout->size()){
    args->triggered_readout_mutex->unlock();
    usleep(100);
    return;
  }
  std::swap(*args->triggered_readout, args->in_progress);
  args->triggered_readout_mutex->unlock();
  
  for(unsigned int i=0; i<args->in_progress.size(); i++){
    
    Job* tmp_job= new Job("window_building");
    WindowBuilder_args* tmp_args = new WindowBuilder_args;
    tmp_args->m_data=args->m_data;
    //printf("p1 %p\n",args->in_progress.front().get());
    tmp_args->time_slice =  std::move(args->in_progress.front());
    //printf("p2 %p\n",args->in_progress.front().get());
    //printf("p3 %p\n",tmp_args->time_slice.get());
    args->in_progress.pop();
    //printf("p4 %p\n",tmp_args->time_slice.get());
    tmp_args->final_readout = args->final_readout;
    tmp_args->final_readout_mutex = args->final_readout_mutex;
    tmp_args->trigger_offset = args->trigger_offset;
    tmp_args->pre_trigger = args->pre_trigger;
    tmp_args->post_trigger = args->post_trigger;
    tmp_job->data=tmp_args;
    tmp_job->func=SelectData;
    tmp_job->fail_func=FailSelect;
    if(!args->m_data->job_queue.AddJob(tmp_job)) printf("ERROR!!!\n");
    
  }
  
  
}

bool WindowBuilder::SelectData(void* data){
  
  WindowBuilder_args* args=reinterpret_cast<WindowBuilder_args*>(data);
  
  //printf("d1\n");
  std::vector<TriggerGroup> trigger_groups;
  
  std::map<unsigned short, bool> veto;
  
  //combine triggers together
  
  for( unsigned int i=0; i< args->time_slice->triggers.size(); i++){
    if(veto.count(i)) continue;
    veto[i]=true;
    TriggerGroup tmp_trigger_group;
    
    tmp_trigger_group.triggers.push_back(args->time_slice->triggers.at(i));

    tmp_trigger_group.min = args->time_slice->triggers.at(i).time.bits() + (*args->trigger_offset)[args->time_slice->triggers.at(i).type] - (*args->pre_trigger)[args->time_slice->triggers.at(i).type];
    tmp_trigger_group.max = args->time_slice->triggers.at(i).time.bits() + (*args->trigger_offset)[args->time_slice->triggers.at(i).type] + (*args->post_trigger)[args->time_slice->triggers.at(i).type];
    
    for( unsigned int j=i+1; j< args->time_slice->triggers.size(); j++){
      if(args->time_slice->triggers.at(j).time.bits() > tmp_trigger_group.min && args->time_slice->triggers.at(j).time.bits()< tmp_trigger_group.max){
	veto[j]=true;
	tmp_trigger_group.triggers.push_back(args->time_slice->triggers.at(j));
	if(args->time_slice->triggers.at(j).time.bits() - (*args->pre_trigger)[args->time_slice->triggers.at(j).type] < tmp_trigger_group.min) tmp_trigger_group.min = args->time_slice->triggers.at(j).time.bits() - (*args->pre_trigger)[args->time_slice->triggers.at(j).type];
	if(args->time_slice->triggers.at(j).time.bits() + (*args->post_trigger)[args->time_slice->triggers.at(j).type] > tmp_trigger_group.max) tmp_trigger_group.max = args->time_slice->triggers.at(j).time.bits() - (*args->post_trigger)[args->time_slice->triggers.at(j).type];
		
      } 
    }
    
    trigger_groups.push_back(tmp_trigger_group);
    
  }
  
  // looping over triggers to add hits
  
  std::vector<TimeSlice*> time_slices;
  for(unsigned int i=0; i< trigger_groups.size(); i++){

    TimeSlice* tmp = new TimeSlice;
    tmp->triggers=trigger_groups.at(i).triggers;
    tmp->time=trigger_groups.at(i).min;

    bool skip=false;
    std::vector<Hit>::iterator min_it;
    std::vector<Hit>::iterator max_it;
    
    for(std::vector<Hit>::iterator it=args->time_slice->hits.begin(); it!= args->time_slice->hits.end(); it++){
      if(it->time.bits()<trigger_groups.at(i).min) continue;
      if(it->time.bits()>trigger_groups.at(i).max) break;
      if(!skip){
	min_it=it;
	skip=true;
      }
      max_it=it;
      
      
    }
    tmp->hits.insert(tmp->hits.end(), min_it, max_it);
    time_slices.push_back(tmp);
  }

  
  
  //printf("d2\n");
   args->final_readout_mutex->lock();
   //printf("d3\n");
   for(unsigned int i=0; i< time_slices.size(); i++){
     args->final_readout->emplace(std::move(time_slices.at(i))); //Ben this is bad and will lead to an unsorted queue dont use a queue;
     time_slices.at(i)=0;
   }
   //printf("d4\n");
   args->final_readout_mutex->unlock(); 
   //printf("d5\n");
   delete args;
   args=0;
   data=0;
   //printf("d6\n");
   return true;
}

void WindowBuilder::FailSelect(void* data){

  WindowBuilder_args* args=reinterpret_cast<WindowBuilder_args*>(data);
  delete args;
  args=0;
  data=0;

  
}

void WindowBuilder::LoadConfig(){

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;
  unsigned long tmp=0;
  
  if(m_variables.Get("nhits_trigger_offset",tmp)) trigger_offset[TriggerType::nhits]=tmp;
  if(m_variables.Get("nhits_pre_trigger",tmp)) pre_trigger[TriggerType::nhits]=tmp;
  if(m_variables.Get("nhits_post_trigger",tmp)) post_trigger[TriggerType::nhits]=tmp;

  if(m_variables.Get("calib_trigger_offset",tmp)) trigger_offset[TriggerType::calib]=tmp;
  if(m_variables.Get("calib_pre_trigger",tmp)) pre_trigger[TriggerType::calib]=tmp;
  if(m_variables.Get("calib_post_trigger",tmp)) post_trigger[TriggerType::calib]=tmp;

  if(m_variables.Get("zero_bias_trigger_offset",tmp)) trigger_offset[TriggerType::zero_bias]=tmp;
  if(m_variables.Get("zero_bias_pre_trigger",tmp)) pre_trigger[TriggerType::zero_bias]=tmp;
  if(m_variables.Get("zero_bias_post_trigger",tmp)) post_trigger[TriggerType::zero_bias]=tmp;
  
  
}
