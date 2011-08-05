//========================================================================
// This is a small test application for GLFW.
// The program uses a "split window" view, rendering four views of the
// same scene in one window (e.g. uesful for 3D modelling software). This
// demo uses scissors to separete the four different rendering areas from
// each other.
//
// This code was ported straight from C, so it may not be a school book
// example of D programming, but it shows how to use GLFW callbacks in D.
//========================================================================

import std.math;
import std.string;
import glfw;



//========================================================================
// Global variables
//========================================================================

// Mouse position
int xpos = 0, ypos = 0;

// Window size
int width, height;

// Active view: 0 = none, 1 = upper left, 2 = upper right, 3 = lower left,
// 4 = lower right
int active_view = 0;

// Rotation around each axis
int rot_x = 0, rot_y = 0, rot_z = 0;


//========================================================================
// DrawTorus() - Draw a solid torus (use a display list for the model)
//========================================================================

const double TORUS_MAJOR     = 1.5;
const double TORUS_MINOR     = 0.5;
const int    TORUS_MAJOR_RES = 32;
const int    TORUS_MINOR_RES = 32;

void DrawTorus()
{
    static GLuint torus_list = 0;
    int    i, j, k;
    double s, t, x, y, z, nx, ny, nz, scale, twopi;

    if( !torus_list )
    {
        // Start recording displaylist
        torus_list = glGenLists( 1 );
        glNewList( torus_list, GL_COMPILE_AND_EXECUTE );

        // Draw torus
        twopi = 2.0 * PI;
        for( i = 0; i < TORUS_MINOR_RES; i++ )
        {
            glBegin( GL_QUAD_STRIP );
            for( j = 0; j <= TORUS_MAJOR_RES; j++ )
            {
                for( k = 1; k >= 0; k-- )
                {
                    s = (i + k) % TORUS_MINOR_RES + 0.5;
                    t = j % TORUS_MAJOR_RES;

                    // Calculate point on surface
                    x = (TORUS_MAJOR+TORUS_MINOR*cos(s*twopi/TORUS_MINOR_RES))*cos(t*twopi/TORUS_MAJOR_RES);
                    y = TORUS_MINOR * sin(s * twopi / TORUS_MINOR_RES);
                    z = (TORUS_MAJOR+TORUS_MINOR*cos(s*twopi/TORUS_MINOR_RES))*sin(t*twopi/TORUS_MAJOR_RES);

                    // Calculate surface normal
                    nx = x - TORUS_MAJOR*cos(t*twopi/TORUS_MAJOR_RES);
                    ny = y;
                    nz = z - TORUS_MAJOR*sin(t*twopi/TORUS_MAJOR_RES);
                    scale = 1.0 / sqrt( nx*nx + ny*ny + nz*nz );
                    nx *= scale;
                    ny *= scale;
                    nz *= scale;

                    glNormal3f( cast(float)nx, cast(float)ny, cast(float)nz );
                    glVertex3f( cast(float)x, cast(float)y, cast(float)z );
                }
            }
            glEnd();
        }

        // Stop recording displaylist
        glEndList();
    }
    else
    {
        // Playback displaylist
        glCallList( torus_list );
    }
}


//========================================================================
// DrawScene() - Draw the scene (a rotating torus)
//========================================================================

void DrawScene()
{
    const GLfloat[4] model_diffuse   = [1.0f, 0.8f, 0.8f, 1.0f];
    const GLfloat[4] model_specular  = [0.6f, 0.6f, 0.6f, 1.0f];
    const GLfloat    model_shininess = 20.0f;

    glPushMatrix();

    // Rotate the object
    glRotatef( cast(GLfloat)rot_x*0.5f, 1.0f, 0.0f, 0.0f );
    glRotatef( cast(GLfloat)rot_y*0.5f, 0.0f, 1.0f, 0.0f );
    glRotatef( cast(GLfloat)rot_z*0.5f, 0.0f, 0.0f, 1.0f );

    // Set model color (used for orthogonal views, lighting disabled)
    glColor4fv( model_diffuse );

    // Set model material (used for perspective view, lighting enabled)
    glMaterialfv( GL_FRONT, GL_DIFFUSE, model_diffuse );
    glMaterialfv( GL_FRONT, GL_SPECULAR, model_specular );
    glMaterialf(  GL_FRONT, GL_SHININESS, model_shininess );

    // Draw torus
    DrawTorus();

    glPopMatrix();
}


//========================================================================
// DrawGrid() - Draw a 2D grid (used for orthogonal views)
//========================================================================

