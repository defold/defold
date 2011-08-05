//========================================================================
// This is a small test application for GLFW.
// The program prints "Hello world!", using two threads.
//========================================================================

import glfw;


//========================================================================
// main() - Main function (main thread)
//========================================================================

int main( )
{
    GLFWthread thread;

    // Initialise GLFW
    if( !glfwInit() )
    {
        return 0;
    }

    // Create thread
    thread = glfwCreateThread( &HelloFun, null );

    // Wait for thread to die
    glfwWaitThread( thread, GLFW_WAIT );

    // Print the rest of the message
    printf( "world!\n" );

    // Terminate GLFW
    glfwTerminate();

    return 0;
}

version(Windows) {
	extern(Windows):
} else {
	extern(C):
}

//========================================================================
// HelloFun() - Thread function
//========================================================================

void HelloFun( void *arg )
{
    // Print the first part of the message
    printf( "Hello " );
}


