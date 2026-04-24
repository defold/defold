// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package javax.vecmath;

public final class VecmathUtils {
    public static final double EPSILON = 0.000001;

    private static Vector3d orthogonalUnitVector(Vector3d axis) {
        var absX = Math.abs(axis.x);
        var absY = Math.abs(axis.y);
        var absZ = Math.abs(axis.z);
        var candidate = new Vector3d();

        if (absX <= absY && absX <= absZ) {
            candidate.set(1.0, 0.0, 0.0);
        } else if (absY <= absZ) {
            candidate.set(0.0, 1.0, 0.0);
        } else {
            candidate.set(0.0, 0.0, 1.0);
        }

        var orthogonal = new Vector3d();
        orthogonal.cross(axis, candidate);
        orthogonal.normalize();
        return orthogonal;
    }

    private static Vector3d scaledColumn(Matrix3d matrix, int column, double scale) {
        var result = new Vector3d();
        matrix.getColumn(column, result);
        result.scale(scale);
        return result;
    }

    private static void completeOrthonormalBasis(Matrix3d inOutRotationMatrix, Tuple3d scale) {
        var hasXScale = Math.abs(scale.x) > EPSILON;
        var hasYScale = Math.abs(scale.y) > EPSILON;
        var hasZScale = Math.abs(scale.z) > EPSILON;
        var nonZeroScaleAxisCount = (hasXScale ? 1 : 0)
                                  + (hasYScale ? 1 : 0)
                                  + (hasZScale ? 1 : 0);

        if (nonZeroScaleAxisCount == 0) {
            inOutRotationMatrix.setIdentity();
            return;
        }

        if (nonZeroScaleAxisCount == 1) {
            if (hasXScale) {
                var axisX = scaledColumn(inOutRotationMatrix, 0, 1.0 / scale.x);
                var axisY = orthogonalUnitVector(axisX);
                var axisZ = new Vector3d();
                axisZ.cross(axisX, axisY);
                axisZ.normalize();
                axisY.cross(axisZ, axisX);
                inOutRotationMatrix.setColumn(0, axisX);
                inOutRotationMatrix.setColumn(1, axisY);
                inOutRotationMatrix.setColumn(2, axisZ);
            } else if (hasYScale) {
                var axisY = scaledColumn(inOutRotationMatrix, 1, 1.0 / scale.y);
                var axisZ = orthogonalUnitVector(axisY);
                var axisX = new Vector3d();
                axisX.cross(axisY, axisZ);
                axisX.normalize();
                axisZ.cross(axisX, axisY);
                inOutRotationMatrix.setColumn(0, axisX);
                inOutRotationMatrix.setColumn(1, axisY);
                inOutRotationMatrix.setColumn(2, axisZ);
            } else {
                var axisZ = scaledColumn(inOutRotationMatrix, 2, 1.0 / scale.z);
                var axisX = orthogonalUnitVector(axisZ);
                var axisY = new Vector3d();
                axisY.cross(axisZ, axisX);
                axisY.normalize();
                axisX.cross(axisY, axisZ);
                inOutRotationMatrix.setColumn(0, axisX);
                inOutRotationMatrix.setColumn(1, axisY);
                inOutRotationMatrix.setColumn(2, axisZ);
            }

            return;
        }

        if (!hasXScale) {
            var axisY = scaledColumn(inOutRotationMatrix, 1, 1.0 / scale.y);
            var axisZ = scaledColumn(inOutRotationMatrix, 2, 1.0 / scale.z);
            var axisX = new Vector3d();
            axisX.cross(axisY, axisZ);
            axisX.normalize();
            axisZ.cross(axisX, axisY);
            inOutRotationMatrix.setColumn(0, axisX);
            inOutRotationMatrix.setColumn(1, axisY);
            inOutRotationMatrix.setColumn(2, axisZ);
            return;
        }

        if (!hasYScale) {
            var axisX = scaledColumn(inOutRotationMatrix, 0, 1.0 / scale.x);
            var axisZ = scaledColumn(inOutRotationMatrix, 2, 1.0 / scale.z);
            var axisY = new Vector3d();
            axisY.cross(axisZ, axisX);
            axisY.normalize();
            axisZ.cross(axisX, axisY);
            inOutRotationMatrix.setColumn(0, axisX);
            inOutRotationMatrix.setColumn(1, axisY);
            inOutRotationMatrix.setColumn(2, axisZ);
            return;
        }

        if (!hasZScale) {
            var axisX = scaledColumn(inOutRotationMatrix, 0, 1.0 / scale.x);
            var axisY = scaledColumn(inOutRotationMatrix, 1, 1.0 / scale.y);
            var axisZ = new Vector3d();
            axisZ.cross(axisX, axisY);
            axisZ.normalize();
            axisY.cross(axisZ, axisX);
            inOutRotationMatrix.setColumn(0, axisX);
            inOutRotationMatrix.setColumn(1, axisY);
            inOutRotationMatrix.setColumn(2, axisZ);
        }
    }

