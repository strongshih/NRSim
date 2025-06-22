#ifndef ICARUSPackDef_H
#define ICARUSPackDef_H

#include <boost/preprocessor/list/for_each.hpp>
#include <ac_std_float.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <ac_float.h>
#include <ac_fixed.h>
#include <nvhls_marshaller.h>
#include <nvhls_int.h>
#include <nvhls_types.h>
// #define USE_FLOAT

// Helpers from https://github.com/hlslibs/matchlib_toolkit/blob/main/include/auto_gen_fields.h#L425
// Slightly modify for marshall, width only
#include "auto_gen_fields.h"

#ifdef USE_FLOAT
typedef ac_std_float<16, 8> TESTTYPE;
#else
//typedef ac_fixed<32, 16, true, AC_TRN, AC_SAT> TESTTYPE; // saturated to nearest max/min
typedef ac_fixed<9, 9, true, AC_TRN, AC_SAT> TESTTYPE; // saturated to nearest max/min
#endif

// MUL types
static int const MUL_PART     = 2;                    // number of selectors
static int const BIT_PER_PART = 4;                    // selector width
static int const INTEGER_WIDTH = 8;                   // fixed point integer width
static int const total_bit = MUL_PART*BIT_PER_PART+1; // +1 => sign bit
typedef ac_fixed<total_bit, INTEGER_WIDTH+1, true, AC_TRN, AC_SAT> MUL_In_Type;  // changeable
typedef ac_fixed<total_bit, INTEGER_WIDTH+1, true, AC_TRN, AC_SAT> MUL_Out_Type; // changeable
#define SCALE 128 // If use actual fixed-point (INTEGER_WIDTH < tota_bit-1), then it is 1 (since handled in fixed)
                // Otherwise, it is related to the range of data, here if INTEGER_WIDTH=8, then it's 256
                // Some combinations of (INTEGER_WIDTH, SCALE) = (2, 1), (8, 256), (8, 128)
typedef MUL_In_Type PCM_In_Type;
class PCM_Out_Type : public nvhls_message {
public:
    ac_int<3*BIT_PER_PART+1, true> X[4];
    bool is_zero;
    AUTO_GEN_FIELD_METHODS((X, is_zero))
};
typedef PCM_Out_Type SSA_In_Type;
typedef MUL_Out_Type SSA_Out_Type;


/*** PEU Constants ***/
static int const PEU_INPUT_DIM = 3;       // changeable
static int const PEU_CORDIC_IN_DIM = 32; // changeable, tb_PEU: 30, tb_ICARUS: 128

// Currently using float32 for correctness check
/*** PEU Types ***/
typedef TESTTYPE PEU_Matrix_A_Type;        // changeable
typedef TESTTYPE PEU_Position_Type;        // changeable
typedef TESTTYPE PEU_CORDIC_In_Elem_Type;  // changeable
typedef TESTTYPE PEU_CORDIC_Out_Elem_Type; // changeable

class PEU_In_Type : public nvhls_message {
public:
    PEU_Position_Type X[PEU_INPUT_DIM];
    bool isLastSample;
    AUTO_GEN_FIELD_METHODS((X, isLastSample))
};

class PEU_CORDIC_In_Type : public nvhls_message {
public:
    PEU_CORDIC_In_Elem_Type X[PEU_CORDIC_IN_DIM];
    bool isLastSample;
    AUTO_GEN_FIELD_METHODS((X, isLastSample))
};

class PEU_Out_Type : public nvhls_message {
public:
    PEU_CORDIC_Out_Elem_Type X[PEU_CORDIC_IN_DIM*2]; // *2 from sin, cos
    bool isLastSample;
    AUTO_GEN_FIELD_METHODS((X, isLastSample))
};


/*** MLP Constants ***/
static int const MLP0_IN_DIM = PEU_CORDIC_IN_DIM*2;
static int const MLP0_OUT_DIM = 256;                // changeable
static int const MLP1_IN_DIM = MLP0_OUT_DIM;
static int const MLP1_OUT_DIM = 4;
static int const MAX_SAMPLE_NUM = 256;
static int const BLOCK_SZ = 4;

/*** MLP Types ***/
typedef ac_int<nvhls::log2_ceil<MAX_SAMPLE_NUM>::val, false> sample_cnt; 
typedef TESTTYPE MLP1_In_Elem_Type; // changeable
typedef TESTTYPE MLP_In_Elem_Type;  // changeable
typedef TESTTYPE MLP_Out_Elem_Type; // changeable
typedef PEU_Matrix_A_Type MLP_Weight_Type;
typedef PEU_Out_Type MLP_In_Type;

class MLP1_In_Type : public nvhls_message {
public:
    MLP1_In_Elem_Type X[MLP1_IN_DIM];
    bool isLastSample;
    AUTO_GEN_FIELD_METHODS((X, isLastSample))
};

class MLP_Out_Type : public nvhls_message {
public:
    MLP_Out_Elem_Type X[MLP1_OUT_DIM];
    bool isLastSample;
    AUTO_GEN_FIELD_METHODS((X, isLastSample))
};


/*** VRU Constants ***/

/*** VRU Types ***/
typedef MLP_Out_Elem_Type VRU_C_Type;
typedef MLP_Out_Elem_Type VRU_Sigma_Type;
typedef TESTTYPE VRU_Delta_Type;
typedef TESTTYPE VRU_Color_Type;

class VRU_In_Type : public nvhls_message {
public:
    VRU_C_Type emitted_c[3];
    VRU_Sigma_Type sigma;
    VRU_Delta_Type delta;
    bool isLastSample;
    AUTO_GEN_FIELD_METHODS((emitted_c, sigma, delta, isLastSample))
};

class VRU_Out_Type : public nvhls_message {
public:
    VRU_Color_Type c[3];
    AUTO_GEN_FIELD_METHODS((c))
};


/*** ICARUS Constants ***/
static int const PEU_MLP_TO_DEPTH = 512;
static int const MEMREQ_DEPTH = 512;
static int const VRUOUT_DEPTH = 512;

/*** ICARUS Types ***/
// For instructions
enum inst_type {WEIGHT_INIT=0, READ_POS=1};      // modify this 
class ICARUS_Op_In_Type : public nvhls_message { // TODO: modify this
public:
    ac_int<3, false> mode;  // opmode
    ac_int<32, false> addr; // offset to read address 
    uint num;               // some counter
    bool wr_en;             // write or read
    AUTO_GEN_FIELD_METHODS((mode, addr, num, wr_en))
};



// For write request to Matrix A memory
class MemReq : public nvhls_message {
public:
    bool forPEU;
    bool forMLP0;
    bool isBias;
    uint16 index[2];
    PEU_Matrix_A_Type data;
    AUTO_GEN_FIELD_METHODS((forPEU, forMLP0, isBias, index, data))
};
class CORDICINPUT : public nvhls_message {
public:
    ac_std_float<16, 8> a;
    AUTO_GEN_FIELD_METHODS((a))
};



#endif //ICARUSPackDef_H
