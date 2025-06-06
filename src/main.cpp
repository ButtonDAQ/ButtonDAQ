#include <string>

#include <signal.h>

#include "ToolDAQChain.h"
#include "DataModel.h"
//#include "DummyTool.h"

static DataModel* data_model;

static void (*default_terminate)() = nullptr;

static void std_terminate() {
  data_model->terminate();
  if (default_terminate) default_terminate();
  std::abort();
};

static void signal_terminate(int sig) {
  fprintf(stderr, "Terminating due to signal %d\n", sig);
  data_model->terminate();
  signal(sig, SIG_DFL);
  if (raise(sig) != 0) {
    fprintf(stderr, "Failed to re-raise signal %d, aborting\n", sig);
    std::abort();
  };
};

int main(int argc, char* argv[]){

  std::string conffile;
  if (argc==1)conffile="configfiles/Dummy/ToolChainConfig";
  else conffile=argv[1];

  data_model = new DataModel();

  default_terminate = std::set_terminate(std_terminate);
  signal(SIGINT,  signal_terminate);
  signal(SIGTERM, signal_terminate);
  signal(SIGQUIT, signal_terminate);
  signal(SIGSEGV, signal_terminate); // crossing fingers

  ToolDAQChain tools(conffile, data_model, argc, argv);


  //DummyTool dummytool;    

  //tools.Add("DummyTool",&dummytool,"configfiles/DummyToolConfig");

  //int portnum=24000;
  //  tools.Remote(portnum);
  //tools.Interactive();
  
  //  tools.Initialise();
  // tools.Execute();
  //tools.Finalise();
  
  
  return 0;
  
}
