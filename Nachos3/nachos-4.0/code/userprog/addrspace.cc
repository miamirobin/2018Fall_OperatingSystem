// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

bool AddrSpace::isPhyPageUsed[NumPhysPages]={0};
static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace()
{
    Id_= kernel->machine->Id_+1;
    kernel->machine->Id_++;
    
    // zero out the entire address space
//    bzero(kernel->machine->mainMemory, MemorySize);
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   for(unsigned int i = 0; i < numPages; i++){
	AddrSpace::isPhyPageUsed[pageTable[i].physicalPage]= FALSE;
   }
   delete pageTable;
}


//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

bool 
AddrSpace::Load(char *fileName) 
{
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;

    if (executable == NULL) {
	cerr << "Unable to open file " << fileName << "\n";
	return FALSE;
    }
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
 
    pageTable = new TranslationEntry[numPages];
    for (unsigned int  j= 0, k=0; j < numPages; j++) {
        pageTable[j].virtualPage = j;   // for now, virt page # = phys page #
        while ( true){
           if (k< NumPhysPages and AddrSpace::isPhyPageUsed[k] ){
               k++;
           }
           else {
               break;
           }
        }
        AddrSpace::isPhyPageUsed[k] = TRUE;
        pageTable[j].physicalPage = k;
        pageTable[j].valid = TRUE;
        pageTable[j].use = FALSE;
        pageTable[j].dirty = FALSE;
        pageTable[j].readOnly = FALSE;
    }  

    //cout<<"OKOK\n";
    size = numPages * PageSize;

// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
  
		for(unsigned int i =0, j=0 ; i<numPages ; i++){
		    
		    while(kernel->machine->isPhyPageUsed[j]==TRUE ){//find the unused physical page
                        j++;
                        if (j>=NumPhysPages){
                            break;
                        }
                    }
		    //if physical memory is enough, we put data directly in without virtual memory
		    if( j < NumPhysPages){
			kernel->machine->PhyPageName[j]= Id_;// record id to physical page
                        kernel->machine->isPhyPageUsed[j] = TRUE;//record this physical page has been used
			kernel->machine->Main[j]= &pageTable[i];//record the address
			pageTable[i].physicalPage = j;//record the corresponding physical page
                        pageTable[i].Id_=Id_;
			pageTable[i].valid = TRUE;
			pageTable[i].use = FALSE;
			pageTable[i].dirty = FALSE;
			pageTable[i].readOnly = FALSE;

		        executable->ReadAt(&(kernel->machine->mainMemory[ PageSize*j ]), PageSize, 
                                             noffH.code.inFileAddr+( PageSize*i ));

		    }
                    else{ // if physical memory is not enough, then we use virtual memory
			   
	  	         char *virMemory = new char[PageSize];
			 unsigned int k=0;
			 while (kernel->machine->isVirPageUsed[k]==TRUE){//find the unused virtual page
                                k++;
                         }
			 kernel->machine->isVirPageUsed[k] = TRUE;//record this virtual page has been used
			 pageTable[i].virtualPage=k;//record the corresponding virtual page
                         pageTable[i].Id_=Id_;
			 pageTable[i].valid=FALSE;
			 pageTable[i].use=FALSE;
			 pageTable[i].dirty=FALSE;
			 pageTable[i].readOnly=FALSE;
			 executable->ReadAt( virMemory, PageSize, noffH.code.inFileAddr+(PageSize*i ));
			 kernel->vmswap->WriteSector(k, virMemory); //Write in virtual memory
		    }
		}
    }
    if (noffH.initData.size > 0) {
  
	    executable->ReadAt(&(kernel->machine->mainMemory[noffH.initData.virtualAddr]),noffH.initData.size, noffH.initData.inFileAddr);
    }

    delete executable;			// close file
    return TRUE;			// success
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program.  Load the executable into memory, then
//	(for now) use our own thread to run it.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

void 
AddrSpace::Execute(char *fileName) 
{
    isPageTableLoad = FALSE;
    if (!Load(fileName)) {
	cout << "inside !Load(FileName)" << endl;
	return;				// executable not found
    }

    //kernel->currentThread->space = this;
    this->InitRegisters();		// set the initial register values
    this->RestoreState();		// load page table register
    isPageTableLoad=TRUE;
    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);
    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{
	if( isPageTableLoad == TRUE){ 
        	pageTable=kernel->machine->pageTable;
        	numPages=kernel->machine->pageTableSize;
	}
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
}