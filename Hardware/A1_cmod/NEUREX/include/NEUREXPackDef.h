#ifndef NEUREXPackDef_H
#define NEUREXPackDef_H

#include <boost/preprocessor/list/for_each.hpp>
#include <ac_std_float.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <ac_float.h>
#include <ac_fixed.h>
#include <ac_int.h>
#include <nvhls_marshaller.h>
#include <nvhls_int.h>
#include <nvhls_types.h>
//#define USE_FLOAT

// Helpers from https://github.com/hlslibs/matchlib_toolkit/blob/main/include/auto_gen_fields.h#L425
// Slightly modify for marshall, width only
#include "auto_gen_fields.h"

#define NPU_SIZE 32

typedef ac_int<16, true> NPU_W_Elem_Type;
typedef ac_int<16, true> NPU_In_Elem_Type;
typedef ac_int<16, true> NPU_Out_Elem_Type;
// typedef ac_std_float<32, 8> NPU_W_Elem_Type;
// typedef ac_std_float<32, 8> NPU_In_Elem_Type;
// typedef ac_std_float<32, 8> NPU_Out_Elem_Type;

class NPU_W_Type : public nvhls_message {
public:
    NPU_W_Elem_Type X[NPU_SIZE];
    AUTO_GEN_FIELD_METHODS((X))
};

class NPU_In_Type : public nvhls_message {
public:
    NPU_In_Elem_Type X[NPU_SIZE];
    AUTO_GEN_FIELD_METHODS((X))
};

class NPU_Out_Type : public nvhls_message {
public:
    NPU_Out_Elem_Type X[NPU_SIZE];
    AUTO_GEN_FIELD_METHODS((X))
};

// Index compute unit
typedef ac_std_float<32, 8> ICU_In_Elem;
class ICU_In_Type : public nvhls_message {
public:
    ICU_In_Elem x[8];
    AUTO_GEN_FIELD_METHODS((x))
};
typedef ac_std_float<32, 8> ICU_Out_Type;

// Index generation unit
typedef ac_std_float<32, 8> IGU_In_Elem_Type;
class IGU_In_Type : public nvhls_message {
public:
    IGU_In_Elem_Type x[3]; // Pos
    AUTO_GEN_FIELD_METHODS((x))
};
typedef ac_std_float<32, 8> IGU_Out_Elem_Type;
typedef int IGU_Grid_Res;
class Hashed_addr : public nvhls_message {
public:
    IGU_Grid_Res x[8];
    AUTO_GEN_FIELD_METHODS((x))
};
class IGU_Weight : public nvhls_message {
public:
    IGU_Out_Elem_Type x[8];
    AUTO_GEN_FIELD_METHODS((x))
};
class IGU_Pos : public nvhls_message {
public:
    int x[3];
    AUTO_GEN_FIELD_METHODS((x))
};
static int P1 = 20;
static int P2 = 30;



#endif //NEUREXPackDef_H
