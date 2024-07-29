#include <vector>
#include <inttypes.h>
#include <assert.h>
#include <iostream>
#include <limits>
#include <parameters.h>
#include <iostream>

   using namespace std;

// have a method to get to know if to stall or not based on emptiness of VPQ?
   struct vpQueueEntry {
      uint64_t pcIndex;
      uint64_t tag;
      uint64_t value;  //potential to be discussed 
   };


   class vpQueue {
   private:
      // public:
        uint64_t vpQueue_Head, vpQueue_Tail;
        bool vpQueueHeadPhase, vpQueueTailPhase;
        vector<vpQueueEntry> vpQueueTable; //value prediction queue used to train the stride value predictor


   void advanceVPQHead() {
      vpQueue_Head++;
      if(vpQueue_Head == VPQsize) {
         vpQueue_Head = 0;
         vpQueueHeadPhase = !vpQueueHeadPhase;
      }
   }

   vpQueueEntry getVPQEntry(uint64_t tag, uint64_t pcIndex) {
      vpQueueEntry vpqEntry;
      vpqEntry.tag = tag;
      vpqEntry.pcIndex = pcIndex;
      vpqEntry.value = 0; //can be used for assertion? like, at the time of training from VPQ, the value should not be (UINT64_MAX)
      return vpqEntry;
   }

   bool isVPQFull() {
      if ((vpQueue_Head == vpQueue_Tail) && (vpQueueHeadPhase != vpQueueTailPhase))
         return true;
      else
         return false;
   }

   uint64_t pushIntoVPQ(uint64_t tag, uint64_t pcIndex ){ // might need an index? //first check the VPQ should not be full
      uint64_t vpqT = vpQueue_Tail;
      assert(isVPQFull() == false && "vpQueue::pushIntoVPQ(): The VPQ is full, cannot create new entry");
      vpQueueEntry vpqEntry = getVPQEntry(tag, pcIndex);
      vpQueueTable[vpQueue_Tail] = vpqEntry; //best way??
      advanceVPQTail();
      return vpqT;
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

   void initialiseVPQTableEntryAs0(){
      vpQueueTable.resize(VPQsize);
      for(int i=0; i<VPQsize; i++){
         vpQueueTable[i].pcIndex = 0;
         vpQueueTable[i].tag = 0;
         vpQueueTable[i].value = 0;
      }
      vpQueue_Head = 0;
      vpQueue_Tail = 0;
      vpQueueHeadPhase = false;
      vpQueueTailPhase = false;
   }

   public:
      vpQueue() {
         initialiseVPQTableEntryAs0();
      }

      uint64_t getVPQHead(){
         return vpQueue_Head;
      }

      uint64_t getVPQTail(){
         return vpQueue_Tail;
      }

      bool getVPQTailPhase(){
         return vpQueueTailPhase;
      }

      bool getVPQHeadPhase(){
         return vpQueueHeadPhase;
      }

      vector<vpQueueEntry> getVPQTable(){
         return vpQueueTable;
      }

      void setVPQTail(uint64_t vpqTail){
         this->vpQueue_Tail = vpqTail;
      }

      void setVPQHead(uint64_t vpqHead){
         this->vpQueue_Head = vpqHead;
      }

      void setVPQTailPhase(bool vpqTailPhase){
         this->vpQueueTailPhase = vpqTailPhase;
      }

      void setVPQHeadPhase(bool vpqHeadPhase){
         this->vpQueueHeadPhase = vpqHeadPhase;
      }

      bool isVPQEmpty() {
         // cout << "In VPQISEmpty vpQueue_Head = " << vpQueue_Head << endl;
         // cout << "In VPQISEmpty vpQueue_Tail = " << vpQueue_Tail << endl;
         // cout << "In VPQISEmpty vpQueueHeadPhase = " << vpQueueHeadPhase << endl;
         // cout << "In VPQISEmpty vpQueueTailPhase = " << vpQueueTailPhase << endl;
         if ((vpQueue_Head == vpQueue_Tail) && (vpQueueHeadPhase == vpQueueTailPhase))
            return true;
         else
            return false;
      }

      void popFromVPQ() { //first check the VPQ should not be empty -- called in retire
         assert(isVPQEmpty() == false && "vpQueue::popFromVPQ(): The VPQ is empty already, cannot pop more");
         //rather just increament head pointer and do not erarse the value there.
         vpQueueTable[vpQueue_Head].tag = 0;
         vpQueueTable[vpQueue_Head].pcIndex = 0;
         vpQueueTable[vpQueue_Head].value = 0;
         advanceVPQHead();
      }

      void advanceVPQTail() {
         // cout << "In VPQueue: Tail before advancing " << this->vpQueue_Tail << " \t Tail Phase " << this->vpQueueTailPhase << endl;
         this->vpQueue_Tail++;
         if(this->vpQueue_Tail == VPQsize){ 
            this->vpQueue_Tail = 0;
            vpQueueTailPhase = !vpQueueTailPhase;
         }
         // cout << "In VPQueue: Tail after advancing " << this->vpQueue_Tail << " \t Tail Phase " << this->vpQueueTailPhase << endl;
      }

//TODo - assertion for (tag bits <= 62-index bits)
      uint64_t setVPQTag(uint64_t addr){ // called in renamer
         uint64_t tag = extractTag(addr);
         uint64_t pcIndex = extractPCIndex(addr);
         uint64_t vpqT = pushIntoVPQ(tag, pcIndex);
         return vpqT;
      }

      void setVPQValue(uint64_t addr, uint64_t value, uint64_t vpqTail){   // computed value from WB
      //get a parameter from payload as index of vpqTable, and  assert for the tag match at that index
         uint64_t tag = extractTag(addr);
         uint64_t pcIndex = extractPCIndex(addr);
         // cout << "printing before assertion of failure : vpQueueTable[vpqTail] with vpqTail = " << vpqTail << " --> \n pcIndex =  " << vpQueueTable[vpqTail].pcIndex << " \n tag " << vpQueueTable[vpqTail].tag << " \n value " << vpQueueTable[vpqTail].value << endl;
         assert(((vpQueueTable[vpqTail].tag == tag) && (vpQueueTable[vpqTail].pcIndex == pcIndex)) && "vpQueue::setVPQValue(): there is tag and or index mismatch for the vpQueueTable[index] and tag from instrcution address"); 
         vpQueueTable[vpqTail].value = value;
      }


      uint64_t getAvailableEntriesInVPQ(){
         uint64_t getAvailableFreeVPQEntries = 0;
	      if(vpQueueHeadPhase == vpQueueTailPhase)
            getAvailableFreeVPQEntries = VPQsize - (vpQueue_Tail - vpQueue_Head);
	      else
	      	getAvailableFreeVPQEntries = (vpQueue_Head - vpQueue_Tail);
         return getAvailableFreeVPQEntries;
      }

      bool checkVPQForEnoughEntries(uint64_t vpEligibleEntries){  //mona.lisa2
            uint64_t getAvailableFreeVPQEntries = getAvailableEntriesInVPQ();      
         if(getAvailableFreeVPQEntries >= vpEligibleEntries)
            return true; //enough entries exist, Don't stall.
         else 
            return false; //not enough entries, Please stall.
      }

      bool stall_value(uint64_t vpEligibleEntries){
         if(!(PERFECT_VALUE_PRED)){ //only for oracle/real mode
            if(!VPQ_full_policy){ // ==0
               // checkfor entries in VPQ, if not stall
               return (!checkVPQForEnoughEntries(vpEligibleEntries)); // false = Dont stall, True = stall
            }
            if(VPQ_full_policy){ //==1
               return false;
            }
         }
         return false; //don't stall for PERFECT_VALUE_PRED
      }

      uint64_t speculateInstance(uint64_t tag, uint64_t pcIndex){
         uint64_t instanceCounter=0;
         if(vpQueueHeadPhase == vpQueueTailPhase){ // H < T
            for(uint64_t i=vpQueue_Head; i<vpQueue_Tail; i++){
               if((vpQueueTable[i].tag == tag) &&  (vpQueueTable[i].pcIndex == pcIndex))
                  instanceCounter++;
            }
         }
         else if(vpQueueHeadPhase != vpQueueTailPhase){ // H > T
            for(uint64_t i=vpQueue_Head; i<VPQsize; i++){ //VPQSize or VPQSize-1??     // loop for head till end of queue
               if((vpQueueTable[i].tag == tag) && (vpQueueTable[i].pcIndex == pcIndex))
                  instanceCounter++;
            }
            for(uint64_t i=0; i<vpQueue_Tail; i++){ //loop from start till tail.
               if((vpQueueTable[i].tag == tag) && (vpQueueTable[i].pcIndex == pcIndex))
                  instanceCounter++;
            }
         }
         return instanceCounter;
      }

      void squashVPQ(){
         // if(vpQueueHeadPhase == vpQueueTailPhase){ // H < T
         //    for(uint64_t i=vpQueue_Head; i<vpQueue_Tail; i++){
         //       vpQueueTable[i].pcIndex = 0;
         //       vpQueueTable[i].tag = 0;
         //       vpQueueTable[i].value = 0;
         //    }
         // }
         // else if(vpQueueHeadPhase != vpQueueTailPhase){ // H > T
         //    for(uint64_t i=vpQueue_Head; i<VPQsize; i++){ //VPQSize or VPQSize-1??     // loop for head till end of queue
         //       vpQueueTable[i].pcIndex = 0;
         //       vpQueueTable[i].tag = 0;
         //       vpQueueTable[i].value = 0;
         //    }
         //    for(uint64_t i=0; i<vpQueue_Tail; i++){ //loop from start till tail.
         //       vpQueueTable[i].pcIndex = 0;
         //       vpQueueTable[i].tag = 0;
         //       vpQueueTable[i].value = 0;
         //    }
         // }
         vpQueue_Tail = vpQueue_Head;
         vpQueueTailPhase = vpQueueHeadPhase;
      }

//==========================================================Debug Methods==================================================================================================
   //   void printVpQueueTable(){
   //      std::cout << "vpQueueTable elements:" << std::endl;
   //      for (int i=0; i<vpQueueTable.size(); i++) {
   //         vpQueueEntry entry = vpQueueTable[i];
   //         std::cout << "Tag: " << entry.tag << ", Value: " << entry.value <<std::endl;
   //      }
   //   }
//
   //   void printAttributes(){
   //      cout << "vpQueue_Head = " << vpQueue_Head << endl;
   //      cout << "vpQueue_Tail = " << vpQueue_Tail << endl;
   //      cout << "vpQueueHeadPhase = " << vpQueueHeadPhase << endl;
   //      cout << "vpQueueTailPhase = " << vpQueueTailPhase << endl;
   //   }
//==========================================================Debug Methods==================================================================================================

   }; //end of class

//==========================================================Unit Test==================================================================================================

   // int main(){
   //    uint64_t sizeofVPQ = 10;
   //    vpQueue vpq(sizeofVPQ);
   //    cout<< "constructor called"<< endl;
   //    vpq.printVpQueueTable();
   //    vpq.printAttributes();
   //    for(int i=0; i<sizeofVPQ; i++)
   //       vpq.setVPQTag(12345+i, sizeofVPQ);
   //    cout<< "vpq.setVPQTag called"<< endl;
   //    vpq.printVpQueueTable();
   //    vpq.printAttributes();
   //    for(int i=0; i<sizeofVPQ; i++)
   //       vpq.setVPQValue(i, 12345+i, 10000+i);
   //    cout<< "vpq.setVPQValue called"<< endl;
   //    vpq.printVpQueueTable();
   //    vpq.printAttributes();
   //    for(int i=0; i<sizeofVPQ; i++)
   //       vpq.popFromVPQ(sizeofVPQ);
   //    cout<< "vpq.popFromVPQ(); called"<< endl;
   //    vpq.printVpQueueTable();
   //    vpq.printAttributes();
   // }

//==========================================================Unit Test==================================================================================================
