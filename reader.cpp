#include <iostream>
#include <BinaryStream.h>
#include <TimeSlice.h>
#include <SerialisableObject.h>

int main(int argc, char* argv[]){

  BinaryStream bs;

  bs.Bopen(argv[1], READ, UNCOMPRESSED);

  unsigned long size=0;

  bs >> size;

  std::cout<<"Time slices in file ="<<size<<std::endl;
  for(unsigned long i=0; i < size; i++){

    TimeSlice ts;
    bs >> ts;

    std::cout<<"TimeSlice "<<i<<":";
    ts.Print();
    std::cout<<std::endl;
  }


  bs.Bclose();

  return 0;

}
