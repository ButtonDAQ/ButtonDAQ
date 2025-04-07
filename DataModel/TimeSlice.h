#ifndef TIME_SLICE_H
#define TIME_SLICE_H

#include <vector>
#include <map>
#include <mutex>
#include <type_traits>

#include <cstddef>
#include <Hit.h>
#include <zmq.hpp>

using namespace ToolFramework;


enum class TriggerType {nhits, calib, zero_bias};

class TriggerInfo : SerialisableObject{

public:

  TriggerInfo(){;}
  TriggerInfo(TriggerType in_type, Time in_time){
    type = in_type;
    time = in_time;
  }
  TriggerType type;
  Time time;

  std::string GetType(TriggerType type){
    switch (type) {
      
    case TriggerType::nhits:
      return "nhits";
      
    case TriggerType::calib:
      return "calib";
      
    case TriggerType::zero_bias:
      return "zero_bias";
      
    default:
      return "";
    }
  }
  
  bool Print(){

    std::cout<<"Trigger "<<GetType(type)<<":"<<time.Print()<<std::endl;

    return true;
  }
  std::string GetVersion(){return "1.0";};
  bool Serialise(BinaryStream &bs){
    

    bs & type;
    bs & time; 
    
    return true;
  }
};

class TimeSlice : SerialisableObject {

public:
  
  Time time;
  std::vector<Hit> hits;
  std::mutex mutex;
  std::vector<TriggerInfo> triggers;

  bool Print(){


    std::cout<<std::endl<<"time="<<time.Print()<<std::endl;
    std::cout<<" hits size="<<hits.size()<<std::endl;

    for(size_t i=0; i<hits.size(); i++){
      std::cout<<"  hit "<<i<<":";
      hits.at(i).Print();
      std::cout<<std::endl;
    }

    std::cout<<std::endl<<" triggers size="<<triggers.size()<<std::endl;

    for(size_t i=0; i<triggers.size(); i++){
      std::cout<<"  trigger "<<i<<":";
      triggers.at(i).Print();
       std::cout<<std::endl;
    }
    std::cout<<std::endl;

    return true;
  }

  std::string GetVersion(){return "1.0";};
  bool Serialise(BinaryStream &bs){

    bs & time;
    bs & hits;
    bs & triggers;
    
    return true;
  }

  void Send(zmq::socket_t* sock){

    zmq::message_t msg1(sizeof(time));
    memcpy(msg1.data(), &time, sizeof(time));
    sock->send(msg1, ZMQ_SNDMORE);

    unsigned long size=triggers.size();
    zmq::message_t msg2(sizeof(size));
    memcpy(msg2.data(), &size, sizeof(size));
    sock->send(msg2, ZMQ_SNDMORE);

    if(triggers.size()>0){
      zmq::message_t msg3(sizeof(TriggerInfo)*triggers.size());
      memcpy(msg3.data(), triggers.data(), sizeof(TriggerInfo)*triggers.size());
      sock->send(msg3, ZMQ_SNDMORE);
    }

    size= hits.size();
    zmq::message_t msg4(sizeof(size));
    memcpy(msg4.data(), &size, sizeof(size));
    if(hits.size()==0){
      sock->send(msg4);
      return;
    }
      
    sock->send(msg4, ZMQ_SNDMORE);
    
    for(size_t i=0; i<hits.size()-1; i++){
      hits.at(i).Send(sock, ZMQ_SNDMORE);	
    }
    hits.at(hits.size()-1).Send(sock);
        
  }

   void Receive(zmq::socket_t* sock){

     zmq::message_t msg1;
     sock->recv(&msg1);
     memcpy(&time, msg1.data(), sizeof(time));

     unsigned long size=0;
     
     if(msg1.more()){  
        zmq::message_t msg2;
	sock->recv(&msg2);
	memcpy(&size, msg2.data(), sizeof(size));
	triggers.resize(size);
	
	zmq::message_t msg3;
	if(size>0 && msg2.more()){	  
	  sock->recv(&msg3);
	  memcpy(triggers.data(), msg3.data(), sizeof(TriggerInfo)*triggers.size());
	}
	
	if((size==0 && msg2.more()) || (size>0 && msg3.more())){
	  zmq::message_t msg4;
	  sock->recv(&msg4);
	  memcpy(&size, msg4.data(), sizeof(size));
	  hits.resize(size);

	  for(size_t i=0; i<hits.size()-1; i++){
	    hits.at(i).Receive(sock); 
	  }
	  
	}
	   

     }

   }
  
};

#endif
