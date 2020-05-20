/*
  Written by Adam Fallon 2020

  Simple VM for running ASM.
  128kb Memory
  10 Registers (8 General Purpose, 1 Program Counter, 1 Condition Flag)
  16 OpCodes 

  Example Assembly Program

  .ORIG x3000                            ; address in memory where the program will be loaded
  LEA R0, HELLO_STR                      ; load the address of HELLO_STR into R0
  PUTs                                   ; output the string in register R0 to the console
  HALT                                   ; halt the program
  HELLO_STR .STRINGZ "Hello, world!"     ; store the string here in the program 
  .END                                   ; mark the end of the file

  Example with a loop
  AND R0, R0, 0        ; Clear R0
  LOOP                 ; Label
  ADD R0, R0, 1        ; add 1 to R0 and store back in R0
  ADD R1, R0, -10      ; subtract 10 from R0 and store back in R1
  BRn LOOP             ; Go back to LOOP if the result was negative
  ...; R0 is now 10!

*/
#include <stdio.h>
#include <stdint.h>

// 2^16 Memory Locations - Each with store 16bit Value - 128Kb Memory
uint16_t memory[UINT16_MAX];

// Registers
// 10 Total Registers - Each holding 16 bits
// 8 General purpose - R0-R7
// 1 Program counter - PC
// 1 Condition flag - COND

enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
};

uint16_t reg[R_COUNT];

// Opcodes
// 16 Opcodes
// Each instruction is 16 bits, Left 4 bits for opcode - the rest for the params
enum
{
    OP_BR = 0,  /* branch */
    OP_ADD,     /* add */
    OP_LD,      /* load */
    OP_ST,      /* store */
    OP_JSR,     /* jump register */
    OP_AND,     /* bitwise and */
    OP_LDR,     /* load register */
    OP_STR,     /* store register */
    OP_RTI,     /* unused */
    OP_NOT,     /* bitwise not */
    OP_LDI,     /* load indirect */
    OP_STI,     /* store indirect */
    OP_JMP,     /* jump */
    OP_RES,     /* reserved (unused) */
    OP_LEA,     /* load effective address */
    OP_TRAP     /* exectute trap */
};

// Condition flags
enum {
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2  /* N */
};

uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }

    return x;
}


void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if(reg[r] >> 15) // a 1 in the left-most bit indicates negative
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}


uint8_t read_image(const char * c) { return -1; }
unsigned short mem_read(uint16_t memory) { return 'c'; }

// Function Impls

// Add
/*

  ADD takes two values and stores them in a register.
  In register mode, the second value to add is found in a register.
  In immediate mode, the second value is embedded in the right-most 5 bits of the instruction.
  Values which are shorter than 16 bits need to be sign extended.
  Any time an instruction modifies a register, the condition flags need to be updated.

*/
void add(uint16_t instr)
{
    // Destination Register
    uint16_t r0 = (instr >> 9) & 0x7;

    // First operand (SR1)
    uint16_t r1 = (instr >> 6) & 0x7;

    // Immediate or register mode?
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if(imm_flag)
    {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }

    update_flags(r0);
}

void and(uint16_t instr) {}
void not(uint16_t instr) {}
void br(uint16_t instr) {}
void jmp(uint16_t instr) {}
void jsr(uint16_t instr) {}
void ld(uint16_t instr) {}

void ldi(uint16_t instr) {
    // Destination Register
    uint16_t r0 = (instr >> 9) & 0x07;

    // PC Offset
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

    // Add pc offset to current PC, look at that memory location to get the fial address.
    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
    update_flags(r0);
}

void ldr(uint16_t instr) {}
void lea(uint16_t instr) {}
void st(uint16_t instr) {}
void sti(uint16_t instr) {}
void str(uint16_t instr) {}
void trap(uint16_t instr) {}

void bad(uint16_t opcode) {
    printf("Unknown opcode!, %d\n", opcode);
}

int main(int argc, const char* argv[]) {
    // Load args
    if (argc < 2)
    {
        printf("lc3 [image-file1] ...\n");
    }

    for(int j = 1; j < argc; ++j)
    {
        if(!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
        }
    }
    // Setup

    /* Set PC to start position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while(running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch(op)
        {
            case OP_ADD:
                add(instr);
                break;
            case OP_AND:
                and(instr);
                break;
            case OP_NOT:
                not(instr);
                break;
            case OP_BR:
                br(instr);
                break;
            case OP_JMP:
                jmp(instr);
                break;
            case OP_JSR:
                jsr(instr);
                break;
            case OP_LD:
                ld(instr);
                break;
            case OP_LDI:
                ldi(instr);
                break;
            case OP_LDR:
                ldr(instr);
                break;
            case OP_LEA:
                lea(instr);
                break;
            case OP_ST:
                st(instr);
                break;
            case OP_STI:
                str(instr);
                break;
            case OP_STR:
                str(instr);
                break;
            case OP_TRAP:
                trap(instr);
                break;
            case OP_RES:
            case OP_RTI:
            default:
                bad(instr);
                break;
	}
    }

    // Shutdown VM
    return 0;
}
