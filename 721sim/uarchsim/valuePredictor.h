#ifndef PIPELINE_H
#define PIPELINE_H

#include <vector>
#include <inttypes.h>
#include <svpEntry.cc>
#include <vpQueue.cc>
#include <payload.h>
// #include <pipeline.h>
#include <math.h>

using namespace std;

class StrideValuePredictor {
public:
   vector<svpEntry> svp;
   vpQueue vpq;
   uint64_t MAX_PC_INDEX;
   uint8_t REPLACE;

   StrideValuePredictor();
   bool isEligible(unsigned int flags);
   void printSVP();
   void printVPQ();
   void flushSVPInstance();
   void train(uint64_t addr, uint64_t ValueFromRetire);
   void predValue(uint64_t &prediction, bool &confidence, uint64_t addr); 
   bool hit(uint64_t index, uint64_t tag);
   svpEntry createEntryInSVP(uint64_t tag);
   bool isTagMatch(uint64_t tag, uint64_t index);
   bool handleSVPEntry(uint64_t tag);
   void forwardWalkVPQToDecreamentInstance(uint64_t checkpointedVPQTail, bool checkpointedVPQTailPhase);
   void rollback(uint64_t checkpointedVPQTail, bool checkpointedVPQTailPhase);
 //recovery wagera ka dekh lo!
};

#endif // PIPELINE_H