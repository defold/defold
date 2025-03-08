// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
