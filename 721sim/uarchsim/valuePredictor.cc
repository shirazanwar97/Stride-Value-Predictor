#include "valuePredictor.h"
#include <assert.h>
#include <limits>
#include <math.h>
#include "pipeline.h"
#include <iostream>
#include <iomanip>
#include <parameters.h>
// #include "payload.h"	

using namespace std;

   StrideValuePredictor::StrideValuePredictor():vpq() {
      MAX_PC_INDEX = pow(2,SVPIndexBits);
      REPLACE = Replace;

      svp.resize(MAX_PC_INDEX);
      //initialize SVP and VPQ.   
      for(int i = 0; i< MAX_PC_INDEX; i++) {
        svp[i].tag = 0;
        svp[i].conf = 0;
        svp[i].retVal = 0;
        svp[i].stride = 0;
        svp[i].instance = 0;
      }
   }

   bool StrideValuePredictor::isEligible(unsigned int flags) {
      if(PERFECT_VALUE_PRED)
         return true;
      else if (IS_INTALU(flags))
         return(predINTALU);     // instr. is INTALU type.  It is eligible if predINTALU is configured "true".
      else if (IS_FPALU(flags))
         return(predFPALU);      // instr. is FPALU type.  It is eligible if predFPALU is configured "true".
      else if (IS_LOAD(flags) && !IS_AMO(flags))
         return(predLOAD);      // instr. is a normal LOAD (not rare load-with-reserv).  It is eligible if predLOAD is configured "true".
      else
         return(false);     // instr. is none of the above major types, so it is never eligible
   }

void StrideValuePredictor::printVPQ() {
    // Set column widths for better alignment
    const int columnWidth = 10;

    // Print header row
    cout << setw(columnWidth) << "index"  
         << setw(columnWidth) << "tag" 
         << setw(columnWidth) << "PCindex"
         << setw(columnWidth) << "value" << endl;

    if(vpq.getVPQHeadPhase() == vpq.getVPQTailPhase()) {
        // Print data rows
        for(int i = vpq.getVPQHead(); i < vpq.getVPQTail(); i++) {
            cout << setw(columnWidth) << std::dec << i 
                 << setw(columnWidth) << std::hex << vpq.getVPQTable()[i].tag 
                 << setw(columnWidth) << std::hex << vpq.getVPQTable()[i].pcIndex 
                 << setw(columnWidth) << vpq.getVPQTable()[i].value << endl;
        }
    }
    else {
        // Print data rows for wrapped-around case
        for(int i = vpq.getVPQHead(); i < VPQsize; i++) {
            cout << setw(columnWidth) << std::dec << i 
                 << setw(columnWidth) << std::hex << vpq.getVPQTable()[i].tag 
                 << setw(columnWidth) << std::hex << vpq.getVPQTable()[i].pcIndex 
                 << setw(columnWidth) << vpq.getVPQTable()[i].value << endl;
        }
        for(int i = 0; i < vpq.getVPQTail(); i++) {
            cout << setw(columnWidth) << std::dec << i 
                 << setw(columnWidth) << std::hex << vpq.getVPQTable()[i].tag 
                 << setw(columnWidth) << std::hex << vpq.getVPQTable()[i].pcIndex 
                 << setw(columnWidth) << vpq.getVPQTable()[i].value << endl;
        }
    }
}


void StrideValuePredictor::printSVP() {
    // Set column widths for better alignment
    const int columnWidth = 18;

    // Print header row
    cout << setw(columnWidth) << "SVP entry #:"  
         << setw(columnWidth) << "tag(hex)"
         << setw(columnWidth) << "conf"
         << setw(columnWidth) << "retired_value"
         << setw(columnWidth) << "stride"
         << setw(columnWidth) << "instance" << endl;
    
    // Print data rows
    for(int i = 0; i < MAX_PC_INDEX; i++) {
        cout << setw(columnWidth) << std::dec << i 
             << setw(columnWidth) << std::hex << svp[i].tag 
             << setw(columnWidth) << std::dec << svp[i].conf 
             << setw(columnWidth) << std::dec << svp[i].retVal 
             << setw(columnWidth) << std::dec << svp[i].stride 
             << setw(columnWidth) << std::dec << svp[i].instance << endl;
    }
}

   void StrideValuePredictor::flushSVPInstance(){
      for(int i = 0; i< MAX_PC_INDEX; i++) {
        svp[i].instance = 0;
      }
   }

      uint64_t extractPCIndex(uint64_t addr){
      uint64_t value = addr;
      value = value >> (2);
      uint64_t mask = (1ULL << SVPIndexBits)-1;
      value = value&mask;
      return value;
   }

   uint64_t extractTag(uint64_t addr){
      if(SVPTagBits != 0) { //tag full or partial case
         uint64_t value = addr;
         value = value >> (SVPIndexBits+2);
         uint64_t mask = (1ULL << SVPTagBits)-1;
         value = value&mask;
         return value;
      }
      else return 0; //tagBits = 0 case
   }


   void StrideValuePredictor::train(uint64_t addr, uint64_t value){//will this handle initial values as well?
      uint64_t pcIndex = extractPCIndex(addr);
      uint64_t tag = extractTag(addr);
      
      if(hit(pcIndex, tag)){ //tag hit case
         
         uint64_t newStride = (value - svp[pcIndex].retVal);
         if(newStride == svp[pcIndex].stride){ //no change in stride value
            svp[pcIndex].increamentConf();
         }
         else{ //stride changes
            if(svp[pcIndex].conf <= replace_stride){
               svp[pcIndex].stride = newStride;
            }
            svp[pcIndex].decreamentConf();
         }
         svp[pcIndex].retVal = value;
         svp[pcIndex].decreamentInstanceCounter();
      }
      else if(svp[pcIndex].conf <= REPLACE){
         svp[pcIndex].tag = tag;
         svp[pcIndex].conf = 0;
         svp[pcIndex].retVal = value;
         svp[pcIndex].stride = value;
         svp[pcIndex].instance = vpq.speculateInstance(tag, pcIndex); //walk VPQ H to T to determine instance count.
      }
         vpq.popFromVPQ();
   }

