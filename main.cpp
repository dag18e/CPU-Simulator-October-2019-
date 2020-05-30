/*
~Instructions~
1. Run g++ -std=c++11 Project1New.cpp
2. Run a.out (inputFile) (outputFile)

Note: test cases are labeled t(1-7)

*/
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <cctype>
#include <vector>
#include <bitset>
#include <cstring>

using namespace std;

//for holding an entire line of assembly code
struct AssemblyLine
{
  string label;
  string instruction;
  string field0;
  string field1;
  string field2;
  string comment;
  int PC;
  string binary;
  int binint;
  string mips;
  bool beqlabel = false;
};



#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define CMOV 5
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

typedef struct IFIDStruct {
    int instr;
    int pcPlus1;
} IFIDType;

typedef struct IDEXStruct {
    int instr;
    int pcPlus1;
    int readRegA;
    int readRegB;
    int offset;
} IDEXType;

typedef struct EXMEMStruct {
    int instr;
    int branchTarget;
    int aluResult;
    int readRegB;
} EXMEMType;

typedef struct MEMWBStruct {
    int instr;
    int writeData;
} MEMWBType;

typedef struct WBENDStruct {
    int instr;
    int writeData;
} WBENDType;

typedef struct stateStruct {
    int pc;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
    IFIDType IFID;
    IDEXType IDEX;
    EXMEMType EXMEM;
    MEMWBType MEMWB;
    WBENDType WBEND;
    int cycles; /* number of cycles run so far */
} stateType;



void toBinary(string&);
void fileRead(vector<AssemblyLine> &bitArray, int &PC, string argv);             // char * argv[]);
void instructionToBinary(string &instruction);
void fieldToBinary(string &field);
void labelToAddress(string &label, vector<AssemblyLine> &bitArray, const int i);
void fieldLabelHandler(vector<AssemblyLine> &bitArray);
void theConcatenator(vector<AssemblyLine> &bitArray);

void printState(stateType *statePtr);
int field0(int instruction);
int field1(int instruction);
int field2(int instruction);
int opcode(int instruction);
void printInstruction(int instr);
int signExtend(int);


