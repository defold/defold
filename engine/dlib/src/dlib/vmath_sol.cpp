#include "transform.h"
#include <assert.h>

using namespace dmTransform;
using namespace Vectormath::Aos;

namespace dmVMath
{
    void ToSolMatrix(Matrix4 const &mtx, float *out)
    {
        assert(out);
        for (int c=0;c<4;c++)
        {
            for (int r=0;r<4;r++)
            {
                *out++ = mtx.getElem(c, r);
            }
        }
    }

    extern "C" void SolQuatToMatrix(float* out, float x, float y, float z, float w)
    {
        assert(out);
        Matrix4 mtx(Quat(x, y, z, w), Vector3(0,0,0));
        ToSolMatrix(mtx, out);
    }
}
