//========================================================================
// This is a small test application for GLFW.
// The program shows texture loading with mipmap generation and trilienar
// filtering.
// Note: For OpenGL 1.0 compability, we do not use texture objects (this
// is no issue, since we only have one texture).
//========================================================================

import glfw;
import std.c.stdio;


//========================================================================
// main()
//========================================================================

int main( )
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

    // Load texture from file, and build all mipmap levels. The
    // texture is automatically uploaded to texture memory.
    if( !glfwLoadTexture2D( "mipmaps.tga", GLFW_BUILD_MIPMAPS_BIT ) )
    {
        glfwTerminate();
        return 0;
    }

    // Use trilinear interpolation (GL_LINEAR_MIPMAP_LINEAR)
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     GL_LINEAR_MIPMAP_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                     GL_LINEAR );

    // Enable texturing
    glEnable( GL_TEXTURE_2D );

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
            fps = cast(double)frames / (t-t0);
            sprintf( titlestr, "Trilinear interpolation (%.1f FPS)", fps );
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
        glClearColor( 0.0f, 0.0f, 0.0f, 0.0f);
        glClear( GL_COLOR_BUFFER_BIT );

        // Select and setup the projection matrix
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        gluPerspective( 65.0f, cast(GLfloat)width/cast(GLfloat)height, 1.0f,
            50.0f );

        // Select and setup the modelview matrix
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        gluLookAt( 0.0f,  3.0f, -20.0f,    // Eye-position
                   0.0f, -4.0f, -11.0f,    // View-point
                   0.0f,  1.0f,   0.0f );  // Up-vector

        // Draw a textured quad
        glRotatef( 0.05*cast(GLfloat)x + cast(GLfloat)t*5.0f, 0.0f, 1.0f, 0.0f );
        glBegin( GL_QUADS );
          glTexCoord2f( -20.0f,  20.0f );
          glVertex3f( -50.0f, 0.0f, -50.0f );
          glTexCoord2f(  20.0f,  20.0f );
          glVertex3f(  50.0f, 0.0f, -50.0f );
          glTexCoord2f(  20.0f, -20.0f );
          glVertex3f(  50.0f, 0.0f,  50.0f );
          glTexCoord2f( -20.0f, -20.0f );
          glVertex3f( -50.0f, 0.0f,  50.0f );
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