int main()
{
  string argv = "test3.a";


  //The program counter
  int PC = 0;

  //vectors for storing lines of assembly code and labels with their pc
  vector<AssemblyLine> bitArray;

  // reads the file and fills the bitArray
  fileRead(bitArray, PC, argv);

  //goes through the bitArray to find labels in the fields
  fieldLabelHandler(bitArray);

  //starts converting everything to binary, line by line
  for(int i = 0; i < PC; i++)
  {
    //translates opcode to binary
    instructionToBinary(bitArray[i].instruction);

    //translates fields to binary for R-Type instructions
    if(bitArray[i].instruction == "000" || bitArray[i].instruction == "001" ||
       bitArray[i].instruction == "101")
    {
      fieldToBinary(bitArray[i].field0);
      fieldToBinary(bitArray[i].field1);
      fieldToBinary(bitArray[i].field2);
    }

    //translates fields to binary for I-Type instructions
    else if(bitArray[i].instruction == "010"||bitArray[i].instruction == "011")
    {
      fieldToBinary(bitArray[i].field0);
      fieldToBinary(bitArray[i].field1);

      //checks to make sure offset is valid
      if(stoi(bitArray[i].field2) < -32768 || stoi(bitArray[i].field2) > 32767)
      {
        cerr << "Error: Invalid offset field on line " << i;
        exit(1);
      }

      toBinary(bitArray[i].field2);
    }

    //translates fields to binary for beq
    else if(bitArray[i].instruction == "100")
    {
      fieldToBinary(bitArray[i].field0);
      fieldToBinary(bitArray[i].field1);

      //checks to make sure offset is valid
      if(stoi(bitArray[i].field2) < -32768 || stoi(bitArray[i].field2) > 32767)
      {
        cerr << "Error: Invalid offset field on line " << i;
        exit(1);
      }

      //calculates offset in decimal
      if(bitArray[i].beqlabel)
        bitArray[i].field2 = to_string(stoi(bitArray[i].field2) - bitArray[i].PC - 1);

      toBinary(bitArray[i].field2);
    }
  }


  //checks to make sure we have no duplicate labels
  for(int i = 0; i < bitArray.size() - 1; i++)
  {
    if(!bitArray[i].label.empty())
    {
      for(int j = 0; j < bitArray.size() - 1; j++)
      {
        if(bitArray[i].label == bitArray[j].label && i != j)
        {
          cerr << "Error: Duplicate label on line " << i << endl;
          exit(1);
        }
      }
    }
  }


  theConcatenator(bitArray);




///////////////////////////begin pipelining assignment/////////////////////////


  stateStruct state, newState;
  int tempOffset;

  //set up our initial state
  state.pc = 0;
  state.cycles = 0;
  state.numMemory = 0;
  for(int i = 0; i < NUMREGS; i++)
    state.reg[i] = 0;

  //fills in our state's data memory
  for(int i = 0; i < bitArray.size() - 1; i++)
  {
    if(bitArray[i].instruction != ".fill")
    {
      //converts to decimal then to binary
      bitset<32> bitsetBinary (bitArray[i].binary);
      bitArray[i].binint = bitsetBinary.to_ulong();
      state.dataMem[i] = bitArray[i].binint;
    }
    else
      state.dataMem[i] = stoi(bitArray[i].field0);

    state.instrMem[i] = state.dataMem[i];

    state.numMemory++;
  }

  //initialize instruction fields in pipeline registers to noop
  state.IFID.instr = NOOPINSTRUCTION;
  state.IDEX.instr = NOOPINSTRUCTION;
  state.EXMEM.instr = NOOPINSTRUCTION;
  state.MEMWB.instr = NOOPINSTRUCTION;
  state.WBEND.instr = NOOPINSTRUCTION;

  while (1)
  {
    printState(&state);

    /* check for halt */
    if (opcode(state.MEMWB.instr) == HALT) {
        printf("machine halted\n");
        printf("total of %d cycles executed\n", state.cycles);
        exit(0);
    }

    //checks if we are exceeding the instruction memory, which means there was
    //likely no halt instruction given
    if(state.numMemory < state.pc - 1)
    {
      cerr << "\nprogram counter exceeded numMemory\n";
      exit(1);
    }

    newState = state;
    newState.cycles++;

    /* --------------------- IF stage --------------------- */
    //does pc+1
    newState.pc = newState.IFID.pcPlus1 = state.pc + 1;

    //gets first instruction from memory
    newState.IFID.instr = state.instrMem[state.pc];


    /* --------------------- ID stage --------------------- */

    //pushes along data from IFID to IDEX
    newState.IDEX.pcPlus1 = state.IFID.pcPlus1;
    newState.IDEX.instr = state.IFID.instr;


    //Decodes the instruction and puts into regA and regB
    newState.IDEX.readRegA = state.reg[field0(state.IFID.instr)];
    newState.IDEX.readRegB = state.reg[field1(state.IFID.instr)];


    //create temporary offset in case we need to sign extend it
    tempOffset = field2(state.IFID.instr);

    //signextends if necessary
    if (field2(state.IFID.instr) & 1<<15 )
      tempOffset -= 1<<16;

    //assigns our tempOffset to the offset pipeline register
    newState.IDEX.offset = tempOffset;


    //stalls if the op is a load word and the next value of regA or regB        ////////////////////////////////////
    //match the destination register of the lw
    if(opcode(state.IDEX.instr) == LW &&
      (field0(state.IFID.instr) == field1(state.IDEX.instr) ||
       field1(state.IFID.instr) == field1(state.IDEX.instr)))
    {
      //sends noop to execute and sets the pc and state of IFID back
      newState.IDEX.instr = NOOPINSTRUCTION;
      newState.pc = state.pc;
      newState.IFID.pcPlus1 = state.IFID.pcPlus1;
      newState.IFID.instr = state.IFID.instr;
    }


    /* --------------------- EX stage --------------------- */

    //pushes along instruction data from IDEX to EXMEM
    newState.EXMEM.instr = state.IDEX.instr;



    //this section handles data forwarding
    //If either of these two return true, then data is forwarded from the alu
    if(field0(state.IDEX.instr) == field2(state.EXMEM.instr))
    {
      if(opcode(state.EXMEM.instr) == ADD || opcode(state.EXMEM.instr) == NAND
         || opcode(state.EXMEM.instr) == CMOV)
      {
        state.IDEX.readRegA = state.EXMEM.aluResult;
      }
    }
    if(field1(state.IDEX.instr) == field2(state.EXMEM.instr))
    {
      if(opcode(state.EXMEM.instr) == ADD || opcode(state.EXMEM.instr) == NAND
         || opcode(state.EXMEM.instr) == CMOV)
      {
        state.IDEX.readRegB = state.EXMEM.aluResult;
      }
    }


    //If any of these four return true, then data is forwarded from MEMWB
    if(field0(state.IDEX.instr) == field2(state.MEMWB.instr))
    {
      if(opcode(state.MEMWB.instr) == ADD || opcode(state.MEMWB.instr) == NAND
         || opcode(state.MEMWB.instr) == CMOV)
      {
        state.IDEX.readRegA = state.MEMWB.writeData;
      }
    }
    if(field1(state.IDEX.instr) == field2(state.MEMWB.instr))
    {
      if(opcode(state.MEMWB.instr) == ADD || opcode(state.MEMWB.instr) == NAND
         || opcode(state.MEMWB.instr) == CMOV)
      {
        state.IDEX.readRegB = state.MEMWB.writeData;
      }
    }
    if(field0(state.IDEX.instr) == field1(state.MEMWB.instr))
    {
      if(opcode(state.MEMWB.instr) == LW)
      {
        state.IDEX.readRegA = state.MEMWB.writeData;
      }
    }

    if(field1(state.IDEX.instr) == field1(state.MEMWB.instr))
    {
      if(opcode(state.MEMWB.instr) == LW)
      {
        state.IDEX.readRegB = state.MEMWB.writeData;
      }
    }




    //If any of these four return true, then data is forwarded from WBEND
    if(field0(state.IDEX.instr) == field2(state.WBEND.instr))
    {
      if(opcode(state.WBEND.instr) == ADD || opcode(state.WBEND.instr) == NAND
         || opcode(state.WBEND.instr) == CMOV)
      {
        state.IDEX.readRegA = state.WBEND.writeData;
      }
    }
    if(field1(state.IDEX.instr) == field2(state.WBEND.instr))
    {
      if(opcode(state.WBEND.instr) == ADD || opcode(state.WBEND.instr) == NAND
         || opcode(state.WBEND.instr) == CMOV)
      {
        state.IDEX.readRegB = state.WBEND.writeData;
      }
    }
    if(field0(state.IDEX.instr) == field1(state.WBEND.instr))
    {
      if(opcode(state.WBEND.instr) == LW)
      {
        state.IDEX.readRegA = state.WBEND.writeData;
      }
    }
    if(field1(state.IDEX.instr) == field1(state.WBEND.instr))
    {
      if(opcode(state.WBEND.instr) == LW)
      {
        state.IDEX.readRegB = state.WBEND.writeData;
      }
    }


    //finished pushing data from IDEX to EXMEM
    newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + state.IDEX.offset;
    newState.EXMEM.readRegB = state.IDEX.readRegB;


    //This block handles ALU stuff
    if(opcode(state.IDEX.instr) == ADD)
      newState.EXMEM.aluResult = state.IDEX.readRegA + state.IDEX.readRegB;
    else if(opcode(state.IDEX.instr) == NAND)
      newState.EXMEM.aluResult = ~(state.IDEX.readRegA & state.IDEX.readRegB);
    else if(opcode(state.IDEX.instr) == BEQ)
      newState.EXMEM.aluResult = state.IDEX.readRegA - state.IDEX.readRegB;
    else if(opcode(state.IDEX.instr) == CMOV)
    {
      if(state.IDEX.readRegB == state.IDEX.readRegA)
        newState.EXMEM.aluResult = state.IDEX.readRegA;
      else
        newState.EXMEM.instr = NOOPINSTRUCTION;
    }
    else if(opcode(state.IDEX.instr) == LW || opcode(state.IDEX.instr) == SW)
      newState.EXMEM.aluResult = state.IDEX.readRegA + state.IDEX.offset;


    /* --------------------- MEM stage --------------------- */

    //pushes along the instruction from EXMEM to MEMWB
    newState.MEMWB.instr = state.EXMEM.instr;

    //this block handles memory related things, most importantly sw and lw
    if(opcode(state.EXMEM.instr) == ADD || opcode(state.EXMEM.instr) == NAND
       || opcode(state.EXMEM.instr) == CMOV)
      newState.MEMWB.writeData = state.EXMEM.aluResult;
    else if(opcode(state.EXMEM.instr) == LW)
      newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
    else if(opcode(state.EXMEM.instr) == SW)
      newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.readRegB;


    //CONTROL HAZARD HANDLER
    //This part handles what happens if it turns out we were supposed to take
    //a branch.
    //check aluresult to see if branch is taken
    if(state.EXMEM.aluResult == 0 && opcode(state.EXMEM.instr) == BEQ)
    {
      //If branch was taken, noop everything and send pc to branchTarget
      newState.IFID.instr = NOOPINSTRUCTION;
      newState.IDEX.instr = NOOPINSTRUCTION;
      newState.EXMEM.instr = NOOPINSTRUCTION;
      newState.pc = state.EXMEM.branchTarget;
    }

    /* --------------------- WB stage --------------------- */
    newState.WBEND.instr = state.MEMWB.instr;
    newState.WBEND.writeData = state.MEMWB.writeData;

    if(opcode(state.MEMWB.instr) == ADD || opcode(state.MEMWB.instr) == NAND
       || opcode(state.MEMWB.instr) == CMOV)
      newState.reg[field2(state.MEMWB.instr)] = state.MEMWB.writeData;
    else if(opcode(state.MEMWB.instr) == LW)
      newState.reg[field1(state.MEMWB.instr)] = state.MEMWB.writeData;

    /* ----------------------- END ------------------------ */


    state = newState; /* this is the last statement before end of the loop.
            It marks the end of the cycle and updates the
            current state with the values calculated in this
            cycle */
  }
  exit(0);
}


