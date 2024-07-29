#include "renamer.h"
#include <assert.h>

using namespace std;

void renamer::initialiseFreeListAsFull(){
    freeList.freeList_Head = 0;
    freeList.freeList_Tail = 0;
    freeList.freeListHeadPhase = false;
    freeList.freeListTailPhase = true;
}

void renamer::initialiseActiveListAsEmpty(){
	activeList.activeList_Head = 0;
    activeList.activeList_Tail = 0;
    activeList.activeListHeadPhase = false;
    activeList.activeListTailPhase = false;
}

void renamer::initialiseActiveListValues(){
	activeListEntry alEntry;
    for(int i=0; i<activeListSize; i++) {
        alEntry.destFlag = false;
        alEntry.logicalRegDest = 0;
        alEntry.physicalRegDest = 0;
        alEntry.isCompleted = false;
        alEntry.isException = false;
        alEntry.isLoadVoilated = false;
        alEntry.isBranchMispredicted = false;
        alEntry.isValueMispredicted = false;
        alEntry.isLoadInstr = false;
        alEntry.isStoreInstr = false;
        alEntry.isBranchInstr = false;
        alEntry.isAtomicMemoryOp = false;
        alEntry.isSystemInstr = false;
        alEntry.pc = 0;
        activeList.activeListTable.push_back(alEntry);
    }
}

renamer::renamer(uint64_t n_log_regs, uint64_t n_phys_regs, uint64_t n_branches, uint64_t n_active){
        assert(n_phys_regs > n_log_regs);
        assert((1 <= n_branches) && (n_branches <= 64));
        assert(n_active > 0);

        freeListSize = n_phys_regs - n_log_regs;
        logicalRegSize = n_log_regs;
        PRFsize = n_phys_regs;
        noOfCheckpoint = n_branches;
        activeListSize = n_active;

		initialiseActiveListAsEmpty();
		initialiseFreeListAsFull();

        for(int i=0; i<logicalRegSize; i++){
            AMT.push_back(i);
            RMT.push_back(i);        
        }

        for(int i=0; i<freeListSize; i++){
            freeList.freeListEntry.push_back(i+logicalRegSize);
        }

		initialiseActiveListValues();

		//initialisation of the PRF
        for(int i=0; i<PRFsize; i++){
            PRF.push_back(0);    
        }

        //initialisation of the PRF Ready
        for(int i=0; i<PRFsize; i++){
	        if(i<logicalRegSize)
    	   	    PRFready.push_back(true);
	        else 
	    	    PRFready.push_back(false);
        }

        //initialisation of the GBM
	    GBM = 0;

    	//initialisation of Branch CheckPoints
        branchCheckpointEntry branchCheckpoint;
        for(int i=0; i<noOfCheckpoint; i++){
            //temp_checkpoint.valid = true;
            branchCheckpoint.freeListHeadCheckpoint = 0;
            branchCheckpoint.freeListHeadPhaseCheckpoint = false;
            branchCheckpoint.Checkpointed_GBM = 0;
            for(int j=0; j<logicalRegSize; j++){
                branchCheckpoint.shadowMapTable.push_back(j);
            }
            branchCheckpointTable.push_back(branchCheckpoint);
        }
    }


uint64_t renamer::getAvailableFreePhysicalReg(){
	uint64_t getAvailableFreePhysicalReg;
	if(freeList.freeListHeadPhase != freeList.freeListTailPhase)
		getAvailableFreePhysicalReg = freeListSize - (freeList.freeList_Head - freeList.freeList_Tail);
	else
		getAvailableFreePhysicalReg = (freeList.freeList_Tail - freeList.freeList_Head);
	return getAvailableFreePhysicalReg;
}

bool renamer::stall_reg(uint64_t bundle_dst) {
	if(getAvailableFreePhysicalReg() < bundle_dst)
		return true; // stall
	else
		return false; // donot stall
}

uint64_t renamer::countZeroBits(){
	uint64_t count = 0;
	uint64_t gbmCopy;
	gbmCopy = GBM;

	for(int i=0; i<noOfCheckpoint; i++) {
		if(!(gbmCopy & 1)) {
			count++;
		}
		gbmCopy = gbmCopy>>1;
	}
	return count;
}

bool renamer::stall_branch(uint64_t bundle_branch) {
	if(countZeroBits() < bundle_branch)
		return true; //stall
	else
		return false; //do not stall
}

