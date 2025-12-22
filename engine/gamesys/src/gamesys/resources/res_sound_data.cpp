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

#include <string.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <sound/sound.h>
#include <resource/resource.h>
#include <resource/resource_chunk_cache.h>
#include "res_sound_data.h"

#include <stdio.h> // printf

namespace dmGameSystem
{
    static const uint32_t SOUNDDATA_DEFAULT_CACHE_SIZE = 2 * 1024*1024;
    static const uint32_t SOUNDDATA_DEFAULT_CHUNK_SIZE = 16 * 1024;

    struct SoundDataContext
    {
        dmMutex::HMutex         m_Mutex;
        dmResource::HFactory    m_Factory;
        HResourceChunkCache     m_Cache;

        // The cache size for all streaming sounds
        uint32_t m_CacheSize;
        // The chunk size when streaming in more data
        uint32_t m_ChunkSize;
    };

    struct SoundDataResource
    {
        dmSound::HSoundData     m_SoundData;
        dmSound::SoundDataType  m_Type;

        // For the streaming
        SoundDataContext*       m_Context;
        const char*             m_Path;
        dmhash_t                m_PathHash;
        uint32_t                m_FileSize:31;
        uint32_t                m_RequestInFlight:1;        // Have we already requested the next chunk?
    };

    static SoundDataContext* g_SoundDataContext = 0;

    dmSound::HSoundData ResSoundDataGetSoundData(SoundDataResource* resource)
    {
        return resource->m_SoundData;
    }

    void ResSoundDataSetStreamingCacheSize(uint32_t cache_size)
    {
        if (!g_SoundDataContext)
            return;
        g_SoundDataContext->m_CacheSize = cache_size;
    }

    // set the size of each streaming chunk size
    void ResSoundDataSetStreamingChunkSize(uint32_t chunk_size)
    {
        if (!g_SoundDataContext)
            return;
        g_SoundDataContext->m_ChunkSize = chunk_size;
    }

    static bool TryToGetTypeFromBuffer(char* buffer, uint32_t buffer_size, dmSound::SoundDataType* out)
    {
        if (buffer_size < 3)
        {
            return false;
        }
        // positions according to format specs (ogg, wav)
        if (buffer[0] == 'O' && buffer[1] == 'g' && buffer[2] == 'g')
        {
            if (buffer_size < 35)
            {
                return false;
            }

            // Make sure Ogg laccing table is only one entry long (should be the case; following offsets depend on it)
            assert(buffer[26] == 1);

            if (buffer[29] == 'v' && buffer[30] == 'o' && buffer[31] == 'r' && buffer[32] == 'b' && buffer[33] == 'i' && buffer[34] == 's')
            {
                *out = dmSound::SOUND_DATA_TYPE_OGG_VORBIS;
                return true;
            }

            if (buffer[28] == 'O' && buffer[29] == 'p' && buffer[30] == 'u' && buffer[31] == 's')
            {
                *out = dmSound::SOUND_DATA_TYPE_OPUS;
                return true;
            }

            return false;
        }
        if (buffer_size < 11)
        {
            return false;
        }
        if (buffer[8] == 'W' && buffer[9] == 'A' && buffer[10] == 'V')
        {
            *out = dmSound::SOUND_DATA_TYPE_WAV;
            return true;
        };
        return false;
    }

    static dmResource::Result DestroyResource(SoundDataResource* resource)
    {
        dmSound::Result r = dmSound::RESULT_OK;

        // The sound data is reference counted
        // The references are held here and also by any playing voices (e.g. like a long playing music track)
        if (resource->m_SoundData)
        {
            dmSound::SetSoundDataCallback(resource->m_SoundData, 0, 0);
            r = dmSound::DeleteSoundData(resource->m_SoundData);
            resource->m_SoundData = 0;
        }

        if (resource->m_Context->m_Cache)
            ResourceChunkCacheEvictPathHash(resource->m_Context->m_Cache, resource->m_PathHash);

        free((void*)resource->m_Path);
        delete resource;

        return dmSound::RESULT_OK == r ? dmResource::RESULT_OK : dmResource::RESULT_INVAL;
    }

    // Find the first chunk that contains the offset
    static ResourceCacheChunk* FindChunk(SoundDataResource* resource, uint32_t offset, ResourceCacheChunk* out)
    {
        bool result = ResourceChunkCacheGet(resource->m_Context->m_Cache, resource->m_PathHash, offset, out);
        return result ? out : 0;
    }

    static void AddChunk(HResourceChunkCache cache, dmhash_t path_hash, uint8_t* data, uint32_t size, uint32_t offset)
    {
        // Offset==0 means the initial chunk of a file (the part that contains the header of a file)
        // We have no way for the decoders to ask for it themselves, so we need to keep it in the cache or
        // the decoder will fail when start playing the sound
        int flags = offset == 0 ? RESOURCE_CHUNK_CACHE_NO_EVICT : RESOURCE_CHUNK_CACHE_DEFAULT;
        if (!ResourceChunkCacheCanFit(cache, size))
        {
            if (!ResourceChunkCacheEvictMemory(cache, size))
            {
                dmLogError("Cannot fit sound data chunk of size %u. Update the sound.stream_cache_size to larger size\n", size);
                return;
            }
        }
        ResourceCacheChunk chunk;
        chunk.m_Data = data,
        chunk.m_Size = size,
        chunk.m_Offset = offset;
        ResourceChunkCachePut(cache, path_hash, flags, &chunk);
    }