// function to convert decimal to binary
void toBinary(string& binary)
{
  //converts the string decimal to an int
  int decimal = stoi(binary);

  //converts the int decimal into a bitset
  bitset<16> bitsetBinary (decimal);

  //converts the bitset back to a string, but now in 2's complement
  binary = bitsetBinary.to_string();

  return;
}


void fileRead(vector<AssemblyLine> &bitArray, int &PC, string argv)
{
  ifstream in;
  in.open(argv);

  //makes sure the input file opens
  if (!in)
  {
    cerr << "\nError: Bad input file or filename" << endl;
    exit(1);
  }

  // Reads the file
  while (!in.eof()) {
      //makes empty spot to put new stuff in at back of vector
      bitArray.push_back(AssemblyLine());

      //begins reading
      string input;
      in >> input;

      //checks for add, nand, lw,, sw, beq, and cmov, since they are all the
      //same for input
      if (input == "add" || input == "nand" || input == "lw" ||
          input == "sw" || input == "beq" || input == "cmov")
      {
        bitArray[PC].instruction = input;
        in >> bitArray[PC].field0 >> bitArray[PC].field1 >> bitArray[PC].field2;
        getline(in, bitArray[PC].comment, '\n');
        bitArray[PC].PC = PC;
      }

      //checks for .fill instruction
      else if (input == ".fill")
      {
          bitArray[PC].instruction = input;
          in >> bitArray[PC].field0;
          getline(in, bitArray[PC].comment, '\n');
      }

      //checks for halt and noop
      else if (input == "halt" || input == "noop")
      {
        bitArray[PC].instruction = input;
        bitArray[PC].PC = PC;
      }

      //catches labels
      else
      {
        bitArray[PC].label = input;

        if(bitArray[PC].label.size() > 6 || isdigit(bitArray[PC].label[0]))
        {
          cerr << "Bad label on line " << PC << endl;
          exit(1);
        }

        //checks if operation is a .fill
        in >> input;
        if (input == ".fill")
        {
          bitArray[PC].instruction = input;
          in >> bitArray[PC].field0;
        }

        //checks if operation is halt
        else if(input == "halt")
        {
          bitArray[PC].instruction = input;
          bitArray[PC].PC = PC;
        }

        //any other instruction goes here
        else
        {
            bitArray[PC].instruction = input;
            in  >> bitArray[PC].field0 >> bitArray[PC].field1
                >> bitArray[PC].field2;
        }

        getline(in, bitArray[PC].comment, '\n');
        bitArray[PC].PC = PC;
      }

      //this section remembers the original line of mips code
      if(!bitArray[PC].label.empty())
      {
        bitArray[PC].mips = bitArray[PC].label +" "+
                            bitArray[PC].instruction +" "+ bitArray[PC].field0
                            +" "+ bitArray[PC].field1 +" "+ bitArray[PC].field2;
      }
      else
      {
        bitArray[PC].mips = bitArray[PC].instruction +" "+ bitArray[PC].field0
                            +" "+ bitArray[PC].field1 +" "+ bitArray[PC].field2;
      }


      //increments the PC
      PC++;
  }
}

