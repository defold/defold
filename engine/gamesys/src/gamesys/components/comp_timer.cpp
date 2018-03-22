#include "comp_timer.h"
#include <dlib/profile.h>

namespace dmGameSystem
{

//    typedef void (*TimerTrigger)(/*dmGameObject::HInstance instance, dmhash_t component_id, */ uint32_t timer_id, void* userdata1, void* userdata2);

    #define INVALID_ID 0xfffffffful
    #define MAX_CAPACITY 65000u
    #define MIN_CAPACITY_GROWTH 2048u

    struct Timer
    {
        // How much time remaining until the timer fires, reduced with dt at each update
        // If the result of removing dt from remaining is <= 0.f then fire the event, if repeat then add intervall to remaining
        // If <= 0.0f then the timer is dead
        float           m_Remaining;

        TimerTrigger    m_Trigger;
        uintptr_t       m_Userdata1;
        uintptr_t       m_Userdata2;

        // We chain together timers associated with the same userdata1 so we can quickly remove all of them without scanning all timers
        uint32_t        m_PrevIdWithSameUserdata1;
        uint32_t        m_NextIdWithSameUserdata1;

        uint32_t        m_Id;

        // The timer intervall, we need to keep this for repeating timers, not really for one-shots
        float           m_Intervall;

        // Flag if the timer should repeat
        uint32_t        m_Repeat : 1;
    };

    // One TimerWorld per Engine?
    struct TimerWorld
    {
        dmArray<Timer>                      m_Timers;
        dmHashTable<uint32_t, uint16_t>     m_IdToIndexMap;
        dmHashTable<uintptr_t, uint32_t>    m_Userdata1ToFirstId;
        uint32_t                            m_IdGenerator;
        uint32_t                            m_InUpdate : 1;
    };

    void FreeTimer(TimerWorld* world, uint16_t index);

