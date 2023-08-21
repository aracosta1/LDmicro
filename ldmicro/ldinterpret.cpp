//-----------------------------------------------------------------------------
// A sample interpreter for the .int files generate by LDmicro. These files
// represent a ladder logic program for a simple 'virtual machine.' The
// interpreter must simulate the virtual machine and for proper timing the
// program must be run over and over, with the period specified when it was
// compiled (in Settings -> MCU Parameters).
//
// This method of running the ladder logic code would be useful if you wanted
// to embed a ladder logic interpreter inside another program. LDmicro has
// converted all variables into addresses, for speed of execution. However,
// the .int file includes the mapping between variable names (same names
// that the user specifies, that are visible on the ladder diagram) and
// addresses. You can use this to establish specially-named variables that
// define the interface between your ladder code and the rest of your program.
//
// In this example, I use this mechanism to print the value of the integer
// variable 'a' after every cycle, and to generate a square wave with period
// 2*Tcycle on the input 'Xosc'. That is only for demonstration purposes, of
// course.
//
// In a real application you would need some way to get the information in the
// .int file into your device; this would be very application-dependent. Then
// you would need something like the InterpretOneCycle() routine to actually
// run the code. You can redefine the program and data memory sizes to
// whatever you think is practical; there are no particular constraints.
//
// The disassembler is just for debugging, of course. Note the unintuitive
// names for the condition ops; the INT_IFs are backwards, and the INT_ELSE
// is actually an unconditional jump! This is because I reused the names
// from the intermediate code that LDmicro uses, in which the if/then/else
// constructs have not yet been resolved into (possibly conditional)
// absolute jumps. It makes a lot of sense to me, but probably not so much
// to you; oh well.
//
// Jonathan Westhues, Aug 2005
//-----------------------------------------------------------------------------
#include "stdafx.h"

#define INTCODE_H_CONSTANTS_ONLY
#include "intcode.h"

typedef unsigned char  BYTE; // 8-bit unsigned
typedef unsigned short WORD; // 16-bit unsigned

// Some arbitrary limits on the program and data size
#define MAX_OPS 256
#define MAX_VARIABLES 128
#define MAX_INTERNAL_RELAYS 128

// This data structure represents a single instruction for the 'virtual
// machine.' The .op field gives the opcode, and the other fields give
// arguments. I have defined all of these as 16-bit fields for generality,
// but if you want then you can crunch them down to 8-bit fields (and
// limit yourself to 256 of each type of variable, of course). If you
// crunch down .op then nothing bad happens at all. If you crunch down
// .literal then you only have 8-bit literals now (so you can't move
// 300 into 'var'). If you crunch down .name3 then that limits your code size,
// because that is the field used to encode the jump addresses.
//
// A more compact encoding is very possible if space is a problem for
// you. You will probably need some kind of translator regardless, though,
// to put it in whatever format you're going to pack in flash or whatever,
// and also to pick out the name <-> address mappings for those variables
// that you're going to use for your interface out. I will therefore leave
// that up to you.
typedef struct {
    int16_t op;
    int16_t name1;
    int16_t name2;
    int16_t name3;
    int32_t literal1;
} BinOp;


BinOp   Program[MAX_OPS];
int32_t Integers[MAX_VARIABLES];
BYTE    Bits[MAX_INTERNAL_RELAYS];

// This is the requested cycle time, the hardware will do it's best to run with this timing but NO guarantee is given.
int CycleTime = 10;

// this is implementation specific. Every target will use a custom association of IO register to real register
// in this example:
//      X0-3 are mapped to input pins
//      Y0-1 are mapped to relays
//      A0 is a temperature value
//      
typedef struct {
    int addr;   // address in the Bits table
} InputMap_t;
#define MAX_INPUT 16
InputMap_t InputMap[MAX_INPUT];

typedef struct {
    int addr; // address in the Bits table
} OutputMap_t;
#define MAX_OUTPUT 2
OutputMap_t OutputMap[MAX_OUTPUT];

typedef struct {
    int addr; // address in the Bits table
} AnalogMap_t;
#define MAX_ANALOG 1
AnalogMap_t AnalogMap[MAX_ANALOG];

    //-----------------------------------------------------------------------------
