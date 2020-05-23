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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
/* unix */
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

// 2^16 Memory Locations - Each with store 16bit Value - 128Kb Memory
uint16_t memory[UINT16_MAX];

// Is the program running?
int running = 0;

// For unix terminals
struct termios original_tio;

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

// Trap Codes
enum
{
    TRAP_GETC = 0x20,    /* get char from keyboard*/
    TRAP_OUT = 0x21,     /* output a char */
    TRAP_PUTS = 0x22,    /* output a word string */
    TRAP_IN = 0x23,      /* get character from keyboard, echo to terminal */
    TRAP_PUTSP = 0x24,   /* output a byte string */
    TRAP_HALT = 0x25     /* halt the program */
};

// Condition flags
enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2  /* N */
};

// Memory mapped registers
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */ 
    MR_KBDR = 0xFE00  /* keyboard data */ 
};

void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

uint16_t mem_read(uint16_t address)
{
    if(address == MR_KBSR)
    {
        if(check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }

    return memory[address];
}

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

// Function Implementations
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

void and(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
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

void not(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    reg[r0] = ~reg[r1];
    update_flags(r0);
}

void br(uint16_t instr) {
    uint16_t pc_offset = sign_extend(instr * 0x1FF, 9);
    uint16_t cond_flag = (instr >> 9) & 0x7;

    if(cond_flag & reg[R_COND])
    {
        reg[R_PC] += pc_offset;
    }
}

void jmp(uint16_t instr) {
    uint16_t r1 = (instr >> 6) & 0x7;
    reg[R_PC] = reg[r1];
}

void jsr(uint16_t instr) {
    uint16_t long_flag = (instr >> 11) & 1;
    reg[R_R7] = reg[R_PC];

    if(long_flag)
    {
        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
        reg[R_PC] += long_pc_offset; /* JSR */
    }
    else
    {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1];
    }
}

void ld(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read(reg[R_PC] + pc_offset);
    update_flags(r0);
}
void ldi(uint16_t instr) {
    // Destination Register
    uint16_t r0 = (instr >> 9) & 0x7;

    // PC Offset
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

    // Add pc offset to current PC, look at that memory location to get the final address.
    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
    update_flags(r0);
}

void ldr(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    reg[r0] = mem_read(reg[r1] + offset);
    update_flags(r0);
}

void lea(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = reg[R_PC] + pc_offset;
    update_flags(r0);
}

void st(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
}

void sti(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
}

void str(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x3F, 6);
    mem_write(reg[r1] + pc_offset, reg[r0]);
}

void bad(uint16_t opcode) {
    printf("Unknown opcode!, %d\n", opcode);
}

// Trap Routines
void trap_getc() {
    reg[R_R0] = (uint16_t)getchar();
}

void trap_out() {
    putc((char)reg[R_R0], stdout);
    fflush(stdout);
}

void trap_in() {
    printf("Enter a character: ");
    char c = getchar();
    putc(c, stdout);
    reg[R_R0] = (uint16_t)c;
}

void trap_putsp() {
    uint16_t* c = memory + reg[R_R0];
    while(*c)
    {
        char char1 = (*c) & 0xFF;
        putc(char1, stdout);
        char char2 = (*c) >> 8;
        if(char2) putc(char2, stdout);
        ++c;
    }

    fflush(stdout);
}

void trap_halt() {
    puts("HALT");
    fflush(stdout);
    running = 0;
}

void trap_puts() {
    uint16_t* c = memory + reg[R_R0];
    while(* c)
    {
        putc((char)*c, stdout);
        ++c;
    }
    fflush(stdout);
}

void trap(uint16_t instr) {
    switch(instr & 0xFF)
    {
        case TRAP_GETC:
            trap_getc();
            break;
        case TRAP_OUT:
            trap_out();
            break;
        case TRAP_PUTS:
            trap_puts();
            break;
        case TRAP_IN:
            trap_in();
            break;
        case TRAP_PUTSP:
            trap_putsp();
            break;
        case TRAP_HALT:
            trap_halt();
            break;
    }
}


uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void read_image_file(FILE* file)
{
    // Origin tells us where in memory to place the image
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    uint16_t max_read = UINT16_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    while(read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char * image_path)
{
    FILE* file = fopen(image_path, "rb");
    if(!file) { return 0; }
    read_image_file(file);
    fclose(file);
    return 1;
}


void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}


void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
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

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    /* Set PC to start position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    running = 1;
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
                abort();
                break;
        }

    }
    // Shutdown VM
    restore_input_buffering();
    return 0;
}
