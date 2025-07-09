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
  //tmp_args->triggered_readout = &(m_data->triggered_readout);
  //tmp_args->triggered_readout_mutex = &(m_data->triggered_readout_mutex);
  //tmp_args->final_readout = &(m_data->final_readout);
  //tmp_args->final_readout_mutex = &(m_data->sorted_readout_mutex);
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

  args->m_data->triggered_readout_mutex.lock();
  if(!args->m_data->triggered_readout.size()){
    args->m_data->triggered_readout_mutex.unlock();
    usleep(100);
    return;
  }
  std::swap(args->m_data->triggered_readout, args->in_progress);
  args->m_data->triggered_readout_mutex.unlock();
  
  for(size_t i=0; i<args->in_progress.size(); i++){
    
    //Job* tmp_job= new Job("window_building");
    args->tmp_job = args->job_pool.GetNew("window_building");

    if(args->tmp_job->data == 0){
      args->tmp_args = new WindowBuilder_args;
      args->tmp_args->m_data=args->m_data;
      args->tmp_job->data=args->tmp_args;
    }
    else args->tmp_args = reinterpret_cast<WindowBuilder_args*>(args->tmp_job->data);
    //printf("p1 %p\n",args->in_progress.front().get());
    args->tmp_args->time_slice =  std::move(args->in_progress.front());
    //printf("p2 %p\n",args->in_progress.front().get());
    //printf("p3 %p\n",tmp_args->time_slice.get());
    args->in_progress.pop();
    //printf("p4 %p\n",tmp_args->time_slice.get());
    //tmp_args->final_readout = args->final_readout;
    //tmp_args->final_readout_mutex = args->final_readout_mutex;
    args->tmp_args->trigger_offset = args->trigger_offset;
    args->tmp_args->pre_trigger = args->pre_trigger;
    args->tmp_args->post_trigger = args->post_trigger;
    //tmp_job->data=tmp_args;
    args->tmp_job->func=SelectData;
    args->tmp_job->fail_func=FailSelect;
    args->tmp_job->out_pool= &args->job_pool;
    if(!args->m_data->job_queue.AddJob(args->tmp_job)) printf("ERROR!!!\n");
    
  }
  
  
}

bool WindowBuilder::SelectData(void*& data){
  
  WindowBuilder_args* args=reinterpret_cast<WindowBuilder_args*>(data);
  
  //printf("d1\n");
  args->trigger_groups.clear();
  
  args->veto.clear();
  
  //combine triggers together
  
  for(size_t i=0; i< args->time_slice->triggers.size(); i++){
    if(args->veto.count(i)) continue;
    args->veto[i]=true;
    TriggerGroup tmp_trigger_group;
    
    tmp_trigger_group.triggers.push_back(args->time_slice->triggers.at(i));

    tmp_trigger_group.min = args->time_slice->triggers.at(i).time.bits() + (*args->trigger_offset)[args->time_slice->triggers.at(i).type] - (*args->pre_trigger)[args->time_slice->triggers.at(i).type];
    tmp_trigger_group.max = args->time_slice->triggers.at(i).time.bits() + (*args->trigger_offset)[args->time_slice->triggers.at(i).type] + (*args->post_trigger)[args->time_slice->triggers.at(i).type];
    
    for(size_t j=i+1; j< args->time_slice->triggers.size(); j++){
      uint64_t min = args->time_slice->triggers.at(j).time.bits() + (*args->trigger_offset)[args->time_slice->triggers.at(j).type] - (*args->pre_trigger)[args->time_slice->triggers.at(j).type];
      uint64_t max = args->time_slice->triggers.at(j).time.bits() + (*args->trigger_offset)[args->time_slice->triggers.at(j).type] + (*args->post_trigger)[args->time_slice->triggers.at(j).type];
      if(max > tmp_trigger_group.min && min < tmp_trigger_group.max){
	args->veto[j]=true;
	tmp_trigger_group.triggers.push_back(args->time_slice->triggers.at(j));
	if(min < tmp_trigger_group.min) tmp_trigger_group.min = min;
	if(max > tmp_trigger_group.max) tmp_trigger_group.max = max;
		
      } 
    }
    
    args->trigger_groups.push_back(tmp_trigger_group);
    
  }


  
  // looping over triggers to add hits
  args->time_slices.clear();  
  //std::vector<TimeSlice*> time_slices;
  for(size_t i=0; i< args->trigger_groups.size(); i++){

    //    std::cout<<"num of triggers ben = "<<args->trigger_groups.at(i).triggers.size()<<std::endl;
    //args->trigger_groups.at(i).triggers.at(0).Print();

    TimeSlice* tmp = new TimeSlice;
    tmp->triggers=args->trigger_groups.at(i).triggers;
    tmp->time=args->trigger_groups.at(i).min;

    //std::cout<<"num of triggers after = "<<tmp->triggers.size()<<std::endl;
    //tmp->triggers.at(0).Print();
    
    args->skip=false;
    // std::vector<Hit>::iterator min_it;
    //std::vector<Hit>::iterator max_it;
    
    for(std::vector<Hit>::iterator it=args->time_slice->hits.begin(); it!= args->time_slice->hits.end(); it++){
      if(it->sync_time.bits()<args->trigger_groups.at(i).min) continue;
      if(it->sync_time.bits()>args->trigger_groups.at(i).max) break;
      if(!args->skip){
	args->min_it=it;
	args->skip=true;
      }
      args->max_it=it; 
    }
    
    if(args->skip){
      args->max_it++;
      tmp->hits.insert(tmp->hits.end(), args->min_it, args->max_it);
    }   
    args->time_slices.push_back(tmp);
    tmp = 0;
  }

  
  
  //printf("d2\n");
   args->m_data->final_readout_mutex.lock();
   //printf("d3\n");
   for(size_t i=0; i< args->time_slices.size(); i++){
     args->m_data->final_readout.emplace(std::move(args->time_slices.at(i))); //Ben this is bad and will lead to an unsorted queue dont use a queue;
     args->time_slices.at(i)=0;
   }
   //printf("d4\n");
   args->m_data->final_readout_mutex.unlock(); 
   //printf("d5\n");
   // delete args;
   //args=0;
   //data=0;
   //printf("d6\n");
   return true;
}

void WindowBuilder::FailSelect(void*& data){

  // WindowBuilder_args* args=reinterpret_cast<WindowBuilder_args*>(data);
  //delete args;
  //args=0;
  //data=0;

  
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