    dmGameObject::CreateResult CompTimerNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        if (params.m_World != 0x0)
        {
            TimerWorld* world = new TimerWorld();
            *params.m_World = world;
            const uint32_t timer_count = 256;
            world->m_Timers.SetCapacity(timer_count);
            const int32_t instance_count = params.m_MaxInstances;
            const uint32_t table_count = dmMath::Max(1, instance_count/3);
            world->m_IdToIndexMap.SetCapacity(table_count, instance_count);
            world->m_Userdata1ToFirstId.SetCapacity(table_count, instance_count);
            world->m_IdGenerator = 0;
            world->m_InUpdate = 0;
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompTimerDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        if (params.m_World != 0x0)
        {
            TimerWorld* timerWorld = (TimerWorld*)params.m_World;
            delete timerWorld;
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompTimerCreate(const dmGameObject::ComponentCreateParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTimerDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompTimerOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult UpdateTimers(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        DM_PROFILE(Timer, "Update");

        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;

        TimerWorld* world = (TimerWorld*)params.m_World;

        float dt = params.m_UpdateContext->m_DT;

        world->m_InUpdate = 1;
        uint32_t size = world->m_Timers.Size();
        DM_COUNTER("timerc", size);
        uint32_t i = 0;
        while (i < size)
        {
            Timer& timer = world->m_Timers[i];
            if (timer.m_Remaining > 0.0f)
            {
                timer.m_Remaining -= dt;
                if (timer.m_Remaining <= 0.0f)
                {
                    timer.m_Trigger(timer.m_Id, (void*)timer.m_Userdata1, (void*)timer.m_Userdata2);

                    // New timers may be added during the trigger
                    size = world->m_Timers.Size();
                }
            }
            ++i;
        }

        size = world->m_Timers.Size();
        i = 0;
        while (i < size)
        {
            Timer& timer = world->m_Timers[i];
            if (timer.m_Remaining <= 0.0f)
            {
                if (timer.m_Repeat)
                {
                    timer.m_Remaining += timer.m_Intervall;
                    ++i;
                }
                else
                {
                    FreeTimer(world, i);
                    --size;
                }
            }
            else
            {
                ++i;
            }
        }

        world->m_InUpdate = 0;

        return result;
    }

    static Timer* AllocateTimer(TimerWorld* world, uintptr_t userdata1)
    {
        uint32_t triggerCount = world->m_Timers.Size();
        if (triggerCount == MAX_CAPACITY)
        {
            dmLogError("Timer could not be stored since the buffer is full (%d).", MAX_CAPACITY);
            return 0x0;
        }

        if (world->m_IdGenerator == INVALID_ID)
        {
            dmLogError("Timer could not be created since the generator exceeded its capacity (%lu).", INVALID_ID);
            return 0x0;
        }

        uint32_t* id_ptr = world->m_Userdata1ToFirstId.Get(userdata1);
        if (id_ptr == 0x0)
        {
            if (world->m_Userdata1ToFirstId.Full())
            {
                dmLogError("Timer could not be stored since the instance lookup buffer is full (%d).", world->m_Userdata1ToFirstId.Size());
                return 0x0;
            }
        }

        uint32_t id = world->m_IdGenerator++;

        if (world->m_Timers.Full())
        {
            // Growth heuristic is to grow with the mean of MIN_CAPACITY_GROWTH and half current capacity, and at least MIN_CAPACITY_GROWTH
            uint32_t capacity = world->m_Timers.Capacity();
            uint32_t growth = dmMath::Min(MIN_CAPACITY_GROWTH, (MIN_CAPACITY_GROWTH + capacity / 2) / 2);
            capacity = dmMath::Min(capacity + growth, MAX_CAPACITY);
            world->m_Timers.SetCapacity(capacity);
        }

        world->m_Timers.SetSize(triggerCount + 1);
        Timer& timer = world->m_Timers[triggerCount + 1];
        timer.m_Id = id;

        timer.m_PrevIdWithSameUserdata1 = INVALID_ID;
        if (id_ptr != 0x0)
        {
            Timer& nextTimer = world->m_Timers[*world->m_IdToIndexMap.Get(*id_ptr)];
            nextTimer.m_PrevIdWithSameUserdata1 = id;
            timer.m_NextIdWithSameUserdata1 = nextTimer.m_Id;
        }
        else
        {
            timer.m_NextIdWithSameUserdata1 = INVALID_ID;
        }

        world->m_Userdata1ToFirstId.Put(userdata1, id);
        return &timer;
    }

    void FreeTimer(TimerWorld* world, uint16_t index)
    {
        Timer* timer = &world->m_Timers[index];
        world->m_IdToIndexMap.Erase(timer->m_Id);

        uint32_t previousId = timer->m_PrevIdWithSameUserdata1;
        uint32_t nextId = timer->m_NextIdWithSameUserdata1;
        if (INVALID_ID != nextId)
        {
            world->m_Timers[*world->m_IdToIndexMap.Get(nextId)].m_PrevIdWithSameUserdata1 = previousId;
        }

        if (INVALID_ID != previousId)
        {
            world->m_Timers[*world->m_IdToIndexMap.Get(previousId)].m_NextIdWithSameUserdata1 = nextId;
        }
        else
        {
            if (INVALID_ID == nextId)
            {
                world->m_Userdata1ToFirstId.Erase(timer->m_Userdata1);
            }
            else
            {
                world->m_Userdata1ToFirstId.Put(timer->m_Userdata1, nextId);
            }
        }

    //    world->m_IdToIndexMap.Erase(timer->m_Id);
    //    timer->m_Id = INVALID_ID;
    //    timer->m_NextIdWithSameUserdata1 = INVALID_ID;
    //    timer->m_PrevIdWithSameUserdata1 = INVALID_ID;
    //    timer->m_Userdata1 = 0x0;

        Timer& movedTimer = world->m_Timers.EraseSwap(index);

        if (world->m_Timers.Size() > 0)
        {
            world->m_IdToIndexMap.Put(movedTimer.m_Id, index);
        }
    }

    uint32_t AddTimer(TimerWorld* world,
                        float delay,
                        TimerTrigger timer_trigger,
                        void* userdata1,
                        void* userdata2,
                        bool repeat)
    {
        Timer* timer = AllocateTimer(world, (uintptr_t)userdata1);
        if (timer == 0x0)
        {
            return INVALID_ID;
        }

        timer->m_Userdata2 = (uintptr_t)userdata2;
        timer->m_Intervall = delay;
        timer->m_Remaining = delay;
        timer->m_Repeat = repeat;

        return timer->m_Id;
    }

    bool CancelTimer(TimerWorld* world, uint32_t id)
    {
        uint16_t* index_ptr = world->m_IdToIndexMap.Get(id);
        if (index_ptr == 0x0)
        {
            // Dead timer
            return false;
        }
        if (world->m_InUpdate)
        {
            Timer& timer = world->m_Timers[*index_ptr];
            timer.m_Remaining = 0.0f;
            timer.m_Repeat = 0;
        }
        else
        {
            FreeTimer(world, *index_ptr);
        }
        return true;
    }

    void CancelTimers(TimerWorld* world, uintptr_t userdata1)
    {
        uint32_t* id_ptr = world->m_Userdata1ToFirstId.Get(userdata1);
        if (id_ptr == 0x0)
            return;

        uint32_t id = *id_ptr;
        do
        {
            uint16_t index = *world->m_IdToIndexMap.Get(id);
            Timer& timer = world->m_Timers[index];

            world->m_IdToIndexMap.Erase(id);
            id = timer.m_NextIdWithSameUserdata1;

            if (world->m_InUpdate)
            {
                timer.m_Remaining = 0.0f;
                timer.m_Repeat = 0;
            }
            else
            {
    //            timer.m_Id = INVALID_ID;
    //            timer.m_NextIdWithSameUserdata1 = INVALID_ID;
    //            timer.m_PrevIdWithSameUserdata1 = INVALID_ID;
    //            timer.m_Userdata1 = 0x0;

                Timer& movedTimer = world->m_Timers.EraseSwap(index);

                if (world->m_Timers.Size() > 0)
                {
                    world->m_IdToIndexMap.Put(movedTimer.m_Id, index);
                }
            }
        } while (id != INVALID_ID);

        world->m_Userdata1ToFirstId.Erase(userdata1);
    }


    uint32_t AddTimer(void* world,
                            float delay,
                            TimerTrigger timer_trigger,
                            void* userdata1,
                            void* userdata2,
                            bool repeat)
    {
        return AddTimer((TimerWorld*)world, delay, timer_trigger, userdata1, userdata2, repeat);
    }

    bool CancelTimer(void* world, uint32_t id)
    {
        return CancelTimer((TimerWorld*)world, id);
    }

    void CancelTimers(void* world, uintptr_t userdata1)
    {
        CancelTimers((TimerWorld*)world, userdata1);
    }
}
