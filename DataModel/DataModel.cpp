#include "DataModel.h"

#include <caen++/vme.hpp>

DataModel::DataModel(){}

/*
TTree* DataModel::GetTTree(std::string name){

  return m_trees[name];

}


void DataModel::AddTTree(std::string name,TTree *tree){

  m_trees[name]=tree;

}


void DataModel::DeleteTTree(std::string name,TTree *tree){

  m_trees.erase(name);

}

*/

uint32_t VMELinks::scan(uint8_t address) {
  printf("VMELinks: looking for %hhu\n", address);
  for (uint32_t link = 0; link < 10; ++link) {
    if (link == m_digitizer || link == m_hvoltage) continue;
    try {
      printf("VMELinks: checking link %u... ", link);
      uint8_t ba = caen::Bridge(cvV1718, &link, 0).readRegister(0x36);
      if (ba == address) {
	printf("success\n");
	return link;
      };
      printf("%hhu\n", ba);
    } catch (caen::Bridge::Error& e) {
      printf("%s\n", e.what());
      if (e.code() != cvCommError) throw;
    };
  };
  return ~0;
};

uint32_t VMELinks::digitizer() {
  if (m_digitizer != ~0) return m_digitizer;
  m_digitizer = scan(0x20);
  if (m_digitizer == ~0)
    throw std::runtime_error("Cannot find the digitizer VME crate");
  return m_digitizer;
};

uint32_t VMELinks::hvoltage() {
  if (m_hvoltage != ~0) return m_hvoltage;
  m_hvoltage = scan(0x10);
  if (m_hvoltage == ~0)
    throw std::runtime_error("Cannot find the high voltage VME crate");
  return m_hvoltage;
};