// What follows are just routines to load the program, which I represent as
// hex bytes, one instruction per line, into memory. You don't need to
// remember the length of the program because the last instruction is a
// special marker (INT_END_OF_PROGRAM).
//
void BadFormat()
{
    fprintf(stderr, "Bad program format.\n");
    exit(-1);
}
int HexDigit(int c)
{
    c = tolower(c);
    if(isdigit(c)) {
        return c - '0';
    } else if(c >= 'a' && c <= 'f') {
        return (c - 'a') + 10;
    } else {
        BadFormat();
    }
    return 0;
}
void LoadProgram(char *fileName)
{
    FILE *f = fopen(fileName, "r");
    char  line[80];

    // This is not suitable for untrusted input.

    if(!f) {
        fprintf(stderr, "couldn't open '%s'\n", fileName);
        exit(-1);
    }

    if(!fgets(line, sizeof(line), f))
        BadFormat();
    if(strcmp(line, "$$LDcode\n") != 0)
        BadFormat();

    for(int pc = 0;; pc++) {
        char *t, i;
        BYTE *b;

        if(!fgets(line, sizeof(line), f))
            BadFormat();
        if(strcmp(line, "$$bits\n") == 0)
            break;
        if(strcmp(line, "$$int16s\n") == 0)
            break;
        if(strcmp(line, "$$cycle\n") == 0)
            break;

        if(strlen(line) != sizeof(BinOp) * 2 + 1)
            BadFormat();

        t = line;
        b = (BYTE *)&Program[pc];

        for(i = 0; i < sizeof(BinOp); i++) {
            b[i] = HexDigit(t[1]) | (HexDigit(t[0]) << 4);
            t += 2;
        }
    }

    // end of LDcode, now parse variables and find X, Y, A
    char *p;
    int   reg;
    int   addr;
    while(fgets(line, sizeof(line), f)) {
        if (*line == 'X') {
            p = strchr(line, ',');
            if(p == NULL)
                BadFormat();
            *p = 0;
            reg = atoi(line + 1);
            addr = atoi(p + 1);
            if(reg < MAX_INPUT) {
                InputMap[reg].addr = addr;
            }
        }
        if(memcmp(line, "X", 1) == 0) {
        }
        if(memcmp(line, "Y", 1) == 0) {
        }
        if(memcmp(line, "A", 1) == 0) {
        }
        if(memcmp(line, "$$cycle", 7) == 0) {
            CycleTime = atoi(line + 7);
        }
    }

    fclose(f);
}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Disassemble the program and pretty-print it. This is just for debugging,
// and it is also the only documentation for what each op does. The bit
// variables (internal relays or whatever) live in a separate space from the
// integer variables; I refer to those as bits[addr] and int16s[addr]
// respectively.
//-----------------------------------------------------------------------------
void Disassemble()
{
    char bad_codes = 0;
    char c;
    for(int pc = 0;; pc++) {
        BinOp *p = &Program[pc];
        printf("%03x: ", pc);

        switch(Program[pc].op) {
            case INT_SET_BIT:
                printf("bits[%03x] := 1", p->name1);
                break;

            case INT_CLEAR_BIT:
                printf("bits[%03x] := 0", p->name1);
                break;

            case INT_COPY_BIT_TO_BIT:
                printf("bits[%03x] := bits[%03x]", p->name1, p->name2);
                break;

            case INT_SET_VARIABLE_TO_LITERAL:
                printf("int16s[%03x] := %d (0x%04x)", p->name1, p->literal1, p->literal1);
                break;

            case INT_SET_VARIABLE_TO_VARIABLE:
                printf("int16s[%03x] := int16s[%03x]", p->name1, p->name2);
                break;

            case INT_DECREMENT_VARIABLE:
                printf("(int16s[%03x])--", p->name1);
                break;

            case INT_INCREMENT_VARIABLE:
                printf("(int16s[%03x])++", p->name1);
                break;

            case INT_SET_VARIABLE_ADD:
                c = '+';
                goto arith;
            case INT_SET_VARIABLE_SUBTRACT:
                c = '-';
                goto arith;
            case INT_SET_VARIABLE_MULTIPLY:
                c = '*';
                goto arith;
            case INT_SET_VARIABLE_DIVIDE:
                c = '/';
                goto arith;
            case INT_SET_VARIABLE_MOD:
                c = '%';
                goto arith;
            arith:
                printf("int16s[%03x] := int16s[%03x] %c int16s[%03x]", p->name1, p->name2, c, p->name3);
                break;

            case INT_IF_BIT_SET:
                printf("unless (bits[%03x] set)", p->name1);
                goto cond;
            case INT_IF_BIT_CLEAR:
                printf("unless (bits[%03x] clear)", p->name1);
                goto cond;
            case INT_IF_VARIABLE_LES_LITERAL:
                printf("unless (int16s[%03x] < %d)", p->name1, p->literal1);
                goto cond;
            case INT_IF_VARIABLE_EQUALS_VARIABLE:
                printf("unless (int16s[%03x] == int16s[%03x])", p->name1, p->name2);
                goto cond;

            case INT_IF_GEQ:
                printf("unless (int16s[%03x] >= int16s[%03x])", p->name1, p->name2);
                goto cond;
            case INT_IF_LEQ:
                printf("unless (int16s[%03x] <= int16s[%03x])", p->name1, p->name2);
                goto cond;

            case INT_IF_NEQ:
                printf("unless (int16s[%03x] != int16s[%03x])", p->name1, p->name2);
                goto cond;
            case INT_IF_VARIABLE_GRT_VARIABLE:
                printf("unless (int16s[%03x] > int16s[%03x])", p->name1, p->name2);
                goto cond;
            cond:
                printf(" jump %03x+1", p->name3);
                break;

            case INT_ELSE:
                printf("jump %03x+1", p->name3);
                break;

            case INT_END_OF_PROGRAM:
                printf("<end of program>\n");
                if(bad_codes > 0)
                    BadFormat();
                return;

            case INT_AllocFwdAddr:
                printf("INT_AllocFwdAddr %03d", p->name1);
                break;

            case INT_AllocKnownAddr:
                printf("INT_AllocKnownAddr %03d", p->name1);
                break;

            case INT_FwdAddrIsNow:
                printf("INT_FwdAddrIsNow %03d", p->name1);
                break;

            default:
                printf("Unsupported op (Peripheral) for interpretable target. INT_%d", Program[pc].op);
                bad_codes++;
                break;
        }
        printf("\n");
    }

    printf("\n");
}

