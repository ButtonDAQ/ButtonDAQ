#include "NhitsTrigger.h"

NhitsTrigger_args::NhitsTrigger_args():Thread_args(){}

NhitsTrigger_args::~NhitsTrigger_args(){}


NhitsTrigger::NhitsTrigger():Tool(){}


bool NhitsTrigger::Initialise(std::string configfile, DataModel &data){
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


bool NhitsTrigger::Execute(){

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


bool NhitsTrigger::Finalise(){

  for(unsigned int i=0;i<args.size();i++) m_util->KillThread(args.at(i));
  
  args.clear();
  
  delete m_util;
  m_util=0;

  return true;
}

void NhitsTrigger::CreateThread(){

  NhitsTrigger_args* tmparg=new NhitsTrigger_args();
  tmparg->busy=0;
  tmparg->message="";
  args.push_back(tmparg);
  std::stringstream tmp;
  tmp<<"T"<<m_threadnum;
  m_util->CreateThread(tmp.str(), &Thread, args.at(args.size()-1));
  m_threadnum++;

}

 void NhitsTrigger::DeleteThread(int pos){

   m_util->KillThread(args.at(pos));
   delete args.at(pos);
   args.at(pos)=0;
   args.erase(args.begin()+(pos-1));

 }

void NhitsTrigger::Thread(Thread_args* arg){

  NhitsTrigger_args* args=reinterpret_cast<NhitsTrigger_args*>(arg);

  if(!args->busy) usleep(100);
  else{ 

    args->message="Hello";
    sleep(10);

    args->busy=0;
  }

}