    // Called from the main or resource preloader thread
    static int StreamingPreloadCallback(dmResource::HFactory factory, void* cbk_ctx, HResourceDescriptor rd, uint32_t offset, uint32_t nread, uint8_t* buffer)
    {
        SoundDataResource* resource = (SoundDataResource*)cbk_ctx;
        DM_MUTEX_SCOPED_LOCK(resource->m_Context->m_Mutex);

        AddChunk(resource->m_Context->m_Cache, resource->m_PathHash, buffer, nread, offset);
        resource->m_RequestInFlight = 0;
        return 1;
    }

    // Called from the Sound thread
    static dmSound::Result SoundDataReadCallback(void* context, uint32_t offset, uint32_t size, void* _out, uint32_t* out_size)
    {
        SoundDataResource* resource = (SoundDataResource*)context;
        DM_MUTEX_SCOPED_LOCK(resource->m_Context->m_Mutex);

        if (offset >= resource->m_FileSize)
        {
            return dmSound::RESULT_END_OF_STREAM;
        }

        uint8_t* out = (uint8_t*)_out;

        uint32_t nread = 0;
        uint32_t bytes_to_read = size;

        uint32_t request_size   = g_SoundDataContext->m_ChunkSize;
        uint32_t request_offset = uint32_t(offset / request_size) * request_size;

        ResourceCacheChunk tmp = {0};
        ResourceCacheChunk* chunk = FindChunk(resource, offset, &tmp);
        while (chunk)
        {
            uint32_t chunk_size = chunk->m_Size;
            uint32_t chunk_offset = chunk->m_Offset;

            // We found a chunk and can start copying
            uint32_t chunk_internal_offset = offset - chunk_offset; // the index to start from within this chunk
            uint32_t chunk_remaining = chunk_size - chunk_internal_offset;
            uint32_t to_read = dmMath::Min(bytes_to_read, chunk_remaining);
            memcpy(out + nread, &chunk->m_Data[chunk_internal_offset], to_read);

            bytes_to_read   -= to_read;
            nread           += to_read;
            offset          += to_read;

            if (bytes_to_read > 0)
                chunk = FindChunk(resource, offset, &tmp);
            else
                chunk = 0;
        }

        uint32_t next_chunk_offset = uint32_t(offset / request_size) * request_size;
        if (next_chunk_offset == request_offset)
            next_chunk_offset += request_size;

        if (next_chunk_offset >= resource->m_FileSize) // Are we wrapping around?
        {
            next_chunk_offset = 0;
            request_offset = 0;
        }

        ResourceCacheChunk* next_chunk = FindChunk(resource, next_chunk_offset, &tmp);
        if (next_chunk && next_chunk->m_Offset != next_chunk_offset)
        {
            next_chunk = 0; // we're actually missing the next chunk. Let's trigger a pre fetch
        }

        *out_size = nread;

        // Last resort:
        //  If we didn't have any chunks that matched
        //  This will cause a delay
        if (nread == 0) // we failed to read from the _current_ offset
        {
            // Round down to nearest chunk offset
            request_offset = uint32_t(offset / request_size) * request_size;
        }
        else if (next_chunk == 0)
        {
            request_offset = next_chunk_offset;
        }

        dmSound::Result result = dmSound::RESULT_PARTIAL_DATA;
        if (offset >= resource->m_FileSize && nread == 0)
        {
            // we cannot return any data as we reached the end of the resource
            result = dmSound::RESULT_END_OF_STREAM;
        }

        if (next_chunk == 0 && !resource->m_RequestInFlight)
        {
            resource->m_RequestInFlight = 1;
            dmResource::PreloadData(resource->m_Context->m_Factory, resource->m_Path, request_offset, request_size, StreamingPreloadCallback, (void*)resource);
        }

        return result;
    }

