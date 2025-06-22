#ifndef CICEROPackDef_H
#define CICEROPackDef_H

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

#define NPU_SIZE 24


// typedef ac_std_float<16, 8> NPU_W_Elem_Type;
// typedef ac_std_float<16, 8> NPU_In_Elem_Type;
// typedef ac_std_float<16, 8> NPU_Out_Elem_Type;
typedef ac_int<16, true> NPU_W_Elem_Type;
typedef ac_int<16, true> NPU_In_Elem_Type;
typedef ac_int<16, true> NPU_Out_Elem_Type;
typedef ac_int<nvhls::log2_ceil<NPU_SIZE>::val+1, true> NPU_Index_Type;  // Generic index type for both row and column

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

// Address Generation
typedef ac_int<32, false> AG_VID;
typedef ac_int<16, false> AG_W;
class AG_VIDsWs : public nvhls_message { // 48B
public:
    AG_VID v[8];
    AG_W   w[8];
    AUTO_GEN_FIELD_METHODS((v, w))
};

// Reducer
typedef ac_int<16, false> reducer_W;
typedef ac_int<16, false> reducer_feature;

#endif //CICEROPackDef_H
