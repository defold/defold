#ifndef DM_ENGINE_H
#define DM_ENGINE_H

#include <app/app.h>
#include <stdint.h>

namespace dmEngine
{
    typedef struct Engine* HEngine;

    uint16_t GetHttpPort(HEngine engine);
    uint32_t GetFrameCount(HEngine engine);
};

// For better granularity
dmEngine::HEngine   dmEngineCreate(int argc, char *argv[]);
void                dmEngineDestroy(dmEngine::HEngine engine);

/*
 * Updates the engine one tick
 *   Returns:
 *   0: Valid for more updates
 * !=0: Engine want to exit or reboot
 */
dmApp::Result      dmEngineUpdate(dmEngine::HEngine engine);

/* Gets the result from the engine
 *      run_action: 0 == update, 1 == reboot, -1 == exit
 *      exit_code:  the app exit code
 *      argc:       the number of arguments in the argv list (only set in case of the reboot)
 *      argv:       the reboot argument list. use free(argv[i]) for each argument after usage, and also free(argv)
 */
void                dmEngineGetResult(dmEngine::HEngine engine, int* run_action, int* exit_code, int* argc, char*** argv);

#endif // DM_ENGINE_H