    dmResource::Result ResSoundDataCreate(const dmResource::ResourceCreateParams* params)
    {
        dmSound::SoundDataType type;
        bool type_result = TryToGetTypeFromBuffer((char*)params->m_Buffer, params->m_BufferSize, &type);
        if (!type_result)
        {
            dmLogError("Failed to detect sound type for: %s", params->m_Filename);
            return dmResource::RESULT_INVALID_DATA;
        }

        SoundDataContext* context = (SoundDataContext*)ResourceTypeGetContext(params->m_Type);
        // Until we have a way to get the factory at the time of type creation
        if (!context->m_Factory)
        {
            context->m_Factory = params->m_Factory;
        }

        SoundDataResource* sound_data_res = new SoundDataResource();
        memset(sound_data_res, 0, sizeof(*sound_data_res));
        sound_data_res->m_Path = strdup(params->m_Filename);
        sound_data_res->m_PathHash = ResourceDescriptorGetNameHash(params->m_Resource);
        sound_data_res->m_FileSize = params->m_FileSize;
        sound_data_res->m_RequestInFlight = 0;
        sound_data_res->m_Context = context;

        dmSound::HSoundData sound_data = 0;
        dmSound::Result r;
        if (params->m_IsBufferPartial)
        {
            if (!context->m_Cache)
            {
                context->m_Cache = ResourceChunkCacheCreate(context->m_CacheSize);
            }

            AddChunk(context->m_Cache, sound_data_res->m_PathHash, (uint8_t*)params->m_Buffer, params->m_BufferSize, 0);

            r = dmSound::NewSoundDataStreaming(SoundDataReadCallback, sound_data_res, type, &sound_data, dmResource::GetNameHash(params->m_Resource));
        }
        else
        {
            r = dmSound::NewSoundData(params->m_Buffer, params->m_BufferSize, type, &sound_data, dmResource::GetNameHash(params->m_Resource));
        }

        if (r != dmSound::RESULT_OK)
        {
            DestroyResource(sound_data_res);
            return dmResource::RESULT_OUT_OF_RESOURCES;
        }

        sound_data_res->m_SoundData = sound_data;
        sound_data_res->m_Type = type;

        dmResource::SetResource(params->m_Resource, sound_data_res);
        if (params->m_IsBufferPartial)
            dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);
        else
            dmResource::SetResourceSize(params->m_Resource, dmSound::GetSoundResourceSize(sound_data));
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundDataDestroy(const dmResource::ResourceDestroyParams* params)
    {
        return DestroyResource((SoundDataResource*) dmResource::GetResource(params->m_Resource));
    }

    dmResource::Result ResSoundDataRecreate(const dmResource::ResourceRecreateParams* params)
    {
        SoundDataResource* sound_data_res = (SoundDataResource*) dmResource::GetResource(params->m_Resource);

        dmSound::SoundDataType type;
        bool type_result = TryToGetTypeFromBuffer((char*)params->m_Buffer, params->m_BufferSize, &type);
        if (!type_result)
        {
            dmLogError("Failed to detect sound type for: %s", params->m_Filename);
            return dmResource::RESULT_INVALID_DATA;
        }

        dmSound::HSoundData sound_data;
        dmSound::Result r = dmSound::NewSoundData(params->m_Buffer, params->m_BufferSize, type, &sound_data, dmResource::GetNameHash(params->m_Resource));

        if (r != dmSound::RESULT_OK)
        {
            return dmResource::RESULT_OUT_OF_RESOURCES;
        }

        dmSound::HSoundData old_sound_data = sound_data_res->m_SoundData;
        dmSound::DeleteSoundData(old_sound_data);

        sound_data_res->m_SoundData = sound_data;

        dmResource::SetResource(params->m_Resource, sound_data_res);
        dmResource::SetResourceSize(params->m_Resource, dmSound::GetSoundResourceSize(sound_data));
        return dmResource::RESULT_OK;
    }

    static ResourceResult ResourceTypeSoundData_Register(HResourceTypeContext ctx, HResourceType type)
    {
        // This function is called once for each file type
        // But we only want to create one global chunk cache
        if (g_SoundDataContext == 0)
        {
            SoundDataContext* context = new SoundDataContext;
            memset(context, 0, sizeof(*context));
            context->m_Mutex     = dmMutex::New();
            context->m_CacheSize = SOUNDDATA_DEFAULT_CACHE_SIZE;
            context->m_ChunkSize = SOUNDDATA_DEFAULT_CHUNK_SIZE;
            context->m_Cache     = 0;

            g_SoundDataContext = context;
        }
        SoundDataContext* context = g_SoundDataContext;

        return (ResourceResult)dmResource::SetupType(ctx,
                                                       type,
                                                       context,
                                                       0, // preload
                                                       ResSoundDataCreate,
                                                       0, // post create
                                                       ResSoundDataDestroy,
                                                       ResSoundDataRecreate);
    }

    static ResourceResult ResourceTypeSoundData_Deregister(HResourceTypeContext ctx, HResourceType type)
    {
        if (g_SoundDataContext)
        {
            if (g_SoundDataContext->m_Mutex)
                dmMutex::Delete(g_SoundDataContext->m_Mutex);

            if (g_SoundDataContext->m_Cache)
                ResourceChunkCacheDestroy(g_SoundDataContext->m_Cache);

            delete g_SoundDataContext;
        }
        g_SoundDataContext = 0;
        return RESOURCE_RESULT_OK;
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeOgg, "oggc", dmGameSystem::ResourceTypeSoundData_Register, dmGameSystem::ResourceTypeSoundData_Deregister);
DM_DECLARE_RESOURCE_TYPE(ResourceTypeOpus, "opusc", dmGameSystem::ResourceTypeSoundData_Register, dmGameSystem::ResourceTypeSoundData_Deregister);
DM_DECLARE_RESOURCE_TYPE(ResourceTypeWav, "wavc", dmGameSystem::ResourceTypeSoundData_Register, dmGameSystem::ResourceTypeSoundData_Deregister);
