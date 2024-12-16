#include "Factory.h"

Tool* Factory(std::string tool) {
Tool* ret=0;

// if (tool=="Type") tool=new Type;
if (tool=="DummyTool") ret=new DummyTool;
if (tool=="Sorter") ret=new Sorter;
if (tool=="NhitsTrigger") ret=new NhitsTrigger;
if (tool=="CalibTrigger") ret=new CalibTrigger;
if (tool=="DataWriter") ret=new DataWriter;
if (tool=="Digitizer") ret=new Digitizer;
  if (tool=="Reformatter") ret=new Reformatter;
return ret;
}
