/*
 * 3-D gear wheels.  This program is in the public domain.
 *
 * Command line options:
 *    -info      print GL implementation information
 *    -exit      automatically exit after 30 seconds
 *
 *
 * Brian Paul
 *
 *
 * Marcus Geelnard:
 *   - Conversion to GLFW
 *   - Time based rendering (frame rate independent)
 *   - Slightly modified camera that should work better for stereo viewing
 */

import std.math;
import std.c.string;
import glfw;

static int running = 1;
static double t0 = 0.0;
static double t = 0.0;
static double dt = 0.0;

static int Frames = 0;
static int autoexit = 0;

/**

Draw a gear wheel.  You'll probably want to call this function when
building a display list since we do a lot of trig here.

Input:  inner_radius - radius of hole at center
outer_radius - radius at center of teeth
width - width of gear
teeth - number of teeth
tooth_depth - depth of tooth

**/

static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
  GLint i;
  GLfloat r0, r1, r2;
  GLfloat angle, da;
  GLfloat u, v, len;

  r0 = inner_radius;
  r1 = outer_radius - tooth_depth / 2.0;
  r2 = outer_radius + tooth_depth / 2.0;

  da = 2.0 * PI / teeth / 4.0;

  glShadeModel(GL_FLAT);

  glNormal3f(0.0, 0.0, 1.0);

  /* draw front face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * PI / teeth;
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    if (i < teeth) {
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
    }
  }
  glEnd();

  /* draw front sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * PI / teeth / 4.0;
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * PI / teeth;

    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
  }
  glEnd();

  glNormal3f(0.0, 0.0, -1.0);

  /* draw back face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * PI / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    if (i < teeth) {
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    }
  }
  glEnd();

  /* draw back sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * PI / teeth / 4.0;
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * PI / teeth;

    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
  }
  glEnd();

  /* draw outward faces of teeth */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * PI / teeth;

    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    u = r2 * cos(angle + da) - r1 * cos(angle);
    v = r2 * sin(angle + da) - r1 * sin(angle);
    len = sqrt(u * u + v * v);
    u /= len;
    v /= len;
    glNormal3f(v, -u, 0.0);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
    u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
    v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
    glNormal3f(v, -u, 0.0);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
  }

  glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
  glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

  glEnd();

  glShadeModel(GL_SMOOTH);

  /* draw inside radius cylinder */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * PI / teeth;
    glNormal3f(-cos(angle), -sin(angle), 0.0);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
  }
  glEnd();

}


static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

/* OpenGL draw function & timing */
static void draw()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);
  glRotatef(view_rotz, 0.0, 0.0, 1.0);

  glPushMatrix();
  glTranslatef(-3.0, -2.0, 0.0);
  glRotatef(cast(GLfloat)angle, 0.0, 0.0, 1.0);
  glCallList(gear1);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(3.1, -2.0, 0.0);
  glRotatef(cast(GLfloat)(-2.0 * angle - 9.0), 0.0, 0.0, 1.0);
  glCallList(gear2);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-3.1, 4.2, 0.0);
  glRotatef(cast(GLfloat)(-2.0 * angle - 25.0), 0.0, 0.0, 1.0);
  glCallList(gear3);
  glPopMatrix();

  glPopMatrix();

  Frames++;

  {
    double t_new = glfwGetTime();
    dt = t_new - t;
    t = t_new;

    if (t - t0 >= 5.0)
      {
        double seconds = t - t0;
        double fps = Frames / seconds;
        printf("%d frames in %6.3f seconds = %6.3f FPS\n", Frames, seconds, fps);
        t0 = t;
        Frames = 0;
        if ((t >= 0.999 * autoexit) && (autoexit))
	  {
	    running = 0;
	  }
      }
  }
}


/* update animation parameters */
static void animate()
{
  angle += 100.0*dt;
}


