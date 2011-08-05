program Triangle;

{$IFDEF FPC}
  {$IFDEF WIN32}
    {$APPTYPE GUI}
  {$ENDIF}
{$ENDIF}

//========================================================================
// This is a small test application for GLFW.
// The program opens a window (640x480), and renders a spinning colored
// triangle (it is controlled with both the GLFW timer and the mouse). It
// also calculates the rendering speed (FPS), which is displayed in the
// window title bar.
//
// Delphi conversion by Jarrod Davis (http://ww.jdsgames.com)
//========================================================================

uses
  SysUtils, 
  gl, glu,
  glfw;

var
  width, height, running, frames, x, y: Integer;
  t, t0, fps: Double;
  titlestr: string;

begin

  // Initialize GLFW
  glfwInit;

  // Open OpenGL window
  if glfwOpenWindow( 640, 480, 0,0,0,0, 0,0, GLFW_WINDOW ) <> 1 then
  begin
    glfwTerminate;
    Exit;
  end;
  
  // Enable sticky keys
  glfwEnable( GLFW_STICKY_KEYS );

  // Disable vertical sync (on cards that support it)
  glfwSwapInterval( 0 );

  // Main loop
  running := 1;
  frames := 0;
  t0 := glfwGetTime;
  while running <> 0 do
  begin
      // Get time and mouse position
      t := glfwGetTime;
      glfwGetMousePos(x, y );

      // Calculate and display FPS (frames per second)
      if ((t-t0) > 1.0) or (frames = 0) then
      begin
        fps := frames / (t-t0);
        titlestr := Format('Spinning Triangle (%.1f FPS)', [fps]);
        glfwSetWindowTitle( PChar(titlestr) );
        t0 := t;
        frames := 0;
      end;
      inc(frames);

      // Get window size (may be different than the requested size)
      glfwGetWindowSize( width, height );
      if height < 1 then height := 1;

      // Set viewport
      glViewport( 0, 0, width, height );

      // Clear color buffer
      glClearColor( 0.0, 0.0, 0.0, 0.0 );
      glClear( GL_COLOR_BUFFER_BIT );

      // Select and setup the projection matrix
      glMatrixMode( GL_PROJECTION );
      glLoadIdentity;
      gluPerspective( 65.0, width/height, 1.0, 100.0 );

      // Select and setup the modelview matrix
      glMatrixMode( GL_MODELVIEW );
      glLoadIdentity;
      gluLookAt( 0.0, 1.0, 0.0,    // Eye-position
                 0.0, 20.0, 0.0,   // View-point
                 0.0, 0.0, 1.0 );  // Up-vector

      // Draw a rotating colorful triangle
      glTranslatef( 0.0, 14.0, 0.0 );
      glRotatef( 0.3*x + t*100.0, 0.0, 0.0, 1.0 );
      glBegin( GL_TRIANGLES );
        glColor3f( 1.0, 0.0, 0.0 );
        glVertex3f( -5.0, 0.0, -4.0 );
        glColor3f( 0.0, 1.0, 0.0 );
        glVertex3f( 5.0, 0.0, -4.0 );
        glColor3f( 0.0, 0.0, 1.0 );
        glVertex3f( 0.0, 0.0, 6.0 );
      glEnd;

      // Swap buffers
      glfwSwapBuffers;

      // Check if the ESC key was pressed or the window was closed
      running := (not glfwGetKey( GLFW_KEY_ESC )) and
                 glfwGetWindowParam( GLFW_OPENED );
  end;

  // Close OpenGL window and terminate GLFW
  glfwTerminate;

end.
