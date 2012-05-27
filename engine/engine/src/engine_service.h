#ifndef DM_ENGINE_SERVICE
#define DM_ENGINE_SERVICE

namespace dmEngineService
{
    typedef struct EngineService* HEngineService;

    HEngineService New(uint16_t port);
    void Delete(HEngineService engine_service);
    void Update(HEngineService engine_service);
    uint16_t GetPort(HEngineService engine_service);

}

#endif // DM_ENGINE_SERVICE
