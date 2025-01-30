#include "Factory.h"
#include "Unity.h"

Tool* Factory(std::string tool) {
Tool* ret=0;

// if (tool=="Type") tool=new Type;
if (tool=="CalibTrigger") ret=new CalibTrigger;
if (tool=="Configuration") ret=new Configuration;
if (tool=="DataWriter") ret=new DataWriter;
if (tool=="Digitizer") ret=new Digitizer;
if (tool=="DummyTool") ret=new DummyTool;
if (tool=="FileWriter") ret=new FileWriter;
if (tool=="HVoltage") ret=new HVoltage;
if (tool=="JobManager") ret=new JobManager;
if (tool=="Monitoring") ret=new Monitoring;
if (tool=="NhitsTrigger") ret=new NhitsTrigger;
if (tool=="Reformatter") ret=new Reformatter;
if (tool=="RunControl") ret=new RunControl;
if (tool=="Sorter") ret=new Sorter;
if (tool=="Trigger") ret=new Trigger;
if (tool=="WindowBuilder") ret=new WindowBuilder;
return ret;
}
