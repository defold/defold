// stubs for unresolved libraries on Switch
#include <assert.h>
extern "C" int pipe(int pipefd[2]) { assert(false && "Defold stub function"); return 0; }
typedef int pid_t;
extern "C" pid_t fork(void) { assert(false && "Defold stub function"); return 0; }
extern "C" pid_t waitpid(pid_t pid, int *status, int options) { assert(false && "Defold stub function"); return 0; }
extern "C" void _exit(int status) { assert(false && "Defold stub function"); }
extern "C" int execvp(const char *file, char *const argv[]) { assert(false && "Defold stub function"); return 0; }
extern "C" int execv(const char *path, char *const argv[]) { assert(false && "Defold stub function"); return 0; }
typedef void (*sighandler_t)(int);
extern "C" sighandler_t signal(int signum, sighandler_t handler) { assert(false && "Defold stub function"); sighandler_t v; return v; }
