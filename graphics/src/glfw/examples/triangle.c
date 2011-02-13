//========================================================================
// This is a small test application for GLFW.
// The program opens a window (640x480), and renders a spinning colored
// triangle (it is controlled with both the GLFW timer and the mouse). It
// also calculates the rendering speed (FPS), which is displayed in the
// window title bar.
//========================================================================

#include <stdio.h>
#include <GL/glfw.h>


//========================================================================
// main()
//========================================================================

int main( void )
{
    int     width, height, running, frames, x, y;
    double  t, t0, fps;
    char    titlestr[ 200 ];

    // Initialise GLFW
    glfwInit();

    // Open OpenGL window
    if( !glfwOpenWindow( 640, 480, 0,0,0,0, 0,0, GLFW_WINDOW ) )
    {
        glfwTerminate();
        return 0;
    }

    // Enable sticky keys
    glfwEnable( GLFW_STICKY_KEYS );

    // Disable vertical sync (on cards that support it)
    glfwSwapInterval( 0 );

    // Main loop
    running = GL_TRUE;
    frames = 0;
    t0 = glfwGetTime();
    while( running )
    {
        // Get time and mouse position
        t = glfwGetTime();
        glfwGetMousePos( &x, &y );

        // Calculate and display FPS (frames per second)
        if( (t-t0) > 1.0 || frames == 0 )
        {
            fps = (double)frames / (t-t0);
            sprintf( titlestr, "Spinning Triangle (%.1f FPS)", fps );
            glfwSetWindowTitle( titlestr );
            t0 = t;
            frames = 0;
        }
        frames ++;

        // Get window size (may be different than the requested size)
        glfwGetWindowSize( &width, &height );
        height = height > 0 ? height : 1;

        // Set viewport
        glViewport( 0, 0, width, height );

        // Clear color buffer
        glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
        glClear( GL_COLOR_BUFFER_BIT );

        // Select and setup the projection matrix
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        gluPerspective( 65.0f, (GLfloat)width/(GLfloat)height, 1.0f,
            100.0f );

        // Select and setup the modelview matrix
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        gluLookAt( 0.0f, 1.0f, 0.0f,    // Eye-position
                   0.0f, 20.0f, 0.0f,   // View-point
                   0.0f, 0.0f, 1.0f );  // Up-vector

        // Draw a rotating colorful triangle
        glTranslatef( 0.0f, 14.0f, 0.0f );
        glRotatef( 0.3*(GLfloat)x + (GLfloat)t*100.0f, 0.0f, 0.0f, 1.0f );
        glBegin( GL_TRIANGLES );
          glColor3f( 1.0f, 0.0f, 0.0f );
          glVertex3f( -5.0f, 0.0f, -4.0f );
          glColor3f( 0.0f, 1.0f, 0.0f );
          glVertex3f( 5.0f, 0.0f, -4.0f );
          glColor3f( 0.0f, 0.0f, 1.0f );
          glVertex3f( 0.0f, 0.0f, 6.0f );
        glEnd();

        // Swap buffers
        glfwSwapBuffers();

        // Check if the ESC key was pressed or the window was closed
        running = !glfwGetKey( GLFW_KEY_ESC ) &&
                  glfwGetWindowParam( GLFW_OPENED );
    }

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}
