#ifndef HIT_H
#define HIT_H

#include <cstdint>
#include <zmq.hpp>

using namespace ToolFramework;

// Stores time in 64 bits as a fixed point value with 1/512 ns precision.
// Bits 10 to 64 store an integer number of ticks, each tick is 2 ns.
// Bits 0 to 9 store sub-ns fraction.
// CAEN digitizer time range is 57 bits in this format.
class Time : SerialisableObject{
  public:
    Time(): time(0) {};
    Time(const Time& t): time(t.time) {};

    // See Table 2.3 in UM2580_DPSD_UserManual_rev9
    explicit Time(long double seconds) {
      seconds /= 2e-9; // Tsampl
      time = seconds; // Tcoarse
      seconds *= 1024;
      time = time << 10 | static_cast<uint64_t>(seconds) & 0x3ff; // Tfine
    };

    // Construct Time from CAEN digitizer output data.
    // tag is the trigger time tag, extras provides the most and the least significant bits.
    // See "Channel aggregate data format" in DPP-PSD documentation.
    Time(uint32_t tag, uint32_t extras) {
      time = extras & 0xffff0000; // bits 16 to 31
      time <<= 31 - 16;
      time |= tag;
      time <<= 10;
      time |= extras & 0x3ff; // bits 0 to 9
    };
  
  Time(uint64_t time): time(time) {}

  double seconds() const {
      return (
            static_cast<long double>(time >> 10)
          + static_cast<long double>(time & 0x3ff) / 1024.0L
      ) * 2e-9L;
    };

    uint64_t bits() const {
      return time;
    };

    Time& operator=(Time t) {
      time = t.time;
      return *this;
    };

    bool operator==(Time t) const {
      return time == t.time;
    };

    bool operator!=(Time t) const {
      return time != t.time;
    };

    bool operator>(Time t) const {
      return time > t.time;
    };

    bool operator>=(Time t) const {
      return time >= t.time;
    };

    bool operator<(Time t) const {
      return time < t.time;
    };

    bool operator<=(Time t) const {
      return time <= t.time;
    };

    Time operator+(Time t) const {
      return Time(time + t.time);
    };

    Time& operator+=(Time t) {
      time += t.time;
      return *this;
    };

    Time operator-(Time t) const {
      return Time(time - t.time);
    };

    Time& operator-=(Time t) {
      time -= t.time;
      return *this;
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time>::type
    operator*(Number x) const {
      return Time(time * x);
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time&>::type
    operator*=(Number x) {
      time *= x;
      return *this;
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time>::type
    operator/(Number x) const {
      return Time(time / x);
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time&>::type
    operator/=(Number x) {
      time /= x;
      return *this;
    };

  bool Print(){

    std::cout<<seconds();
    
    return true;
  }
  std::string GetVersion(){return "1.0";};
  bool Serialise(BinaryStream &bs){
    bs & time;
    return true;
  }
  
  private:
    uint64_t time;

  //  explicit Time(uint64_t time): time(time) {};
};

template <typename Number>
typename std::enable_if<std::is_arithmetic<Number>::value, Time>::type
operator*(Number x, Time t) {
  return t * x;
};

class Hit : SerialisableObject {

public:
  
  Time     time;
  uint16_t charge_short;
  uint16_t charge_long;
  uint16_t baseline;
  uint8_t  channel; // (digitizer channel) | (digitizer id) << 4
  std::vector<uint16_t> waveform;

  static uint8_t get_digitizer_id(uint8_t channel) {
    return channel >> 4;
  };
  
  bool Print(){

    unsigned long sum=0;
    std::cout<<time.Print()<<","<<charge_short<<","<<charge_long<<","<<baseline<<","<<((unsigned short)channel);
    for(size_t i=0; i<waveform.size(); i++){
      //std::cout<<", ["<<i<<"]"
      std::cout<<", "<<waveform.at(i);
      sum+=waveform.at(i);
    }

    std::cout<<", sum="<<sum;
    
    return true;
  }
  std::string GetVersion(){return "1.0";};
  bool Serialise(BinaryStream &bs){
    
    bs & time;
    bs & charge_short;
    bs & charge_long;
    bs & baseline;
    bs & channel;
    bs & waveform;
    
    return true;
  }

  void Send(zmq::socket_t* sock, unsigned int more=0){
    
    zmq::message_t msg1(sizeof(time));
    memcpy(msg1.data(), &time, sizeof(time));
    sock->send(msg1, ZMQ_SNDMORE);

    zmq::message_t msg2(sizeof(charge_short));
    memcpy(msg2.data(), &charge_short, sizeof(charge_short));
    sock->send(msg2, ZMQ_SNDMORE);

    zmq::message_t msg3(sizeof(charge_long));
    memcpy(msg3.data(), &charge_long, sizeof(charge_long));
    sock->send(msg3, ZMQ_SNDMORE);
 
    zmq::message_t msg4(sizeof(baseline));
    memcpy(msg4.data(), &baseline, sizeof(baseline));
    sock->send(msg4, ZMQ_SNDMORE);    
    
    zmq::message_t msg5(sizeof(channel));
    memcpy(msg5.data(), &channel, sizeof(channel));
    sock->send(msg5, ZMQ_SNDMORE);

    unsigned long size= waveform.size();
    zmq::message_t msg6(sizeof(size));
    memcpy(msg6.data(), &size, sizeof(size));
    if(size==0){
      sock->send(msg6, more);
      return;
    }
    
    sock->send(msg6, ZMQ_SNDMORE);
    zmq::message_t msg7(sizeof(uint16_t)* waveform.size());
    memcpy(msg7.data(), waveform.data(), sizeof(uint16_t)* waveform.size());
    sock->send(msg7, more);
    
  }


  void Receive(zmq::socket_t* sock){

    zmq::message_t msg1;
    sock->recv(&msg1);
    memcpy(&time, msg1.data(), sizeof(time));
    
    zmq::message_t msg2;
    sock->recv(&msg2);
    memcpy(&charge_short, msg2.data(), sizeof(charge_short));
    
    zmq::message_t msg3;
    sock->recv(&msg3);
    memcpy(&charge_long, msg3.data(), sizeof(charge_long));
  
    zmq::message_t msg4;
    sock->recv(&msg4);
    memcpy(&baseline, msg4.data(), sizeof(baseline));
     
    zmq::message_t msg5;
    sock->recv(&msg5);
    memcpy(&channel, msg5.data(), sizeof(channel));

    unsigned long size= 0;
    zmq::message_t msg6;
    sock->recv(&msg6);
    memcpy(&size, msg6.data(), sizeof(size));

    waveform.resize(size);
    
    if(size!=0){
      zmq::message_t msg7;
      sock->recv(&msg7);
      memcpy(waveform.data(), msg7.data(), sizeof(uint16_t)* waveform.size());
      
    }

    
  }

  
  
};

#endif