uint64_t renamer::get_branch_mask() {
    return GBM;
}

uint64_t renamer::rename_rsrc(uint64_t log_reg){
    return RMT[log_reg];
}

void renamer::advanceFreeListHead(){
	freeList.freeList_Head++;
	if(freeList.freeList_Head == freeListSize)
	{ 
		freeList.freeList_Head = 0;
		freeList.freeListHeadPhase = !freeList.freeListHeadPhase;
	}
}

uint64_t renamer::rename_rdst(uint64_t log_reg){
	uint64_t freePhysicalReg;
	freePhysicalReg = freeList.freeListEntry[freeList.freeList_Head];
	//update RMT for new mapping
	RMT[log_reg] = freePhysicalReg;
	//Head++
	advanceFreeListHead();
	return freePhysicalReg;
}

uint64_t renamer::findFirstzeroBit(){
	uint64_t gbmCopy;
	uint64_t zeroTHBitPoisition;
	gbmCopy = GBM;
	for(int i=0; i<noOfCheckpoint; i++){
		if(!((gbmCopy>>i) & 1)){
			zeroTHBitPoisition = i;	
			break;
		}
		if(i == noOfCheckpoint-1){
			zeroTHBitPoisition = (-1);
		}
	}
	return zeroTHBitPoisition;
}

uint64_t renamer::checkpoint(){
	uint64_t branchID = findFirstzeroBit();
	if(branchID!=(-1)) {
		branchCheckpointTable[branchID].freeListHeadCheckpoint = freeList.freeList_Head;
		branchCheckpointTable[branchID].freeListHeadPhaseCheckpoint = freeList.freeListHeadPhase;
		branchCheckpointTable[branchID].Checkpointed_GBM = GBM;
		branchCheckpointTable[branchID].shadowMapTable = RMT;
		GBM = GBM | (1<<branchID);
	}
	return branchID;
}

uint64_t renamer::getAvailableActiveListEntry(){
	uint64_t freeSpaceInActiveList;
	uint64_t userActiveList;
	if(activeList.activeListHeadPhase != activeList.activeListTailPhase)
		userActiveList = activeListSize - (activeList.activeList_Head - activeList.activeList_Tail);
	else
		userActiveList = (activeList.activeList_Tail - activeList.activeList_Head);
	freeSpaceInActiveList = activeListSize - userActiveList; 
	return freeSpaceInActiveList;
}

// uint64_t renamer::getAvailableActiveListEntry(){
// 	uint64_t freeReg = 0;
//     if(activeList.activeListHeadPhase > activeList.activeListTailPhase)
//         freeReg = activeListSize - (activeList.activeList_Head - activeList.activeList_Tail);
//     else if(activeList.activeListTailPhase > activeList.activeListHeadPhase)
//         freeReg = activeListSize - (activeList.activeList_Tail - activeList.activeList_Head);
//     else if((activeList.activeList_Head == activeList.activeList_Tail) && (activeList.activeListTailPhase == activeList.activeListHeadPhase))
//         freeReg = activeListSize; //empty Active List
//     else if((activeList.activeList_Head == activeList.activeList_Tail) && (activeList.activeListTailPhase != activeList.activeListHeadPhase))
//         freeReg = 0; //full Active list 
//     return freeReg;
// }

bool renamer::stall_dispatch(uint64_t bundle_inst){
	uint64_t availableSpaceInActiveList = getAvailableActiveListEntry();
	if(availableSpaceInActiveList < bundle_inst)
		return true; //stall
	else
        return false; //do not stall
}

void renamer::advanceActiveListTail(){
	activeList.activeList_Tail++;
	if(activeList.activeList_Tail == freeListSize){ 
		activeList.activeList_Tail = 0;
		activeList.activeListTailPhase = !activeList.activeListTailPhase;
	}
}