void DrawGrid( float scale, int steps )
{
    int   i;
    float x, y;

    glPushMatrix();

    // Set background to some dark bluish grey
    glClearColor( 0.05f, 0.05f, 0.2f, 0.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    // Setup modelview matrix (flat XY view)
    glLoadIdentity();
    gluLookAt( 0.0, 0.0, 1.0,
               0.0, 0.0, 0.0,
               0.0, 1.0, 0.0 );

    // We don't want to update the Z-buffer
    glDepthMask( GL_FALSE );

    // Set grid color
    glColor3f( 0.0f, 0.5f, 0.5f );

    glBegin( GL_LINES );

    // Horizontal lines
    x = scale * 0.5f * cast(float)(steps-1);
    y = -scale * 0.5f * cast(float)(steps-1);
    for( i = 0; i < steps; i ++ )
    {
        glVertex3f( -x, y, 0.0f );
        glVertex3f( x, y, 0.0f );
        y += scale;
    }

    // Vertical lines
    x = -scale * 0.5f * cast(float)(steps-1);
    y = scale * 0.5f * cast(float)(steps-1);
    for( i = 0; i < steps; i ++ )
    {
        glVertex3f( x, -y, 0.0f );
        glVertex3f( x, y, 0.0f );
        x += scale;
    }

    glEnd();

    // Enable Z-buffer writing again
    glDepthMask( GL_TRUE );

    glPopMatrix();
}


//========================================================================
// DrawAllViews()
//========================================================================

void DrawAllViews()
{
    const GLfloat[4] light_position = [0.0f, 8.0f, 8.0f, 1.0f];
    const GLfloat[4] light_diffuse  = [1.0f, 1.0f, 1.0f, 1.0f];
    const GLfloat[4] light_specular = [1.0f, 1.0f, 1.0f, 1.0f];
    const GLfloat[4] light_ambient  = [0.2f, 0.2f, 0.3f, 1.0f];
    double aspect;

    // Calculate aspect of window
    if( height > 0 )
    {
        aspect = cast(double)width / cast(double)height;
    }
    else
    {
        aspect = 1.0;
    }

    // Clear screen
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // Enable scissor test
    glEnable( GL_SCISSOR_TEST );

    // Enable depth test
    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LEQUAL );


    // ** ORTHOGONAL VIEWS **

    // For orthogonal views, use wireframe rendering
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    // Enable line anti-aliasing
    glEnable( GL_LINE_SMOOTH );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    // Setup orthogonal projection matrix
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( -3.0*aspect, 3.0*aspect, -3.0, 3.0, 1.0, 50.0 );

    // Upper left view (TOP VIEW)
    glViewport( 0, height/2, width/2, height/2 );
    glScissor( 0, height/2, width/2, height/2 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    gluLookAt( 0.0f, 10.0f, 1e-3f,   // Eye-position (above)
               0.0f, 0.0f, 0.0f,     // View-point
               0.0f, 1.0f, 0.0f );   // Up-vector
    DrawGrid( 0.5, 12 );
    DrawScene();

    // Lower left view (FRONT VIEW)
    glViewport( 0, 0, width/2, height/2 );
    glScissor( 0, 0, width/2, height/2 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    gluLookAt( 0.0f, 0.0f, 10.0f,    // Eye-position (in front of)
               0.0f, 0.0f, 0.0f,     // View-point
               0.0f, 1.0f, 0.0f );   // Up-vector
    DrawGrid( 0.5, 12 );
    DrawScene();

    // Lower right view (SIDE VIEW)
    glViewport( width/2, 0, width/2, height/2 );
    glScissor( width/2, 0, width/2, height/2 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    gluLookAt( 10.0f, 0.0f, 0.0f,    // Eye-position (to the right)
               0.0f, 0.0f, 0.0f,     // View-point
               0.0f, 1.0f, 0.0f );   // Up-vector
    DrawGrid( 0.5, 12 );
    DrawScene();

    // Disable line anti-aliasing
    glDisable( GL_LINE_SMOOTH );
    glDisable( GL_BLEND );


    // ** PERSPECTIVE VIEW **

    // For perspective view, use solid rendering
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

    // Enable face culling (faster rendering)
    glEnable( GL_CULL_FACE );
    glCullFace( GL_BACK );
    glFrontFace( GL_CW );

    // Setup perspective projection matrix
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    gluPerspective( 65.0f, aspect, 1.0f, 50.0f );

    // Upper right view (PERSPECTIVE VIEW)
    glViewport( width/2, height/2, width/2, height/2 );
    glScissor( width/2, height/2, width/2, height/2 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    gluLookAt( 3.0f, 1.5f, 3.0f,     // Eye-position
               0.0f, 0.0f, 0.0f,     // View-point
               0.0f, 1.0f, 0.0f );   // Up-vector

    // Configure and enable light source 1
    glLightfv( GL_LIGHT1, GL_POSITION, light_position );
    glLightfv( GL_LIGHT1, GL_AMBIENT, light_ambient );
    glLightfv( GL_LIGHT1, GL_DIFFUSE, light_diffuse );
    glLightfv( GL_LIGHT1, GL_SPECULAR, light_specular );
    glEnable( GL_LIGHT1 );
    glEnable( GL_LIGHTING );

    // Draw scene
    DrawScene();

    // Disable lighting
    glDisable( GL_LIGHTING );

    // Disable face culling
    glDisable( GL_CULL_FACE );

    // Disable depth test
    glDisable( GL_DEPTH_TEST );

    // Disable scissor test
    glDisable( GL_SCISSOR_TEST );


    // Draw a border around the active view
    if( active_view > 0 && active_view != 2 )
    {
        glViewport( 0, 0, width, height );
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        glOrtho( 0.0, 2.0, 0.0, 2.0, 0.0, 1.0 );
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        glColor3f( 1.0f, 1.0f, 0.6f );
        glTranslatef( (active_view-1)&1, 1-(active_view-1)/2, 0.0f );
        glBegin( GL_LINE_STRIP );
          glVertex2i( 0, 0 );
          glVertex2i( 1, 0 );
          glVertex2i( 1, 1 );
          glVertex2i( 0, 1 );
          glVertex2i( 0, 0 );
        glEnd();
    }
}


//========================================================================
// WindowSizeFun() - Window size callback function
//========================================================================

extern (Windows)
{
  void WindowSizeFun( int w, int h )
  {
      width  = w;
      height = h > 0 ? h : 1;
  }
}

//========================================================================
// MousePosFun() - Mouse position callback function
//========================================================================

extern (Windows)
{
  void MousePosFun( int x, int y )
  {
      // Depending on which view was selected, rotate around different axes
      switch( active_view )
      {
          case 1:
              rot_x += y - ypos;
              rot_z += x - xpos;
              break;
          case 3:
              rot_x += y - ypos;
              rot_y += x - xpos;
              break;
          case 4:
              rot_y += x - xpos;
              rot_z += y - ypos;
              break;
          default:
              // Do nothing for perspective view, or if no view is selected
              break;
      }

      // Remember mouse position
      xpos = x;
      ypos = y;
  }
}


//========================================================================
// MouseButtonFun() - Mouse button callback function
//========================================================================

extern (Windows)
{
  void MouseButtonFun( int button, int action )
  {
      // Button clicked?
      if( ( button == GLFW_MOUSE_BUTTON_LEFT ) && action == GLFW_PRESS )
      {
          // Detect which of the four views was clicked
          active_view = 1;
          if( xpos >= width/2 )
          {
              active_view += 1;
          }
          if( ypos >= height/2 )
          {
              active_view += 2;
          }
      }

      // Button released?
      else if( button == GLFW_MOUSE_BUTTON_LEFT )
      {
          // Deselect any previously selected view
          active_view = 0;
      }
  }
}


//========================================================================
// main()
//========================================================================

int main()
{
    int     running;

    // Initialise GLFW
    glfwInit();

    // Open OpenGL window
    if( !glfwOpenWindow( 500, 500, 0,0,0,0, 16,0, GLFW_WINDOW ) )
    {
        glfwTerminate();
        return 0;
    }

    // Set window title
    glfwSetWindowTitle( "Split view demo\0" );

    // Enable sticky keys
    glfwEnable( GLFW_STICKY_KEYS );

    // Enable mouse cursor (only needed for fullscreen mode)
    glfwEnable( GLFW_MOUSE_CURSOR );

    // Disable automatic event polling
    glfwDisable( GLFW_AUTO_POLL_EVENTS );

    // Set callback functions
    glfwSetWindowSizeCallback( &WindowSizeFun );
    glfwSetMousePosCallback( &MousePosFun );
    glfwSetMouseButtonCallback( &MouseButtonFun );

    // Main loop
    do
    {
        // Draw all views
        DrawAllViews();

        // Swap buffers
        glfwSwapBuffers();

        // Wait for new events
        glfwWaitEvents();

        // Check if the ESC key was pressed or the window was closed
        running = !glfwGetKey( GLFW_KEY_ESC ) &&
                  glfwGetWindowParam( GLFW_OPENED );
    }
    while( running );

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}