//-----------------------------------------------------------------------------
// This is the actual interpreter. It runs the program, and needs no state
// other than that kept in Bits[] and Integers[]. If you specified a cycle
// time of 10 ms when you compiled the program, then you would have to
// call this function 100 times per second for the timing to be correct.
//
// The execution time of this function depends mostly on the length of the
// program. It will be a little bit data-dependent but not very.
//-----------------------------------------------------------------------------
void InterpretOneCycle()
{
    for(int pc = 0;; pc++) {
        BinOp *p = &Program[pc];

        switch(Program[pc].op) {
            case INT_SET_BIT:
                Bits[p->name1] = 1;
                break;

            case INT_CLEAR_BIT:
                Bits[p->name1] = 0;
                break;

            case INT_COPY_BIT_TO_BIT:
                Bits[p->name1] = Bits[p->name2];
                break;

            case INT_SET_VARIABLE_TO_LITERAL:
                Integers[p->name1] = p->literal1;
                break;

            case INT_SET_VARIABLE_TO_VARIABLE:
                Integers[p->name1] = Integers[p->name2];
                break;

            case INT_DECREMENT_VARIABLE:
                (Integers[p->name1])--;
                break;

            case INT_INCREMENT_VARIABLE:
                (Integers[p->name1])++;
                break;

            case INT_SET_VARIABLE_ADD:
                Integers[p->name1] = Integers[p->name2] + Integers[p->name3];
                break;

            case INT_SET_VARIABLE_SUBTRACT:
                Integers[p->name1] = Integers[p->name2] - Integers[p->name3];
                break;

            case INT_SET_VARIABLE_MULTIPLY:
                Integers[p->name1] = Integers[p->name2] * Integers[p->name3];
                break;

            case INT_SET_VARIABLE_DIVIDE:
                if(Integers[p->name3] != 0)
                    Integers[p->name1] = Integers[p->name2] / Integers[p->name3];
                break;

            case INT_SET_VARIABLE_MOD:
                if(Integers[p->name3] != 0)
                    Integers[p->name1] = Integers[p->name2] % Integers[p->name3];
                break;

            case INT_IF_BIT_SET:
                if(!Bits[p->name1])
                    pc = p->name3;
                break;

            case INT_IF_BIT_CLEAR:
                if(Bits[p->name1])
                    pc = p->name3;
                break;

            case INT_IF_VARIABLE_LES_LITERAL:
                if(!(Integers[p->name1] < p->literal1))
                    pc = p->name3;
                break;

            case INT_IF_GEQ:
                if(!(Integers[p->name1] >= p->literal1))
                    pc = p->name3;
                break;

            case INT_IF_LEQ:
                if(!(Integers[p->name1] <= p->literal1))
                    pc = p->name3;
                break;

            case INT_IF_NEQ:
                if(!(Integers[p->name1] != Integers[p->name2]))
                    pc = p->name3;

            case INT_IF_VARIABLE_EQUALS_VARIABLE:
                if(!(Integers[p->name1] == Integers[p->name2]))
                    pc = p->name3;
                break;

            case INT_IF_VARIABLE_GRT_VARIABLE:
                if(!(Integers[p->name1] > Integers[p->name2]))
                    pc = p->name3;
                break;

            case INT_ELSE:
                pc = p->name3;
                break;

            case INT_END_OF_PROGRAM:
                return;

            case INT_AllocFwdAddr:
            case INT_AllocKnownAddr:
            case INT_FwdAddrIsNow:
                //ignore this vitual opcodes
                break;

            default:
                printf("Unsupported op (Peripheral) for interpretable target. INT_%d", Program[pc].op);
                break;
        }
    }
}

void ReadInputs(void)
{

}

void WriteOutputs(void)
{
}


int main(int argc, char **argv)
{
    if(argc != 2) {
//        fprintf(stderr, "usage: %s xxx.int\n", argv[0]);
//        return -1;
        LoadProgram("coil_s_r_n.int");
    } else {
        LoadProgram(argv[1]);
    }

    memset(Integers, 0, sizeof(Integers));
    memset(Bits, 0, sizeof(Bits));

    Disassemble();
    // 1000 cycles times 10 ms gives 10 seconds execution
    for(int i = 0; i < 10; i++) {
        // function to read the phisical inputs and update the variables
        ReadInputs();

        InterpretOneCycle();

        // function to write the outputs with values from the variables
        WriteOutputs();

        // Example for reaching in and reading a variable: just print it.
        //printf("a = %d              \r", Integers[SpecialAddrForA]);

        // XXX, nonportable; replace with whatever timing functions are
        // available on your target.
        Sleep(CycleTime/1000);
    }

    return 0;
}
