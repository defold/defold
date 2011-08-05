program mipmaps;

{$IFDEF FPC}
  {$IFDEF WIN32}
    {$APPTYPE GUI}
  {$ENDIF}

{$ENDIF}

//========================================================================
// This is a small test application for GLFW.
// The program shows texture loading with mipmap generation and trilienar
// filtering.
// Note: For OpenGL 1.0 compability, we do not use texture objects (this
// is no issue, since we only have one texture).
//
// Object Pascal conversion by Johannes Stein (http://www.freeze-dev.de)
//========================================================================

uses
  SysUtils, 
  gl, glu,
  glfw;

var
  width, height, running, frames, x, y: Integer;
  t, t0, fps: Double;
  titlestr: string;
  Directory: String;

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
  
  Directory := ExtractFilePath(ParamStr(0));
  if glfwLoadTexture2D(PChar(Directory + 'mipmaps.tga'), GLFW_BUILD_MIPMAPS_BIT) <> 1 then
  begin
    glfwTerminate;
    Exit;
  end;
  
  // Use trilinear interpolation (GL_LINEAR_MIPMAP_LINEAR)
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   GL_LINEAR_MIPMAP_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                   GL_LINEAR );

  // Enable texturing
  glEnable( GL_TEXTURE_2D );
    
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
        titlestr := Format('Trilinear interpolation (%.1f FPS)', [fps]);
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
      gluPerspective( 65.0, width/height, 1.0, 50.0 );

      // Select and setup the modelview matrix
      glMatrixMode( GL_MODELVIEW );
      glLoadIdentity;
      gluLookAt( 0.0, 3.0, -20.0,    // Eye-position
                 0.0, -4.0, -11.0,   // View-point
                 0.0, 1.0, 1.0 );  // Up-vector

        // Draw a textured quad
        glRotatef(0.05*x + t*5.0, 0.0, 1.0, 0.0);
        glBegin( GL_QUADS );
          glTexCoord2f( -20.0,  20.0 );
          glVertex3f( -50.0, 0.0, -50.0 );
          glTexCoord2f(  20.0,  20.0 );
          glVertex3f(  50.0, 0.0, -50.0 );
          glTexCoord2f(  20.0, -20.0 );
          glVertex3f(  50.0, 0.0,  50.0 );
          glTexCoord2f( -20.0, -20.0 );
          glVertex3f( -50.0, 0.0,  50.0 );
        glEnd();

      // Swap buffers
      glfwSwapBuffers;

      // Check if the ESC key was pressed or the window was closed
      running := (not glfwGetKey( GLFW_KEY_ESC )) and
                 glfwGetWindowParam( GLFW_OPENED );  
  end;

end.
