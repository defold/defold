#include "comp_model.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <sound/sound.h>

#include "gamesys_ddf.h"
#include "../gamesys_private.h"

namespace dmGameSystem
{
    struct PlayEntry
    {
        dmSound::HSoundInstance m_Instance;
        float                   m_Delay;
    };

    struct World
    {
        dmArray<PlayEntry>  m_Entries;
        dmIndexPool32       m_EntryIndices;
    };

    dmGameObject::CreateResult CompSoundNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        World* world = new World();
        const uint32_t MAX_INSTANCE_COUNT = 32;
        world->m_Entries.SetCapacity(MAX_INSTANCE_COUNT);
        world->m_Entries.SetSize(MAX_INSTANCE_COUNT);
        world->m_EntryIndices.SetCapacity(MAX_INSTANCE_COUNT);
        memset(&world->m_Entries.Front(), 0, MAX_INSTANCE_COUNT * sizeof(PlayEntry));
        *params.m_World = (void*)world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        World* world = (World*)params.m_World;
        uint32_t size = world->m_Entries.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            if (world->m_Entries[i].m_Instance != 0)
            {
                dmSound::DeleteSoundInstance(world->m_Entries[i].m_Instance);
            }
        }
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundCreate(const dmGameObject::ComponentCreateParams& params)
    {
        *params.m_UserData = (uintptr_t) params.m_Resource;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        World* world = (World*)params.m_World;
        for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (entry.m_Instance != 0)
            {
                float prev_delay = entry.m_Delay;
                entry.m_Delay -= params.m_UpdateContext->m_DT;
                if (entry.m_Delay < 0.0f)
                {
                    if (prev_delay >= 0.0f)
                    {
                        dmSound::Result r = dmSound::Play(entry.m_Instance);
                        if (r != dmSound::RESULT_OK)
                        {
                            dmLogError("Error playing sound: (%d)", r);
                        }
                    }
                    else if (!dmSound::IsPlaying(entry.m_Instance))
                    {
                        dmSound::Result r = dmSound::DeleteSoundInstance(entry.m_Instance);
                        if (r != dmSound::RESULT_OK)
                        {
                            dmLogError("Error deleting sound: (%d)", r);
                            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                        }
                        entry.m_Instance = 0;
                        world->m_EntryIndices.Push(i);
                    }
                }
            }
        }
        dmSound::Update();
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::PlaySound::m_DDFDescriptor)
        {
            World* world = (World*)params.m_World;
            if (world->m_EntryIndices.Remaining() > 0)
            {
                dmGameSystemDDF::PlaySound* play_sound = (dmGameSystemDDF::PlaySound*)params.m_Message->m_Data;
                dmSound::HSoundData sound_data = (dmSound::HSoundData)*params.m_UserData;
                uint32_t index = world->m_EntryIndices.Pop();
                PlayEntry& entry = world->m_Entries[index];
                entry.m_Delay = play_sound->m_Delay;
                dmSound::Result result = dmSound::NewSoundInstance(sound_data, &entry.m_Instance);
                if (result != dmSound::RESULT_OK)
                {
                    LogMessageError(params.m_Message, "A sound could not be played, error: %d.", result);
                }
            }
            else
            {
                LogMessageError(params.m_Message, "A sound could not be played since the sound buffer is full (%d).", world->m_EntryIndices.Capacity());
            }
        }
        else
        {
            const char* id_str = (const char*) dmHashReverse64(params.m_Message->m_Id, 0);
            LogMessageError(params.m_Message, "Unsupported sound message '%s'.", id_str);
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }
}