//translates the instruction to binary
void instructionToBinary(string &instruction)
{
  if(instruction == "add")
    instruction = "000";
  else if(instruction == "nand")
    instruction = "001";
  else if(instruction == "lw")
    instruction = "010";
  else if(instruction == "sw")
    instruction = "011";
  else if(instruction == "beq")
    instruction = "100";
  else if(instruction == "cmov")
    instruction = "101";
  else if(instruction == "halt")
    instruction = "110";
  else if(instruction == "noop")
    instruction = "111";

  return;
}

//translates the field to binary
void fieldToBinary(string &field)
{
  //converts the string decimal to an int
  int decimal = stoi(field);

  //converts the int decimal into a bitset
  bitset<3> bitsetBinary (decimal);

  //converts the bitset back to a string, but now in 2's complement
  field = bitsetBinary.to_string();

  return;
}

//converts the given label to an address
void labelToAddress(string &label, vector<AssemblyLine> &bitArray, const int i)
{
  //checks for the label in the bitArray
  int j = 0;
  while (j < bitArray.size())
  {
    //if found, sets the label to be the PC stored for the label
    if(label == bitArray[j].label)
    {
      label = to_string(bitArray[j].PC);
      break;
    }

    j++;
  }

  //checks to make sure the label was appropriately decoded
  if(isalpha(label[0]))
  {
    cerr << "\nError: Unknown label on line " << i << endl;
    exit(1);
  }

  return;
}

