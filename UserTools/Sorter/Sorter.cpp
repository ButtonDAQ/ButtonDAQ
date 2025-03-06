#include "Sorter.h"

Sorter_args::Sorter_args():Thread_args(){}

Sorter_args::~Sorter_args(){}


Sorter::Sorter():Tool(){}


bool Sorter::Initialise(std::string configfile, DataModel &data){
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  m_util=new Utilities();

  m_threadnum=0;
  CreateThread();
  
  m_freethreads=1;
  
  ExportConfiguration();
  return true;
}


bool Sorter::Execute(){

  
  return true;
}


bool Sorter::Finalise(){

  for(unsigned int i=0;i<args.size();i++) m_util->KillThread(args.at(i));
  
  args.clear();
  
  delete m_util;
  m_util=0;

  return true;
}

void Sorter::CreateThread(){

  Sorter_args* tmp_args=new Sorter_args();
  tmp_args->m_data = m_data;
  tmp_args->readout = &(m_data->readout);
  tmp_args->readout_mutex = &(m_data->readout_mutex);
  tmp_args->sorted_readout = &(m_data->sorted_readout);
  tmp_args->sorted_readout_mutex = &(m_data->sorted_readout_mutex);
  args.push_back(tmp_args);
  
  std::stringstream tmp;
  tmp<<"T"<<m_threadnum;

  m_util->CreateThread(tmp.str(), &Thread, args.at(args.size()-1));
  m_threadnum++;
  m_data->thread_num++;
  
}

 void Sorter::DeleteThread(int pos){

   m_util->KillThread(args.at(pos));
   delete args.at(pos);
   args.at(pos)=0;
   args.erase(args.begin()+(pos-1));

 }

void Sorter::Thread(Thread_args* arg){

  Sorter_args* args=reinterpret_cast<Sorter_args*>(arg);

  args->readout_mutex->lock();
  if(!args->readout->size()){
    args->readout_mutex->unlock();
    usleep(100);
    return;
  }
  std::swap(*args->readout, args->in_progress);
  args->readout_mutex->unlock();
  
  for(unsigned int i=0; i<args->in_progress.size(); i++){
    
    Job* tmp_job= new Job("sorting");
    Sorter_args* tmp_args = new Sorter_args;
    tmp_args->m_data=args->m_data;
    //printf("p1 %p\n",args->in_progress.front().get());
    tmp_args->time_slice =  std::move(args->in_progress.front());
    //printf("p2 %p\n",args->in_progress.front().get());
    //printf("p3 %p\n",tmp_args->time_slice.get());
    args->in_progress.pop();
    //printf("p4 %p\n",tmp_args->time_slice.get());
    tmp_args->sorted_readout = args->sorted_readout;
    tmp_args->sorted_readout_mutex = args->sorted_readout_mutex;
    tmp_job->data=tmp_args;
    tmp_job->func=SortData;
    tmp_job->fail_func=FailSort;
    if(!args->m_data->job_queue.AddJob(tmp_job)) printf("ERROR!!!\n");
    
  }
  
  
}

bool Sorter::SortData(void* data){

  Sorter_args* args=reinterpret_cast<Sorter_args*>(data);

  //printf("d1\n");
  //printf("d1.2 %p\n",args->time_slice.get());
  //unsigned int a=args->time_slice->hits.size();
  //printf("d1.5 %u\n",a); 
  std::sort(args->time_slice->hits.begin(), args->time_slice->hits.end(), [](Hit a, Hit b)
                                  {
                                      return a.time < b.time;
                                  });
  //printf("d2\n");
   args->sorted_readout_mutex->lock();
   //printf("d3\n");
   args->sorted_readout->emplace(std::move(args->time_slice)); //Ben this is bad and will lead to an unsorted queue dont use a queue;
   //printf("d4\n");
   args->sorted_readout_mutex->unlock(); 
   //printf("d5\n");
   delete args;
   args=0;
   data=0;
   //printf("d6\n");
   return true;
}

void Sorter::FailSort(void* data){

  Sorter_args* args=reinterpret_cast<Sorter_args*>(data);
  delete args;
  args=0;
  data=0;

  
}
