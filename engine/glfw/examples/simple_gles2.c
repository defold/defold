//========================================================================
// This is a small test application for GLFW.
// The program opens a window (640x480), and renders a spinning colored
// triangle (it is controlled with both the GLFW timer and the mouse).
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <GL/glfw.h>

#define DEGREES_TO_RADIANS(ANGLE) ((ANGLE) / 180.0 * 3.14159265f)

int main(int argc, char** argv)
{
    int width, height, touchx, touchy, angle;
    double t;

    angle = 0;

    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        exit( EXIT_FAILURE );
    }

    // Open a window and create its OpenGL context
    if( !glfwOpenWindow( 640, 480, 0,0,0,0, 0,0, GLFW_WINDOW ) )
    {
        fprintf( stderr, "Failed to open GLFW window\n" );

        glfwTerminate();
        exit( EXIT_FAILURE );
    }

    glfwSetWindowTitle( "Spinning Triangle" );

    // Ensure we can capture the escape key being pressed below
    glfwEnable( GLFW_STICKY_KEYS );

    // Enable vertical sync (on cards that support it)
    glfwSwapInterval( 1 );

    do
    {
        t = glfwGetTime();
        glfwGetMousePos( &touchx, &touchy );

        // Get window size (may be different than the requested size)
        glfwGetWindowSize( &width, &height );

        // Special case: avoid division by zero below
        height = height > 0 ? height : 1;


        const GLfloat zNear = 0.1, zFar = 1000.0, fieldOfView = 60.0;
        GLfloat size;

        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        size = zNear * tanf(DEGREES_TO_RADIANS(fieldOfView) / 2.0);


        float aspect = (float) width / (float) height;

        glFrustumf(-size, size,
                   -size / aspect,
                   size / aspect,
                   zNear, zFar);

        glViewport( 0, 0, width, height );

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);


        // Clear color buffer to black
        glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
        glClear( GL_COLOR_BUFFER_BIT );

        const GLfloat triangleVertices[] =
        {
          0.0, 1.5, -6.0,      // top center
          -1.5, -1.5, -6.0,    // bottom left
          1.5, -1.5, -6.0      // bottom right
        };

        const GLfloat triangleColors[] =
        {
          1.0, 0.0, 0.0, 1.0,  // red
          0.0, 1.0, 0.0, 1.0,  // green
          0.0, 0.0, 1.0, 1.0,  // blue
        };

        //Draw stuff

        //clear the back color back to our original color and depth buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //reset matrix to identity
        glLoadIdentity();
        //rough approximation of screen to current 3D space
        GLfloat x = (touchx - 160.0) / 38.0;
        GLfloat y = (240.0 - touchy) / 38.0;
        //translate the triangle
        glTranslatef(x, y, -1.0);
        glRotatef(angle, 0, 0, 1);
        angle += 10.0f;
        //set the format and location for verticies
        glVertexPointer(3, GL_FLOAT, 0, triangleVertices);
        //set the opengl state
        glEnableClientState(GL_VERTEX_ARRAY);
        //set the format and location for colors
        glColorPointer(4, GL_FLOAT, 0, triangleColors);
        //set the opengl state
        glEnableClientState(GL_COLOR_ARRAY);
        //draw the triangles
        glDrawArrays(GL_TRIANGLES, 0, 3);

#if 0
        // Select and setup the projection matrix
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        gluPerspective( 65.0f, (GLfloat)width/(GLfloat)height, 1.0f, 100.0f );

        // Select and setup the modelview matrix
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        gluLookAt( 0.0f, 1.0f, 0.0f,    // Eye-position
                   0.0f, 20.0f, 0.0f,   // View-point
                   0.0f, 0.0f, 1.0f );  // Up-vector

        // Draw a rotating colorful triangle
        glTranslatef( 0.0f, 14.0f, 0.0f );
        glRotatef( 0.3f*(GLfloat)x + (GLfloat)t*100.0f, 0.0f, 0.0f, 1.0f );
        glBegin( GL_TRIANGLES );
          glColor3f( 1.0f, 0.0f, 0.0f );
          glVertex3f( -5.0f, 0.0f, -4.0f );
          glColor3f( 0.0f, 1.0f, 0.0f );
          glVertex3f( 5.0f, 0.0f, -4.0f );
          glColor3f( 0.0f, 0.0f, 1.0f );
          glVertex3f( 0.0f, 0.0f, 6.0f );
        glEnd();
#endif

        // Swap buffers
        glfwSwapBuffers();

    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey( GLFW_KEY_ESC ) != GLFW_PRESS &&
           glfwGetWindowParam( GLFW_OPENED ) );

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    exit( EXIT_SUCCESS );
}

