#include "Sorter.h"

Sorter_args::Sorter_args():Thread_args(){}

Sorter_args::~Sorter_args(){}


Sorter::Sorter():Tool(){}


bool Sorter::Initialise(std::string configfile, DataModel &data){

  if(configfile!="")  m_variables.Initialise(configfile);
  //m_variables.Print();

  m_data= &data;
  m_log= m_data->Log;

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  m_util=new Utilities();

  m_threadnum=0;
  CreateThread();
  
  m_freethreads=1;
  
    
  
  return true;
}


bool Sorter::Execute(){

  for(unsigned int i=0; i<args.size(); i++){
    if(args.at(i)->busy==0){
      std::cout<<"reply="<<args.at(i)->message<<std::endl;
      args.at(i)->message="Hi";
      args.at(i)->busy=1;
      break;
    }

  }

  m_freethreads=0;
  int lastfree=0;
  for(unsigned int i=0; i<args.size(); i++){
    if(args.at(i)->busy==0){
      m_freethreads++;
      lastfree=i; 
    }
  }

  if(m_freethreads<1) CreateThread();
  if(m_freethreads>1) DeleteThread(lastfree);
  
  std::cout<<"free threads="<<m_freethreads<<":"<<args.size()<<std::endl;
  
  sleep(1);
  
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

  Sorter_args* tmparg=new Sorter_args();
  tmparg->busy=0;
  tmparg->message="";
  args.push_back(tmparg);
  std::stringstream tmp;
  tmp<<"T"<<m_threadnum;
  m_util->CreateThread(tmp.str(), &Thread, args.at(args.size()-1));
  m_threadnum++;

}

 void Sorter::DeleteThread(int pos){

   m_util->KillThread(args.at(pos));
   delete args.at(pos);
   args.at(pos)=0;
   args.erase(args.begin()+(pos-1));

 }

void Sorter::Thread(Thread_args* arg){

  Sorter_args* args=reinterpret_cast<Sorter_args*>(arg);

  if(!args->busy) usleep(100);
  else{ 

    args->message="Hello";
    sleep(10);

    args->busy=0;
  }

}