//if a label is found, converts the label to the address
void fieldLabelHandler(vector<AssemblyLine> &bitArray)
{
  //cycles through the bitArray to find labels
  for(int i = 0; i < bitArray.size(); i++)
  {
    //if label is found, checks for match, then applies appropriate address
    //Checks all instances of field0
    if(isalpha(bitArray[i].field0[0]))
    {
      labelToAddress(bitArray[i].field0, bitArray, i);
      bitArray[i].beqlabel = true;
    }

    //Checks all instances of field1
    if(isalpha(bitArray[i].field1[0]))
    {
      labelToAddress(bitArray[i].field1, bitArray, i);
      bitArray[i].beqlabel = true;
    }

    //Checks all instances of field2
    if(isalpha(bitArray[i].field2[0]))
    {
      labelToAddress(bitArray[i].field2, bitArray, i);
      bitArray[i].beqlabel = true;
    }
  }

  return;
}

void theConcatenator(vector<AssemblyLine> &bitArray)
{
  for(int i = 0; i < bitArray.size(); i++)
  {
    //puts in the unused bits and the opcode
    bitArray[i].binary = "0000000" + bitArray[i].instruction;

    //handles R-type instructions
    if(bitArray[i].instruction == "000" || bitArray[i].instruction == "001" ||
       bitArray[i].instruction == "101")
    {
      bitArray[i].binary += bitArray[i].field0;
      bitArray[i].binary += bitArray[i].field1;
      bitArray[i].binary += "0000000000000";
      bitArray[i].binary += bitArray[i].field2;
    }

    //handles I-type instructions
    else if(bitArray[i].instruction == "010" || bitArray[i].instruction == "011"||
            bitArray[i].instruction == "100")
    {
      bitArray[i].binary += bitArray[i].field0;
      bitArray[i].binary += bitArray[i].field1;
      bitArray[i].binary += bitArray[i].field2;
    }

    //handles O-type instructions
    else if (bitArray[i].instruction == "110" || bitArray[i].instruction == "111")
    {
      bitArray[i].binary += "0000000000000000000000";
    }
  }
}



