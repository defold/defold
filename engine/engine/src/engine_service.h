#ifndef DM_ENGINE_SERVICE
#define DM_ENGINE_SERVICE

#include <stdint.h>

namespace dmEngineService
{
    typedef struct EngineService* HEngineService;

    // Parses the environment flag DM_SERVICE_PORT=xxxx for a port, or returns the default port
    uint16_t GetServicePort(uint16_t default_port);

    HEngineService New(uint16_t port);
    void Delete(HEngineService engine_service);
    void Update(HEngineService engine_service);
    uint16_t GetPort(HEngineService engine_service);

}

#endif // DM_ENGINE_SERVICE