//=======================================================================================================================================================================
   bool StrideValuePredictor::hit(uint64_t index, uint64_t tag) {
      return (svp[index].getTag() == tag); //will this work for no Tag?? skhan25
   }

   void StrideValuePredictor::predValue(uint64_t &prediction, bool &confidence, uint64_t addr){
      uint64_t pcIndex = extractPCIndex(addr);
      uint64_t tag = extractTag(addr);
      prediction = svp[pcIndex].retVal + (svp[pcIndex].instance*svp[pcIndex].stride);
      confidence = ((hit(pcIndex, tag)) && (svp[pcIndex].conf == confMax));
   }
//=======================================================================================================================================================================

//=======================================================================================================================================================================
   bool StrideValuePredictor::isTagMatch(uint64_t tag, uint64_t index){
      if(svp[index].getTag() == tag){ //tag match?
         return true; //entry found in the SVP
      }
      return false;
   }

   svpEntry StrideValuePredictor::createEntryInSVP(uint64_t tag){
      svpEntry newSNPEntry;
      newSNPEntry.setTag(tag);
      newSNPEntry.setInstanceCounter(1);
      newSNPEntry.retVal = 0;
      newSNPEntry.conf = 0;
      newSNPEntry.stride = 0;
      //conf, retVal, stride set as 0 by default
      return newSNPEntry;
   }

   bool StrideValuePredictor::handleSVPEntry(uint64_t addr){// increamets the instance counter based on tagMatch.
      uint64_t tag = extractTag(addr);
      uint64_t pcIndex = extractPCIndex(addr);
      assert(((pcIndex >= 0) && (pcIndex <= MAX_PC_INDEX)) && "StrideValuePredictor::handleSVPEntry: invalid index of svp"); //index out of expected range
      if(isTagMatch(tag, pcIndex)) { //check if tag matches
         svp[pcIndex].increamentInstanceCounter();
         return true;
      }
      return false;
   }

      void StrideValuePredictor::forwardWalkVPQToDecreamentInstance(uint64_t checkpointedVPQTail, bool checkpointedVPQTailPhase){
         bool vpQueueTailPhase = vpq.getVPQTailPhase();
         vector<vpQueueEntry> vpqTable = vpq.getVPQTable();
         uint64_t vpQueue_Tail = vpq.getVPQTail();
         if(checkpointedVPQTailPhase == vpQueueTailPhase){ // H < T
            for(uint64_t i=checkpointedVPQTail; i<vpQueue_Tail; i++){
               if(svp[vpqTable[i].pcIndex].tag == vpqTable[i].tag){
                  svp[vpqTable[i].pcIndex].decreamentInstanceCounter();
               }
            }
         }
         else if(checkpointedVPQTailPhase != vpQueueTailPhase){ // H > T
            for(uint64_t i=checkpointedVPQTail; i<VPQsize; i++){ //VPQSize or VPQSize-1??     // loop for head till end of queue
               if(svp[vpqTable[i].pcIndex].tag == vpqTable[i].tag){
                  svp[vpqTable[i].pcIndex].decreamentInstanceCounter();
               }
            }
            for(uint64_t i=0; i<vpQueue_Tail; i++){ //loop from start till tail.
               if(svp[vpqTable[i].pcIndex].tag == vpqTable[i].tag){
                  svp[vpqTable[i].pcIndex].decreamentInstanceCounter();
               }
            }
         }
      }

   void StrideValuePredictor::rollback(uint64_t checkpointedVPQTail, bool checkpointedVPQTailPhase) {
         forwardWalkVPQToDecreamentInstance(checkpointedVPQTail, checkpointedVPQTailPhase);
         vpq.setVPQTail(checkpointedVPQTail);
         vpq.setVPQTailPhase(checkpointedVPQTailPhase);
   }