    public static void assignFromOrthonormal(Quat4d target, Matrix3d source) {
        var tr = source.m00 + source.m11 + source.m22;

        if (tr > 0) {
            var r = Math.sqrt(1.0 + tr);
            var s = r * 2.0;
            target.x = (source.m21 - source.m12) / s;
            target.y = (source.m02 - source.m20) / s;
            target.z = (source.m10 - source.m01) / s;
            target.w = r * 0.5;
        } else if (source.m00 > source.m11 && source.m00 > source.m22) {
            var r = Math.sqrt(1.0 + source.m00 - source.m11 - source.m22);
            var s = r * 2.0;
            target.x = r * 0.5;
            target.y = (source.m01 + source.m10) / s;
            target.z = (source.m02 + source.m20) / s;
            target.w = (source.m21 - source.m12) / s;
        } else if (source.m11 > source.m22) {
            var r = Math.sqrt(1.0 + source.m11 - source.m00 - source.m22);
            var s = r * 2.0;
            target.x = (source.m01 + source.m10) / s;
            target.y = r * 0.5;
            target.z = (source.m12 + source.m21) / s;
            target.w = (source.m02 - source.m20) / s;
        } else {
            var r = Math.sqrt(1.0 + source.m22 - source.m00 - source.m11);
            var s = r * 2.0;
            target.x = (source.m02 + source.m20) / s;
            target.y = (source.m12 + source.m21) / s;
            target.z = r * 0.5;
            target.w = (source.m10 - source.m01) / s;
        }
    }

    private static double[] getRotationScaleMatrixArray(Matrix3d matrix) {
        var matrixArray = new double[9];

        matrixArray[0] = matrix.m00;
        matrixArray[1] = matrix.m01;
        matrixArray[2] = matrix.m02;

        matrixArray[3] = matrix.m10;
        matrixArray[4] = matrix.m11;
        matrixArray[5] = matrix.m12;

        matrixArray[6] = matrix.m20;
        matrixArray[7] = matrix.m21;
        matrixArray[8] = matrix.m22;

        return matrixArray;
    }

    private static double[] getRotationScaleMatrixArray(Matrix4d matrix) {
        var matrixArray = new double[9];

        matrixArray[0] = matrix.m00;
        matrixArray[1] = matrix.m01;
        matrixArray[2] = matrix.m02;

        matrixArray[3] = matrix.m10;
        matrixArray[4] = matrix.m11;
        matrixArray[5] = matrix.m12;

        matrixArray[6] = matrix.m20;
        matrixArray[7] = matrix.m21;
        matrixArray[8] = matrix.m22;

        return matrixArray;
    }

    public static void cancelScale(Matrix3d inOutRotationScaleMatrix, Tuple3d scale) {
        if (Math.abs(scale.x) > EPSILON) {
            var scaleXInverse = 1.0 / scale.x;
            inOutRotationScaleMatrix.m00 *= scaleXInverse;
            inOutRotationScaleMatrix.m10 *= scaleXInverse;
            inOutRotationScaleMatrix.m20 *= scaleXInverse;
        }

        if (Math.abs(scale.y) > EPSILON) {
            var scaleYInverse = 1.0 / scale.y;
            inOutRotationScaleMatrix.m01 *= scaleYInverse;
            inOutRotationScaleMatrix.m11 *= scaleYInverse;
            inOutRotationScaleMatrix.m21 *= scaleYInverse;
        }

        if (Math.abs(scale.z) > EPSILON) {
            var scaleZInverse = 1.0 / scale.z;
            inOutRotationScaleMatrix.m02 *= scaleZInverse;
            inOutRotationScaleMatrix.m12 *= scaleZInverse;
            inOutRotationScaleMatrix.m22 *= scaleZInverse;
        }
    }

