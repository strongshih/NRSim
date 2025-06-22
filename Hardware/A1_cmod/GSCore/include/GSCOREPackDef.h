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
#define USE_FLOAT

// Helpers from https://github.com/hlslibs/matchlib_toolkit/blob/main/include/auto_gen_fields.h#L425
// Slightly modify for marshall, width only
#include "auto_gen_fields.h"

/*** BSU Constants ***/
#define SORT_NUM 16
/*** BSU Types ***/
//                  16-bit, 8-bit precision
typedef ac_std_float<16, 5> BSU_DATA_TYPE;
typedef ac_int<nvhls::log2_ceil<SORT_NUM>::val, false> log_bsu_num;

class BSU_IN_OUT_TYPE : public nvhls_message {
public:
    BSU_DATA_TYPE x[SORT_NUM];
    AUTO_GEN_FIELD_METHODS((x))
};

/*** QSU Constants ***/
#define NUM_PIVOTS 7  // Number of pivot values
#define NUM_SUBSETS 8 // Number of subsets (NUM_PIVOTS + 1)

/*** QSU Types ***/
typedef ac_std_float<16, 5> FP16_TYPE;     // 16-bit floating point for Depth
typedef ac_int<16, false> UINT16_TYPE;     // 16-bit unsigned int for GID
typedef ac_int<8, false> SUBSET_INDEX_TYPE; // 8-bit unsigned int for subset index
// Input type for QSU: key-value pair (Depth, GID)
class QSU_IN_TYPE : public nvhls_message {
public:
    FP16_TYPE depth; // The key (Depth)
    UINT16_TYPE gid; // The value (GID)
    
    AUTO_GEN_FIELD_METHODS((depth,gid))
};

// Output type for QSU: GID and subset index
class QSU_OUT_TYPE : public nvhls_message {
public:
    UINT16_TYPE gid;               // The value (GID)
    SUBSET_INDEX_TYPE subset;      // The subset index
    
    AUTO_GEN_FIELD_METHODS((gid,subset))
};

/*** VRU Types ***/
#define NUM_ROTATE 4
typedef ac_int<nvhls::log2_ceil<NUM_ROTATE>::val, false> ROTATE_INDEX_TYPE; // 2-bit unsigned int for rotate index
// RGB color type
class RGB_TYPE : public nvhls_message{
public:
    FP16_TYPE r;
    FP16_TYPE g;
    FP16_TYPE b;
    
    AUTO_GEN_FIELD_METHODS((r,g,b))
};

// Input type for VRU
class VRU_IN_TYPE : public nvhls_message {
public:
    // Pixel position
    FP16_TYPE pixel_pos_x;
    FP16_TYPE pixel_pos_y;
    
    // Gaussian 2D mean (projected from 3D)
    FP16_TYPE mean_x;
    FP16_TYPE mean_y;
    
    // Gaussian 2D covariance matrix (projected from 3D)
    FP16_TYPE conx;
    FP16_TYPE cony;
    FP16_TYPE conz;
    
    // Gaussian color and opacity
    RGB_TYPE color;
    FP16_TYPE opacity;
    
    // Bitmap for subtile skipping
    // UINT8_TYPE bitmap;
    // UINT8_TYPE subtile_idx;
    
    // Flag to indicate last Gaussian for this pixel
    bool last_gaussian;

    ROTATE_INDEX_TYPE rotate_idx;

    AUTO_GEN_FIELD_METHODS((pixel_pos_x,pixel_pos_y,mean_x,mean_y,
                           conx,cony,conz,
                           color,opacity,/*(bitmap)(subtile_idx)*/last_gaussian,rotate_idx))
};

// Output type for VRU
class VRU_OUT_TYPE : public nvhls_message {
public:
    RGB_TYPE color;
    
    AUTO_GEN_FIELD_METHODS((color))
};


#endif //ICARUSPackDef_H