uint64_t renamer::dispatch_inst(bool dest_valid,
                       uint64_t log_reg,
                       uint64_t phys_reg,
                       bool load,
                       bool store,
                       bool branch,
                       bool amo,
                       bool csr,
                       uint64_t PC)
{
	assert(!((activeList.activeList_Head == activeList.activeList_Tail) && (activeList.activeListHeadPhase != activeList.activeListTailPhase)));
	uint64_t AL_index;
	AL_index = activeList.activeList_Tail;
	activeList.activeListTable[AL_index].destFlag = dest_valid;
	activeList.activeListTable[AL_index].logicalRegDest = log_reg;
	activeList.activeListTable[AL_index].physicalRegDest = phys_reg;
	activeList.activeListTable[AL_index].isCompleted   = false; 
	activeList.activeListTable[AL_index].isException = false; 
	activeList.activeListTable[AL_index].isBranchMispredicted = false; 
	activeList.activeListTable[AL_index].isValueMispredicted = false; 
	activeList.activeListTable[AL_index].isLoadVoilated = false;
	activeList.activeListTable[AL_index].isLoadInstr = load;
	activeList.activeListTable[AL_index].isStoreInstr = store;
	activeList.activeListTable[AL_index].isBranchInstr = branch;
	activeList.activeListTable[AL_index].isAtomicMemoryOp = amo;
	activeList.activeListTable[AL_index].isSystemInstr = csr;
	activeList.activeListTable[AL_index].pc = PC;

	//advance active list tail
	advanceActiveListTail();
	return AL_index;
}

bool renamer::is_ready(uint64_t phys_reg){
	return PRFready[phys_reg];
}

void renamer::clear_ready(uint64_t phys_reg){
	PRFready[phys_reg] = false;
}

uint64_t renamer::read(uint64_t phys_reg) {
	return PRF[phys_reg];
}

void renamer::set_ready(uint64_t phys_reg) {
	PRFready[phys_reg] = true;
}

void renamer::write(uint64_t phys_reg, uint64_t value) {
	PRF[phys_reg] = value;
}

void renamer::set_complete(uint64_t AL_index) {
	activeList.activeListTable[AL_index].isCompleted = true;
}

void renamer::resolve(uint64_t AL_index, uint64_t branch_ID, bool correct){
	assert((GBM>>branch_ID) & 1); //sanity check
	uint64_t mask = ~(1<<branch_ID);
	if(correct){
		// 1. clear the branchId bit in GBM
		GBM &= mask;
		// 2. clear the branchID bit in all the checkpointed GBM
		for(int i=0; i<noOfCheckpoint; i++){
			branchCheckpointTable[i].Checkpointed_GBM &= mask;
		}
	}
	else{
		// 1. Restore GBM
		GBM = branchCheckpointTable[branch_ID].Checkpointed_GBM;
		GBM &= mask;
		// 2. restore RMT
		RMT = branchCheckpointTable[branch_ID].shadowMapTable;
        // 3. restore freelist
		freeList.freeList_Head = branchCheckpointTable[branch_ID].freeListHeadCheckpoint;
		freeList.freeListHeadPhase = branchCheckpointTable[branch_ID].freeListHeadPhaseCheckpoint;
		// 4. restore activeList pointers
		if(AL_index != activeListSize -1)
			activeList.activeList_Tail = AL_index + 1;
		else
			activeList.activeList_Tail = 0;
		if(activeList.activeList_Tail <= activeList.activeList_Head)
			activeList.activeListTailPhase = !activeList.activeListHeadPhase;	
		else 
			activeList.activeListTailPhase = activeList.activeListHeadPhase;
	}
}

bool renamer::precommit(bool &completed, bool &exception, bool &load_viol, bool &br_misp, bool &val_misp, bool &load, bool &store, bool &branch, bool &amo, bool &csr, uint64_t &PC) {
	if((activeList.activeListHeadPhase == activeList.activeListTailPhase) && (activeList.activeList_Tail == activeList.activeList_Head)) {
		return false; 
	}
	else {
		uint64_t AL_head = activeList.activeList_Head;
		completed = activeList.activeListTable[AL_head].isCompleted;
		exception = activeList.activeListTable[AL_head].isException;
		load_viol = activeList.activeListTable[AL_head].isLoadVoilated;
		br_misp = activeList.activeListTable[AL_head].isBranchMispredicted;
		val_misp = activeList.activeListTable[AL_head].isValueMispredicted;
		load = activeList.activeListTable[AL_head].isLoadInstr;
		store = activeList.activeListTable[AL_head].isStoreInstr;
		branch = activeList.activeListTable[AL_head].isBranchInstr;
		amo = activeList.activeListTable[AL_head].isAtomicMemoryOp;
		csr  = activeList.activeListTable[AL_head].isSystemInstr;
		PC = activeList.activeListTable[AL_head].pc;
		return true;
	}
}