void printState(stateType *statePtr)
{
  int i;
  printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
  printf("\tpc %d\n", statePtr->pc);

  printf("\tdata memory:\n");
	for (i=0; i<statePtr->numMemory; i++) {
	    printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
	}
    printf("\tregisters:\n");
	for (i=0; i<NUMREGS; i++) {
	    printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
	}
    printf("\tIFID:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IFID.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
    printf("\tIDEX:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IDEX.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
	printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
	printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
	printf("\t\toffset %d\n", statePtr->IDEX.offset);
    printf("\tEXMEM:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->EXMEM.instr);
	printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
	printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
	printf("\t\treadRegB %d\n", statePtr->EXMEM.readRegB);
    printf("\tMEMWB:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->MEMWB.instr);
	printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
    printf("\tWBEND:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->WBEND.instr);
	printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
}

int field0(int instruction)
{
    return( (instruction>>19) & 0x7);
}

int field1(int instruction)
{
    return( (instruction>>16) & 0x7);
}

int field2(int instruction)
{
    return(instruction & 0xFFFF);
}

int opcode(int instruction)
{
    return(instruction>>22);
}

void printInstruction(int instr)
{
    char opcodeString[10];
    if (opcode(instr) == ADD) {
	strcpy(opcodeString, "add");
    } else if (opcode(instr) == NAND) {
	strcpy(opcodeString, "nand");
    } else if (opcode(instr) == LW) {
	strcpy(opcodeString, "lw");
    } else if (opcode(instr) == SW) {
	strcpy(opcodeString, "sw");
    } else if (opcode(instr) == BEQ) {
	strcpy(opcodeString, "beq");
    } else if (opcode(instr) == CMOV) {
	strcpy(opcodeString, "cmov");
    } else if (opcode(instr) == HALT) {
	strcpy(opcodeString, "halt");
    } else if (opcode(instr) == NOOP) {
	strcpy(opcodeString, "noop");
    } else {
	strcpy(opcodeString, "data");
    }

    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
	field2(instr));
}

int signExtend(int value)
{
  if (value & (1<<15) )
    value -= (1<<16);

  return(value);
}