/* change view angle, exit upon ESC */
extern (Windows)
{
  void key( int k, int action )
    {
      if( action != GLFW_PRESS ) return;

      switch (k) {
      case 'Z':
	if( glfwGetKey( GLFW_KEY_LSHIFT ) )
	  view_rotz -= 5.0;
	else
	  view_rotz += 5.0;
	break;
      case GLFW_KEY_ESC:
	running = 0;
	break;
      case GLFW_KEY_UP:
	view_rotx += 5.0;
	break;
      case GLFW_KEY_DOWN:
	view_rotx -= 5.0;
	break;
      case GLFW_KEY_LEFT:
	view_roty += 5.0;
	break;
      case GLFW_KEY_RIGHT:
	view_roty -= 5.0;
	break;
      default:
	return;
      }
    }
}

/* new window size */
extern (Windows)
{
  void reshape( int width, int height )
    {
      GLfloat h = cast(GLfloat) height / cast(GLfloat) width;
      GLfloat xmax, znear, zfar;

      znear = 5.0f;
      zfar  = 30.0f;
      xmax  = znear * 0.5;

      glViewport( 0, 0, cast(GLint) width, cast(GLint) height );
      glMatrixMode( GL_PROJECTION );
      glLoadIdentity();
      glFrustum( -xmax, xmax, -xmax*h, xmax*h, znear, zfar );
      glMatrixMode( GL_MODELVIEW );
      glLoadIdentity();
      glTranslatef( 0.0, 0.0, -20.0 );
    }
}

/* program & OpenGL initialization */
static void init(char[][] args)
{
  static const GLfloat[4] pos = [5.0, 5.0, 10.0, 0.0];
  static const GLfloat[4] red = [0.8, 0.1, 0.0, 1.0];
  static const GLfloat[4] green = [0.0, 0.8, 0.2, 1.0];
  static const GLfloat[4] blue = [0.2, 0.2, 1.0, 1.0];
  GLint i;

  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  /* make the gears */
  gear1 = glGenLists(1);
  glNewList(gear1, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
  gear(1.0, 4.0, 1.0, 20, 0.7);
  glEndList();

  gear2 = glGenLists(1);
  glNewList(gear2, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
  gear(0.5, 2.0, 2.0, 10, 0.7);
  glEndList();

  gear3 = glGenLists(1);
  glNewList(gear3, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
  gear(1.3, 2.0, 0.5, 10, 0.7);
  glEndList();

  glEnable(GL_NORMALIZE);

  for ( i=1; i<args.length; i++ ) {
    if (strcmp(args[i], "-info")==0) {
      printf("GL_RENDERER   = %s\n", cast(char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", cast(char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", cast(char *) glGetString(GL_VENDOR));
      printf("GL_EXTENSIONS = %s\n", cast(char *) glGetString(GL_EXTENSIONS));
    }
    else if ( strcmp(args[i], "-exit")==0) {
      autoexit = 30;
      printf("Auto Exit after %i seconds.\n", autoexit );
    }
  }
}


/* program entry */
int main(char[][] args)
{
  // Init GLFW and open window
  glfwInit();
  if( !glfwOpenWindow( 300,300, 0,0,0,0, 16,0, GLFW_WINDOW ) )
    {
      glfwTerminate();
      return 0;
    }
  glfwSetWindowTitle( "Gears" );
  glfwEnable( GLFW_KEY_REPEAT );
  glfwSwapInterval( 0 );

  // Special args?
  init(args);

  // Set callback functions
  glfwSetWindowSizeCallback( &reshape );
  glfwSetKeyCallback( &key );

  // Main loop
  while( running )
    {
      // Draw gears
      draw();

      // Update animation
      animate();

      // Swap buffers
      glfwSwapBuffers();

      // Was the window closed?
      if( !glfwGetWindowParam( GLFW_OPENED ) )
        {
	  running = 0;
        }
    }

  // Terminate GLFW
  glfwTerminate();

  // Exit program
  return 0;
}