    public static void correctHandedness(Matrix3d inOutRotationMatrix, Tuple3d inOutScale) {
        // The rotation matrix might have a different handedness than we expect.
        // This can happen when there is a negatively scaled axis. Correct the
        // handedness of the rotation part and invert the scale factor for one
        // axis if this is the case. Unfortunately, the combination of rotations
        // and negative scale factors is ambiguous, so we're forced to just pick
        // one axis arbitrarily to flip here.
        var axisX = new Vector3d(inOutRotationMatrix.m00, inOutRotationMatrix.m10, inOutRotationMatrix.m20);
        var axisY = new Vector3d(inOutRotationMatrix.m01, inOutRotationMatrix.m11, inOutRotationMatrix.m21);
        var axisZ = new Vector3d(inOutRotationMatrix.m02, inOutRotationMatrix.m12, inOutRotationMatrix.m22);
        var axisXYCross = new Vector3d();
        axisXYCross.cross(axisX, axisY);

        if (axisXYCross.dot(axisZ) < 0.0) {
            inOutRotationMatrix.m02 = -axisZ.x;
            inOutRotationMatrix.m12 = -axisZ.y;
            inOutRotationMatrix.m22 = -axisZ.z;
            inOutScale.z = -inOutScale.z;
        }
    }

    public static void extractTranslation(Matrix4d matrix, Tuple3d outTranslation) {
        outTranslation.set(matrix.m03, matrix.m13, matrix.m23);
    }

    public static void extractRotationScaleOrthogonal(Matrix3d rotationScaleMatrix, Matrix3d outRotationMatrix, Tuple3d outScale) {
        var sx = Math.sqrt(rotationScaleMatrix.m00 * rotationScaleMatrix.m00 + rotationScaleMatrix.m10 * rotationScaleMatrix.m10 + rotationScaleMatrix.m20 * rotationScaleMatrix.m20);
        var sy = Math.sqrt(rotationScaleMatrix.m01 * rotationScaleMatrix.m01 + rotationScaleMatrix.m11 * rotationScaleMatrix.m11 + rotationScaleMatrix.m21 * rotationScaleMatrix.m21);
        var sz = Math.sqrt(rotationScaleMatrix.m02 * rotationScaleMatrix.m02 + rotationScaleMatrix.m12 * rotationScaleMatrix.m12 + rotationScaleMatrix.m22 * rotationScaleMatrix.m22);
        outRotationMatrix.set(rotationScaleMatrix);
        outScale.set(sx, sy, sz);
        cancelScale(outRotationMatrix, outScale);
        completeOrthonormalBasis(outRotationMatrix, outScale);
        correctHandedness(outRotationMatrix, outScale);
    }

    public static void extractRotationScaleOrthogonal(Matrix4d rotationScaleMatrix, Matrix3d outRotationMatrix, Tuple3d outScale) {
        var sx = Math.sqrt(rotationScaleMatrix.m00 * rotationScaleMatrix.m00 + rotationScaleMatrix.m10 * rotationScaleMatrix.m10 + rotationScaleMatrix.m20 * rotationScaleMatrix.m20);
        var sy = Math.sqrt(rotationScaleMatrix.m01 * rotationScaleMatrix.m01 + rotationScaleMatrix.m11 * rotationScaleMatrix.m11 + rotationScaleMatrix.m21 * rotationScaleMatrix.m21);
        var sz = Math.sqrt(rotationScaleMatrix.m02 * rotationScaleMatrix.m02 + rotationScaleMatrix.m12 * rotationScaleMatrix.m12 + rotationScaleMatrix.m22 * rotationScaleMatrix.m22);
        outRotationMatrix.m00 = rotationScaleMatrix.m00;
        outRotationMatrix.m10 = rotationScaleMatrix.m10;
        outRotationMatrix.m20 = rotationScaleMatrix.m20;
        outRotationMatrix.m01 = rotationScaleMatrix.m01;
        outRotationMatrix.m11 = rotationScaleMatrix.m11;
        outRotationMatrix.m21 = rotationScaleMatrix.m21;
        outRotationMatrix.m02 = rotationScaleMatrix.m02;
        outRotationMatrix.m12 = rotationScaleMatrix.m12;
        outRotationMatrix.m22 = rotationScaleMatrix.m22;
        outScale.set(sx, sy, sz);
        cancelScale(outRotationMatrix, outScale);
        completeOrthonormalBasis(outRotationMatrix, outScale);
        correctHandedness(outRotationMatrix, outScale);
    }

