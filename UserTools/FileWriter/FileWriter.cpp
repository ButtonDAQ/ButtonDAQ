#include "FileWriter.h"

FileWriter_args::FileWriter_args():Thread_args(){
  data=0;
  file_name=0;
  part_number=0;
  file_writeout_period=0;
}

FileWriter_args::~FileWriter_args(){
  data=0;
  file_name=0;
  part_number=0;
  file_writeout_period=0;
}


FileWriter::FileWriter():Tool(){}


bool FileWriter::Initialise(std::string configfile, DataModel &data){

  InitialiseTool(data);
  m_configfile=configfile;
  InitialiseConfiguration(configfile);
  //m_variables.Print();

  m_util=new Utilities();
  args=new FileWriter_args();
  
  LoadConfig();
  
  args->data= m_data;
  args->file_name= &m_file_name;
  args->part_number= &m_part_number;
  args->file_writeout_period= & m_file_writeout_period;
  
  m_util->CreateThread("test", &Thread, args);

  ExportConfiguration();
  
  return true;
}


bool FileWriter::Execute(){

  if(m_data->change_config){
    InitialiseConfiguration(m_configfile); // surely add load config here   need to do return checks.
    
    ExportConfiguration();
  }
  if(m_data->run_start) LoadConfig();   ///?   oh maybe to ensure file file written before load config happends but this is a crap way of doing it please change Ben
  if(m_data->run_stop) args->period=boost::posix_time::seconds(10);

  m_data->vars.Set("part",m_part_number);
  
  return true;
}


bool FileWriter::Finalise(){

  m_util->KillThread(args);

  delete args;
  args=0;

  delete m_util;
  m_util=0;

  return true;
}

void FileWriter::Thread(Thread_args* arg){

  FileWriter_args* args=reinterpret_cast<FileWriter_args*>(arg);

  args->lapse = args->period -( boost::posix_time::microsec_clock::universal_time() - args->last);
  if(!args->lapse.is_negative()){
    usleep(100);
    return;
  }
  //printf("%d\n", *(args->part_number));
  //  if(*args->file_writeout_period==0){
  // *args->file_writeout_period=1;
  //}
  
  args->last= boost::posix_time::microsec_clock::universal_time();
  
  args->data->triggered_readout_mutex.lock();

  if(args->data->triggered_readout.size()==0){
    args->data->triggered_readout_mutex.unlock();
    return;
  }

  printf("writing out data\n");

  std::queue<std::unique_ptr<TimeSlice>> local_readout;

  std::swap(args->data->triggered_readout, local_readout);
  args->data->triggered_readout_mutex.unlock();
  /*
  std::queue<std::unique_ptr<TimeSlice>> local_trimmed_readout;

  for(unsigned int i=0; i<local_readout.size(); i++){
    if(!local_readout.front()->triggers.size()){
      local_readout.pop();
      continue;
    }
    local_trimmed_readout.emplace(std::move(local_readout.front())); 
    local_readout.pop();
  }
  */
  std::stringstream filename;
  filename<<(*args->file_name)<<"R"<<args->data->run_number<<"S"<<args->data->sub_run_number<<"P"<<(*args->part_number)<<".dat";
  BinaryStream output;
  output.Bopen(filename.str().c_str(), NEW, UNCOMPRESSED);


  //  unsigned long size=local_trimmed_readout.size();
  unsigned long size=local_readout.size();
  output<<size;

  for(unsigned int i=0; i< size; i++){
    //    output<<local_trimmed_readout.front();
    //    local_trimmed_readout.pop();
    output<<local_readout.front();
    local_readout.pop();
  }
  
  output.Bclose();
  (*args->part_number)++;
  
}

void FileWriter::LoadConfig(){ // change to bool have a return type

  
  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;
  if(!m_variables.Get("file_path",m_file_name)) m_file_name="./data";
  if(!m_variables.Get("file_writeout_period",m_file_writeout_period)) m_file_writeout_period=60;//300;
  
  m_part_number=0;
  args->last=boost::posix_time::microsec_clock::universal_time();
  args->period=boost::posix_time::seconds(m_file_writeout_period);
  
}
