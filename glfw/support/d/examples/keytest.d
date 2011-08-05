//========================================================================
// This is a small test application for GLFW.
// Keyboard input test.
//========================================================================

import glfw;
import std.math;
import std.string;
import std.c.stdio;

int running = 0;
int keyrepeat  = 0;
int systemkeys = 1;


//========================================================================
// main()
//========================================================================

int main( )
{
    int     width, height;
    double  t;

    // Initialise GLFW
    glfwInit();

    // Open OpenGL window
    if( !glfwOpenWindow( 250,100, 0,0,0,0, 0,0, GLFW_WINDOW ) )
    {
        glfwTerminate();
        return 0;
    }

    // Set key callback function
    glfwSetKeyCallback( &keyfun );

    // Set tile
    glfwSetWindowTitle( "Press some keys!" );

    // Main loop
    running = GL_TRUE;
    while( running )
    {
        // Get time and mouse position
        t = glfwGetTime();

        // Get window size (may be different than the requested size)
        glfwGetWindowSize( &width, &height );
        height = height > 0 ? height : 1;

        // Set viewport
        glViewport( 0, 0, width, height );

        // Clear color buffer
        glClearColor( cast(GLfloat)(0.5+0.5*sin(3.0*t)), 0.0f, 0.0f, 0.0f);
        glClear( GL_COLOR_BUFFER_BIT );

        // Swap buffers
        glfwSwapBuffers();

        // Check if the window was closed
        running = running && glfwGetWindowParam( GLFW_OPENED );
    }

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}

//========================================================================
// keyfun()
//========================================================================

version(Windows) {
	extern(Windows):
} else {
	extern(C):
}

void keyfun( int key, int action )
{
    if( action != GLFW_PRESS )
    {
        return;
    }

    switch( key )
    {
    case GLFW_KEY_ESC:
        printf( "ESC => quit program\n" );
        running = GL_FALSE;
        break;
    case GLFW_KEY_F1:
    case GLFW_KEY_F2:
    case GLFW_KEY_F3:
    case GLFW_KEY_F4:
    case GLFW_KEY_F5:
    case GLFW_KEY_F6:
    case GLFW_KEY_F7:
    case GLFW_KEY_F8:
    case GLFW_KEY_F9:
    case GLFW_KEY_F10:
    case GLFW_KEY_F11:
    case GLFW_KEY_F12:
    case GLFW_KEY_F13:
    case GLFW_KEY_F14:
    case GLFW_KEY_F15:
    case GLFW_KEY_F16:
    case GLFW_KEY_F17:
    case GLFW_KEY_F18:
    case GLFW_KEY_F19:
    case GLFW_KEY_F20:
    case GLFW_KEY_F21:
    case GLFW_KEY_F22:
    case GLFW_KEY_F23:
    case GLFW_KEY_F24:
    case GLFW_KEY_F25:
        printf( "F%d\n", 1 + key - GLFW_KEY_F1 );
        break;
    case GLFW_KEY_UP:
        printf( "UP\n" );
        break;
    case GLFW_KEY_DOWN:
        printf( "DOWN\n" );
        break;
    case GLFW_KEY_LEFT:
        printf( "LEFT\n" );
        break;
    case GLFW_KEY_RIGHT:
        printf( "RIGHT\n" );
        break;
    case GLFW_KEY_LSHIFT:
        printf( "LSHIFT\n" );
        break;
    case GLFW_KEY_RSHIFT:
        printf( "RSHIFT\n" );
        break;
    case GLFW_KEY_LCTRL:
        printf( "LCTRL\n" );
        break;
    case GLFW_KEY_RCTRL:
        printf( "RCTRL\n" );
        break;
    case GLFW_KEY_LALT:
        printf( "LALT\n" );
        break;
    case GLFW_KEY_RALT:
        printf( "RALT\n" );
        break;
    case GLFW_KEY_TAB:
        printf( "TAB\n" );
        break;
    case GLFW_KEY_ENTER:
        printf( "ENTER\n" );
        break;
    case GLFW_KEY_BACKSPACE:
        printf( "BACKSPACE\n" );
        break;
    case GLFW_KEY_INSERT:
        printf( "INSERT\n" );
        break;
    case GLFW_KEY_DEL:
        printf( "DEL\n" );
        break;
    case GLFW_KEY_PAGEUP:
        printf( "PAGEUP\n" );
        break;
    case GLFW_KEY_PAGEDOWN:
        printf( "PAGEDOWN\n" );
        break;
    case GLFW_KEY_HOME:
        printf( "HOME\n" );
        break;
    case GLFW_KEY_END:
        printf( "END\n" );
        break;
    case GLFW_KEY_KP_0:
        printf( "KEYPAD 0\n" );
        break;
    case GLFW_KEY_KP_1:
        printf( "KEYPAD 1\n" );
        break;
    case GLFW_KEY_KP_2:
        printf( "KEYPAD 2\n" );
        break;
    case GLFW_KEY_KP_3:
        printf( "KEYPAD 3\n" );
        break;
    case GLFW_KEY_KP_4:
        printf( "KEYPAD 4\n" );
        break;
    case GLFW_KEY_KP_5:
        printf( "KEYPAD 5\n" );
        break;
    case GLFW_KEY_KP_6:
        printf( "KEYPAD 6\n" );
        break;
    case GLFW_KEY_KP_7:
        printf( "KEYPAD 7\n" );
        break;
    case GLFW_KEY_KP_8:
        printf( "KEYPAD 8\n" );
        break;
    case GLFW_KEY_KP_9:
        printf( "KEYPAD 9\n" );
        break;
    case GLFW_KEY_KP_DIVIDE:
        printf( "KEYPAD DIVIDE\n" );
        break;
    case GLFW_KEY_KP_MULTIPLY:
        printf( "KEYPAD MULTIPLY\n" );
        break;
    case GLFW_KEY_KP_SUBTRACT:
        printf( "KEYPAD SUBTRACT\n" );
        break;
    case GLFW_KEY_KP_ADD:
        printf( "KEYPAD ADD\n" );
        break;
    case GLFW_KEY_KP_DECIMAL:
        printf( "KEYPAD DECIMAL\n" );
        break;
    case GLFW_KEY_KP_EQUAL:
        printf( "KEYPAD =\n" );
        break;
    case GLFW_KEY_KP_ENTER:
        printf( "KEYPAD ENTER\n" );
        break;
    case GLFW_KEY_SPACE:
        printf( "SPACE\n" );
        break;
    case 'R':
        keyrepeat = (keyrepeat+1) & 1;
        if( keyrepeat )
        {
            glfwEnable( GLFW_KEY_REPEAT );
        }
        else
        {
            glfwDisable( GLFW_KEY_REPEAT );
        }
        printf( "R => Key repeat: %s\n", keyrepeat ? toStringz("ON") : toStringz("OFF") );
        break;
    case 'S':
        systemkeys = (systemkeys+1) & 1;
        if( systemkeys )
        {
            glfwEnable( GLFW_SYSTEM_KEYS );
        }
        else
        {
            glfwDisable( GLFW_SYSTEM_KEYS );
        }
        printf( "S => System keys: %s\n", systemkeys ? "ON" : "OFF" );
        break;
    default:
        if( key > 0 && key < 256 )
        {
            printf( "%c\n", cast(char) key );
        }
        else
        {
            printf( "???\n" );
        }
        break;
    }

    fflush( stdout );
}


