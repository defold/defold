package org.jagatoo.loaders.models.collada;

import org.openmali.FastMath;
import org.openmali.vecmath2.Matrix3f;
import org.openmali.vecmath2.Quaternion4f;
import org.openmali.vecmath2.Tuple3f;
import org.openmali.vecmath2.Vector3f;
import org.openmali.vecmath2.util.MatrixUtils;

/**
 * Rotations utility functions.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Martin Baker (code borrowed from euclideanspace.com)
 */
public class Rotations
{
    /**
     * Converts Euler angles (in radians) to a (normalized) Quaternion.
     * 
     * @param rotX Rotation about the X axis, in degrees
     * @param rotY Rotation about the Y axis, in degrees
     * @param rotZ Rotation about the Z axis, in degrees
     * @param quat destiny quaternion
     * 
     * @return the given quaternion with complete with the values.
     */
    public static Quaternion4f toQuaternion( float rotX, float rotY, float rotZ, Quaternion4f quat )
    {
        /*
        rotX = FastMath.toRad( rotX );
    	rotY = FastMath.toRad( rotY );
        rotZ = FastMath.toRad( rotZ );
        */
        
        Matrix3f matrix = MatrixUtils.eulerToMatrix3f( rotX, rotY, rotZ );
        quat.set( matrix );
        quat.normalize();
        
        return ( quat );
    }
    
    /**
     * Converts Euler angles (in radians) to a (normalized) Quaternion.
     * 
     * @param rotX Rotation about the X axis, in degrees
     * @param rotY Rotation about the Y axis, in degrees
     * @param rotZ Rotation about the Z axis, in degrees
     * 
     * @return The Quaternion representing the same rotation.
     */
    public static Quaternion4f toQuaternion( float rotX, float rotY, float rotZ )
    {
        return ( toQuaternion( rotX, rotY, rotZ, new Quaternion4f() ) );
    }
    
    /**
     * Converts Euler angles (in radians) to a (normalized) Quaternion.
     * 
     * @param tup rotation values
     * @return The Quaternion representing the same rotation.
     */
    public static Quaternion4f toQuaternion( Tuple3f tup )
    {
        return toQuaternion( tup.getX(), tup.getY(), tup.getZ(), new Quaternion4f() );
    }
    
    /**
     * Converts Euler angles (in radians) to a (normalized) Quaternion.
     * 
     * @param tup rotation values
     * @param quat destiny quaternion
     * 
     * @return the given quaternion with complete with the values.
     */
    public static Quaternion4f toQuaternion( Tuple3f tup, Quaternion4f quat )
    {
    	return ( toQuaternion( tup.getX(), tup.getY(), tup.getZ(), quat ) );
    }
    
    /**
     * Converts a Quaternion to Euler angles (in radians).
     * Note : the Quaternion can be non-normalized.
     * 
     * @param quaternion The quaternion
     * 
     * @return The euler angles (in radians).
     */
    public static Tuple3f toEuler( Quaternion4f quaternion )
    {
        Matrix3f matrix = new Matrix3f();
        matrix.set( quaternion );
        
        Tuple3f euler = MatrixUtils.matrixToEuler( matrix );
        euler.setX( euler.getX() );
        euler.setY( euler.getY() );
        euler.setZ( euler.getZ() );
        
        return ( euler );
    }
    
    /**
     * Converts an AxisAngle representation to a Quaternion representation.
     * Credits goes to euclideanspace.com author.
     * 
     * @param axis The axis. Must be unit-length
     * @param angle The angle, in degrees
     * 
     * @return the rotation quaternion.
     */
    public static Quaternion4f toQuaternion( Vector3f axis, float angle ) {
        
        final float sinHalfHangle = FastMath.sin( angle / 2f );
        final float cosHalfHangle = FastMath.cos( angle / 2f );
        
        Quaternion4f quat = new Quaternion4f(
            axis.getX() * sinHalfHangle,
            axis.getY() * sinHalfHangle,
            axis.getZ() * sinHalfHangle,
            cosHalfHangle
        );
        
        return ( quat );
    }
    
    private Rotations() {}
    
    /*
    public static void main( String[] args )
    {
        StringTokenizer tknz = new StringTokenizer( JOptionPane.showInputDialog( "Enter a rotation of the form X, Y, Z"), "," );
        Tuple3f tup = new Tuple3f( Float.parseFloat( tknz.nextToken() ), Float.parseFloat( tknz.nextToken() ), Float.parseFloat( tknz.nextToken() ) );
        Quaternion4f quat = toQuaternion( tup.getX(), tup.getY(), tup.getZ() );
        quat.normalize();
        Tuple3f vec2 = toEuler( quat );
        JOptionPane.showMessageDialog( null, "Original tup : " + tup + "\n Converted quaternion : " + quat + "\n Converted again tuple : " + vec2 );
    }
    */
}
