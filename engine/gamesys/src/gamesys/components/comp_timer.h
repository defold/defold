#ifndef DM_GAMESYS_COMP_TIMER_H
#define DM_GAMESYS_COMP_TIMER_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompTimerNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTimerDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTimerCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTimerDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompTimerOnMessage(const dmGameObject::ComponentOnMessageParams& params);


// What is the context here "world" - collection?

typedef void (*TimerTrigger)(uint32_t timer_id, void* userdata1, void* userdata2);

uint32_t AddTimer(void* world,
                        float delay,
                        TimerTrigger timer_trigger,
                        void* userdata1,
                        void* userdata2,
                        bool repeat);
bool CancelTimer(void* world, uint32_t id);
void CancelTimers(void* world, uintptr_t userdata1);

}

#endif // DM_GAMESYS_COMP_TIMER_H