void renamer::advanceFreeListTail(){
	freeList.freeList_Tail++;
	if(freeList.freeList_Tail == freeListSize){ 
		freeList.freeList_Tail = 0;
		freeList.freeListTailPhase = !freeList.freeListTailPhase;
	}
}

void renamer::advanceActiveListHead(){
	activeList.activeList_Head++;
	if(activeList.activeList_Head == activeListSize){
		activeList.activeList_Head = 0;
		activeList.activeListHeadPhase = !activeList.activeListHeadPhase;
	}
}

bool renamer::isActiveListEmpty(){
	return ((activeList.activeListHeadPhase == activeList.activeListTailPhase) && (activeList.activeList_Tail == activeList.activeList_Head));
}

void renamer::commit() {
	assert(!(isActiveListEmpty())); //activeList not empty assertion
	assert(activeList.activeListTable[activeList.activeList_Head].isCompleted);
	assert(!activeList.activeListTable[activeList.activeList_Head].isException);
	assert(!activeList.activeListTable[activeList.activeList_Head].isLoadVoilated);

	uint64_t activeListHead = activeList.activeList_Head;
	if(activeList.activeListTable[activeListHead].destFlag == true){
		uint64_t freePhysicalRegister = AMT[activeList.activeListTable[activeListHead].logicalRegDest];
		freeList.freeListEntry[freeList.freeList_Tail] = freePhysicalRegister;
		AMT[activeList.activeListTable[activeListHead].logicalRegDest] = activeList.activeListTable[activeListHead].physicalRegDest;
		//Advance FreeList Tail pointer
		advanceFreeListTail();
	}
	//Advance ActiveList haed pointer
	advanceActiveListHead();
}

void renamer::squash() {
	//Empty the active list
	activeList.activeList_Tail = activeList.activeList_Head;
	activeList.activeListTailPhase = activeList.activeListHeadPhase;

	//Make the free list full
	freeList.freeList_Head =  freeList.freeList_Tail;
	freeList.freeListHeadPhase  = !freeList.freeListTailPhase;

	//Copy AMT to RMT
	RMT = AMT;

	 for(int i=0; i<activeListSize; i++){
        activeList.activeListTable[i].destFlag = false;
		activeList.activeListTable[i].logicalRegDest = 0;
		activeList.activeListTable[i].physicalRegDest = 0;
		activeList.activeListTable[i].isCompleted = false;
		activeList.activeListTable[i].isException = false;
		activeList.activeListTable[i].isLoadVoilated = false;
		activeList.activeListTable[i].isBranchMispredicted = false;
		activeList.activeListTable[i].isValueMispredicted = false;
		activeList.activeListTable[i].isLoadInstr = false;
		activeList.activeListTable[i].isStoreInstr = false;
		activeList.activeListTable[i].isBranchInstr = false;
		activeList.activeListTable[i].isAtomicMemoryOp = false;
		activeList.activeListTable[i].isSystemInstr = false;
		activeList.activeListTable[i].pc = 0;
    }

	for(int i=0; i<PRFsize; i++){
		PRFready[i]= true;
    }

	for(int i=0; i<logicalRegSize; i++){
	 PRFready[AMT[i]] = true;
	} 

	GBM = 0;

	//initialisation of Branch CheckPoints
	for(int i=0; i<noOfCheckpoint ;i++){
		branchCheckpointTable[i].freeListHeadCheckpoint = 0;
		branchCheckpointTable[i].freeListHeadPhaseCheckpoint = false;
		branchCheckpointTable[i].Checkpointed_GBM = 0;
		for(int j=0; j<logicalRegSize; j++){
			branchCheckpointTable[i].shadowMapTable[j] = j;
		}
    }
}

void renamer::set_exception(uint64_t AL_index) {
	activeList.activeListTable[AL_index].isException = true;
}
void renamer::set_load_violation(uint64_t AL_index) {
	activeList.activeListTable[AL_index].isLoadVoilated = true;
}
void renamer::set_branch_misprediction(uint64_t AL_index) {
	activeList.activeListTable[AL_index].isBranchMispredicted = true;
}
void renamer::set_value_misprediction(uint64_t AL_index) {
	activeList.activeListTable[AL_index].isValueMispredicted = true;
}

bool renamer::get_exception(uint64_t AL_index) {
	return activeList.activeListTable[AL_index].isException;
}