// SirioC.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "cpu_features.h"
#include "cuckoo.h"
#include "threads.h"
#include "tt.h"
#include "uci.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

int main(int argc, char** argv)
{
  auto cpu = detectCpuFeatures();
  std::fprintf(stderr, "info string CPU features: %s\n", cpu.toString().c_str());
  requireSupportedOrExit(cpu);

  std::cout << "SirioC " << engineVersion << " by Gabriele Lombardo" << std::endl;

  Zobrist::init();

  Bitboards::init();

  positionInit();

  Cuckoo::init();

  Search::init();

  UCI::init();

  Threads::setThreadCount(UCI::Options["Threads"]);
  TT::resize(UCI::Options["Hash"]);

  NNUE::loadWeights();

  UCI::loop(argc, argv);

  Threads::setThreadCount(0);

  return 0;
}