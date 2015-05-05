#ifndef DM_EASING
#define DM_EASING

#include "vmath.h"

namespace dmEasing
{
    /**
     * Easing curve types
     */
    enum Type
    {
        TYPE_LINEAR = 0,       //!< TYPE_LINEAR
        TYPE_INQUAD = 1,       //!< TYPE_INQUAD
        TYPE_OUTQUAD = 2,      //!< TYPE_OUTQUAD
        TYPE_INOUTQUAD = 3,    //!< TYPE_INOUTQUAD
        TYPE_OUTINQUAD = 4,    //!< TYPE_OUTINQUAD
        TYPE_INCUBIC = 5,      //!< TYPE_INCUBIC
        TYPE_OUTCUBIC = 6,     //!< TYPE_OUTCUBIC
        TYPE_INOUTCUBIC = 7,   //!< TYPE_INOUTCUBIC
        TYPE_OUTINCUBIC = 8,   //!< TYPE_OUTINCUBIC
        TYPE_INQUART = 9,      //!< TYPE_INQUART
        TYPE_OUTQUART = 10,    //!< TYPE_OUTQUART
        TYPE_INOUTQUART = 11,  //!< TYPE_INOUTQUART
        TYPE_OUTINQUART = 12,  //!< TYPE_OUTINQUART
        TYPE_INQUINT = 13,     //!< TYPE_INQUINT
        TYPE_OUTQUINT = 14,    //!< TYPE_OUTQUINT
        TYPE_INOUTQUINT = 15,  //!< TYPE_INOUTQUINT
        TYPE_OUTINQUINT = 16,  //!< TYPE_OUTINQUINT
        TYPE_INSINE = 17,      //!< TYPE_INSINE
        TYPE_OUTSINE = 18,     //!< TYPE_OUTSINE
        TYPE_INOUTSINE = 19,   //!< TYPE_INOUTSINE
        TYPE_OUTINSINE = 20,   //!< TYPE_OUTINSINE
        TYPE_INEXPO = 21,      //!< TYPE_INEXPO
        TYPE_OUTEXPO = 22,     //!< TYPE_OUTEXPO
        TYPE_INOUTEXPO = 23,   //!< TYPE_INOUTEXPO
        TYPE_OUTINEXPO = 24,   //!< TYPE_OUTINEXPO
        TYPE_INCIRC = 25,      //!< TYPE_INCIRC
        TYPE_OUTCIRC = 26,     //!< TYPE_OUTCIRC
        TYPE_INOUTCIRC = 27,   //!< TYPE_INOUTCIRC
        TYPE_OUTINCIRC = 28,   //!< TYPE_OUTINCIRC
        TYPE_INELASTIC = 29,   //!< TYPE_INELASTIC
        TYPE_OUTELASTIC = 30,  //!< TYPE_OUTELASTIC
        TYPE_INOUTELASTIC = 31,//!< TYPE_INOUTELASTIC
        TYPE_OUTINELASTIC = 32,//!< TYPE_OUTINELASTIC
        TYPE_INBACK = 33,      //!< TYPE_INBACK
        TYPE_OUTBACK = 34,     //!< TYPE_OUTBACK
        TYPE_INOUTBACK = 35,   //!< TYPE_INOUTBACK
        TYPE_OUTINBACK = 36,   //!< TYPE_OUTINBACK
        TYPE_INBOUNCE = 37,    //!< TYPE_INBOUNCE
        TYPE_OUTBOUNCE = 38,   //!< TYPE_OUTBOUNCE
        TYPE_INOUTBOUNCE = 39, //!< TYPE_INOUTBOUNCE
        TYPE_OUTINBOUNCE = 40, //!< TYPE_OUTINBOUNCE
        TYPE_FLOAT_VECTOR = 41,//!< TYPE_FLOAT_VECTOR
        TYPE_COUNT = 42,       //!< TYPE_COUNT
    };

    struct Curve;
    typedef void (*ReleaseCurve)(Curve* curve);

    struct Curve
    {
        Type type;
        dmVMath::FloatVector* vector;

        ReleaseCurve release_callback;
        void* userdata1;
        void* userdata2;

        Curve() {
            type             = TYPE_LINEAR;
            vector           = 0x0;
            userdata1        = 0x0;
            userdata2        = 0x0;
            release_callback = 0x0;
        };
        Curve(Type curve_type) {
            type             = curve_type;
            vector           = 0x0;
            userdata1        = 0x0;
            userdata2        = 0x0;
            release_callback = 0x0;
        };
    };

    /**
     * Easing-curve evaluation
     * @param type curve type
     * @param t time in the range [0,1]
     * @return curve value
     */
    float GetValue(Type type, float t);
    float GetValue(Curve curve, float t);
}

#endif // DM_EASING