    public static void extractRotationScaleOrthogonal(Matrix3d rotationScaleMatrix, Quat4d outRotation, Tuple3d outScale) {
        var rotationMatrix = new Matrix3d();
        extractRotationScaleOrthogonal(rotationScaleMatrix, rotationMatrix, outScale);
        assignFromOrthonormal(outRotation, rotationMatrix);
    }

    public static void extractRotationScaleOrthogonal(Matrix4d rotationScaleMatrix, Quat4d outRotation, Tuple3d outScale) {
        var rotationMatrix = new Matrix3d();
        extractRotationScaleOrthogonal(rotationScaleMatrix, rotationMatrix, outScale);
        assignFromOrthonormal(outRotation, rotationMatrix);
    }

    public static void extractTranslationRotationScaleOrthogonal(Matrix4d matrix, Tuple3d outTranslation, Matrix3d outRotationMatrix, Tuple3d outScale) {
        extractTranslation(matrix, outTranslation);
        extractRotationScaleOrthogonal(matrix, outRotationMatrix, outScale);
    }

    public static void extractTranslationRotationScaleOrthogonal(Matrix4d matrix, Tuple3d outTranslation, Quat4d outRotation, Tuple3d outScale) {
        extractTranslation(matrix, outTranslation);
        extractRotationScaleOrthogonal(matrix, outRotation, outScale);
    }

    private static void extractRotationScaleSVD(double[] rotationScaleMatrixArray, Matrix3d outRotationMatrix, Tuple3d outScale) {
        assert(rotationScaleMatrixArray.length == 9);
        var scaleArray = new double[3];
        var rotationMatrixArray = new double[9];
        Matrix3d.compute_svd(rotationScaleMatrixArray, scaleArray, rotationMatrixArray);
        outRotationMatrix.set(rotationMatrixArray);
        outScale.set(scaleArray);
        completeOrthonormalBasis(outRotationMatrix, outScale);
        correctHandedness(outRotationMatrix, outScale);
    }

    public static void extractRotationScaleSVD(Matrix3d matrix, Matrix3d outRotationMatrix, Tuple3d outScale) {
        var rotationScaleMatrixArray = getRotationScaleMatrixArray(matrix);
        extractRotationScaleSVD(rotationScaleMatrixArray, outRotationMatrix, outScale);
    }

    public static void extractRotationScaleSVD(Matrix4d matrix, Matrix3d outRotationMatrix, Tuple3d outScale) {
        var rotationScaleMatrixArray = getRotationScaleMatrixArray(matrix);
        extractRotationScaleSVD(rotationScaleMatrixArray, outRotationMatrix, outScale);
    }

    private static void extractRotationScaleSVD(double[] rotationScaleMatrixArray, Quat4d outRotation, Tuple3d outScale) {
        var rotationMatrix = new Matrix3d();
        extractRotationScaleSVD(rotationScaleMatrixArray, rotationMatrix, outScale);
        assignFromOrthonormal(outRotation, rotationMatrix);
    }

    public static void extractRotationScaleSVD(Matrix3d matrix, Quat4d outRotation, Tuple3d outScale) {
        var rotationScaleMatrixArray = getRotationScaleMatrixArray(matrix);
        extractRotationScaleSVD(rotationScaleMatrixArray, outRotation, outScale);
    }

    public static void extractRotationScaleSVD(Matrix4d matrix, Quat4d outRotation, Tuple3d outScale) {
        var rotationScaleMatrixArray = getRotationScaleMatrixArray(matrix);
        extractRotationScaleSVD(rotationScaleMatrixArray, outRotation, outScale);
    }

    public static void extractTranslationRotationScaleSVD(Matrix4d matrix, Tuple3d outTranslation, Matrix3d outRotationMatrix, Tuple3d outScale) {
        extractTranslation(matrix, outTranslation);
        extractRotationScaleSVD(matrix, outRotationMatrix, outScale);
    }

    public static void extractTranslationRotationScaleSVD(Matrix4d matrix, Tuple3d outTranslation, Quat4d outRotation, Tuple3d outScale) {
        extractTranslation(matrix, outTranslation);
        extractRotationScaleSVD(matrix, outRotation, outScale);
    }
}
