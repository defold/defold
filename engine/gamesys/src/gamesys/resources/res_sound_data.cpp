// Copyright 2020-2024 The Defold Foundation
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
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <sound/sound.h>
#include <resource/resource.h>
#include "res_sound_data.h"

#include <stdio.h> // printf

namespace dmGameSystem
{
    struct SoundDataContext
    {
        dmMutex::HMutex m_Mutex;
    };

    struct SoundDataChunk
    {
        uint8_t* m_Data;
        uint32_t m_Size;
        uint32_t m_Offset; // Offset into the file
    };


    static dmSound::SoundDataType TryToGetTypeFromBuffer(char* buffer, dmSound::SoundDataType default_type, uint32_t bufferSize)
    {
        dmSound::SoundDataType type = default_type;
        if (bufferSize < 3)
        {
            return type;
        }
        // positions according to format specs (ogg, wav)
        if (buffer[0] == 'O' && buffer[1] == 'g' && buffer[2] == 'g')
        {
            type = dmSound::SOUND_DATA_TYPE_OGG_VORBIS;
        }
        if (bufferSize < 11)
        {
            return type;
        }
        if (buffer[8] == 'W' && buffer[9] == 'A' && buffer[10] == 'V')
        {
            type = dmSound::SOUND_DATA_TYPE_WAV;
        };
        return type;
    }

    static dmResource::Result DestroyResource(SoundDataResource* resource)
    {
        // The sound data is reference counted
        // The references are held here and also by any playing voices (e.g. like a long playing music track)
        dmSound::SetSoundDataCallback(resource->m_SoundData, 0, 0);
        dmSound::Result r = dmSound::DeleteSoundData(resource->m_SoundData);

        free((void*)resource->m_Path);
        delete resource;

        return dmSound::RESULT_OK == r ? dmResource::RESULT_OK : dmResource::RESULT_INVAL;
    }

    static SoundDataChunk* AllocChunk(uint8_t* data, uint32_t data_size, uint32_t offset)
    {
        SoundDataChunk* chunk = new SoundDataChunk;
        chunk->m_Size   = data_size;
        chunk->m_Offset = offset;
        chunk->m_Data = new uint8_t[data_size];
        memcpy(chunk->m_Data, data, data_size);
        return chunk;
    }

    static void FreeChunk(SoundDataChunk* chunk)
    {
        delete chunk->m_Data;
        delete chunk;
    }

    static void DebugChunks(SoundDataResource* resource)
    {
        uint32_t num_chunks = resource->m_Chunks.Size();
        printf("NUM CHUNKS: %u\n", num_chunks);
        for (uint32_t i = 0; i < num_chunks; ++i)
        {
            SoundDataChunk* chunk = resource->m_Chunks[i];
            assert(chunk != 0);
            printf("  chunk %u: offset: %u  size: %u\n", i, chunk->m_Offset, chunk->m_Size);
        }
    }

    static void InsertChunk(SoundDataResource* resource, SoundDataChunk* chunk)
    {
        uint32_t offset = chunk->m_Offset;
        uint32_t num_chunks = resource->m_Chunks.Size();

        if (resource->m_Chunks.Full())
            resource->m_Chunks.OffsetCapacity(2);

        //printf("INSERT chunk: offset: %u  size: %u\n", chunk->m_Offset, chunk->m_Size);

        for (uint32_t i = 0; i < num_chunks; ++i)
        {
            SoundDataChunk* c = resource->m_Chunks[i];
            printf("   c %u: offset: %u  size: %u\n", i, c->m_Offset, c->m_Size);
            if (offset < c->m_Offset)
            {
                // Insert the chunk into the array
                memmove(resource->m_Chunks.Begin()+i, resource->m_Chunks.Begin()+i+1, (num_chunks - i) * sizeof(SoundDataChunk*));
                resource->m_Chunks[i] = chunk;
                resource->m_Chunks.SetSize(num_chunks+1);
                return;
            }
        }

        // it was the first entry
        resource->m_Chunks.Push(chunk);
    }

    // Called from the main or resource preloader thread
    static int StreamingPreloadCallback(dmResource::HFactory factory, void* cbk_ctx, HResourceDescriptor rd, uint32_t offset, uint32_t nread, uint8_t* buffer)
    {
        SoundDataResource* resource = (SoundDataResource*)cbk_ctx;
        DM_MUTEX_SCOPED_LOCK(resource->m_Context->m_Mutex);

        //printf("%s: offset: %u  size: %u  %s\n", __FUNCTION__, offset, nread, resource->m_Path);

        SoundDataChunk* chunk = AllocChunk(buffer, nread, offset);
        InsertChunk(resource, chunk);
        //DebugChunks(resource);
        resource->m_RequestInFlight = 0;
        return 1;
    }

    // Called from the Sound thread
    static dmSound::Result SoundDataReadCallback(void* context, uint32_t offset, uint32_t size, void* _out, uint32_t* out_size)
    {
        SoundDataResource* resource = (SoundDataResource*)context;
        DM_MUTEX_SCOPED_LOCK(resource->m_Context->m_Mutex);

        //printf("%s: offset: %u  size: %u  filesize: %u\n", __FUNCTION__, offset, size, resource->m_FileSize);

        if (offset >= resource->m_FileSize)
        {
            return dmSound::RESULT_END_OF_STREAM;
        }

        uint8_t* out = (uint8_t*)_out;

        uint32_t nread = 0;
        uint32_t bytes_to_read = size;

        bool missing_chunk = false;

        uint32_t request_offset = 0;
        uint32_t request_size   = size; // TODO: Are we guaranteed this is a fixed size? Does it have to be?
        if (request_size < 16*1024) // TODO: make this number configurable
            request_size = 16*1024;

        uint32_t num_chunks = resource->m_Chunks.Size();
        for (uint32_t i = 0; i < num_chunks; ++i)
        {
            SoundDataChunk* chunk = resource->m_Chunks[i];
            assert(chunk != 0);
            uint32_t chunk_size = chunk->m_Size;
            uint32_t chunk_offset = chunk->m_Offset;

            if (offset < chunk_offset)
            {
                // We currently don't have the chunk in question, and need to request it
                missing_chunk = true;
                request_offset = offset;
                break;
            }

            if (offset >= (chunk_offset + chunk_size))
            {
                // We need a later chunk

                if (nread > 0)
                {
                    // We've already started reading, but are apparently missing a previous chunk!
                    missing_chunk = true;
                    request_offset = offset;
                    break;
                }

                continue;
            }

            // We break if we have nothing more to read
            // We do it here as the code above will detect if the next chunk is already in place
            // or if we need to request it.
            if (bytes_to_read == 0)
                break; // the next chunk is already loaded

            // We are within a chunk and can start copying
            uint32_t chunk_internal_offset = offset - chunk_offset; // the index to start from within this chunk
            uint32_t chunk_remaining = chunk_size - chunk_internal_offset;
            uint32_t to_read = dmMath::Min(size, chunk_remaining);
            memcpy(out + nread, &chunk->m_Data[chunk_internal_offset], to_read);

            bytes_to_read   -= to_read;
            nread           += to_read;
            offset          += to_read;
        }

        *out_size = nread;

        // Last resort:
        //  If we didn't have any chunks that matched
        //  This will cause a delay
        if (nread == 0)
        {
            missing_chunk = true;
            request_offset = offset;
        }

        //printf("  nread: %u  missing_chunk: %d  in flight: %u\n", nread, missing_chunk, resource->m_RequestInFlight);

        if (missing_chunk && !resource->m_RequestInFlight)
        {
            resource->m_RequestInFlight = 1;

        //printf("MAWE   request: offset: %u  size: %u\n", request_offset, request_size);

            dmResource::PreloadData(resource->m_Factory, resource->m_Path, request_offset, request_size, StreamingPreloadCallback, (void*)resource);
        }

        return dmSound::RESULT_PARTIAL_DATA;
    }

    dmResource::Result ResSoundDataCreate(const dmResource::ResourceCreateParams* params)
    {
        dmSound::HSoundData sound_data;

        dmSound::SoundDataType type = dmSound::SOUND_DATA_TYPE_WAV;

        size_t filename_len = strlen(params->m_Filename);
        if (filename_len > 5 && strcmp(params->m_Filename + filename_len - 5, ".oggc") == 0)
        {
            type = dmSound::SOUND_DATA_TYPE_OGG_VORBIS;
        }

        SoundDataResource* sound_data_res = new SoundDataResource();
        sound_data_res->m_Path = strdup(params->m_Filename);
        sound_data_res->m_FileSize = params->m_FileSize;
        sound_data_res->m_Factory = params->m_Factory;
        sound_data_res->m_RequestInFlight = 0;
        sound_data_res->m_Context = (SoundDataContext*)ResourceTypeGetContext(params->m_Type);

//printf("MAWE: sound: '%s'  partial: %d  initial: %u %p\n", params->m_Filename, params->m_IsBufferPartial, params->m_BufferSize, sound_data_res);

        dmSound::Result r;
        if (params->m_IsBufferPartial)
        {
            SoundDataChunk* chunk = AllocChunk((uint8_t*)params->m_Buffer, params->m_BufferSize, 0);
            InsertChunk(sound_data_res, chunk);
            //DebugChunks(sound_data_res);

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
        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundDataDestroy(const dmResource::ResourceDestroyParams* params)
    {
        return DestroyResource((SoundDataResource*) dmResource::GetResource(params->m_Resource));
    }

    dmResource::Result ResSoundDataRecreate(const dmResource::ResourceRecreateParams* params)
    {
        SoundDataResource* sound_data_res = (SoundDataResource*) dmResource::GetResource(params->m_Resource);

        dmSound::HSoundData sound_data;
        dmSound::SoundDataType type = TryToGetTypeFromBuffer((char*)params->m_Buffer, (dmSound::SoundDataType)sound_data_res->m_Type, params->m_BufferSize);
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
        SoundDataContext* context = new SoundDataContext;
        context->m_Mutex = dmMutex::New();
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
        SoundDataContext* context = (SoundDataContext*)ResourceTypeGetContext(type);
        if (context->m_Mutex)
            dmMutex::Delete(context->m_Mutex);
        delete context;
        return RESOURCE_RESULT_OK;
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeOgg, "oggc", dmGameSystem::ResourceTypeSoundData_Register, dmGameSystem::ResourceTypeSoundData_Deregister);
DM_DECLARE_RESOURCE_TYPE(ResourceTypeWav, "wavc", dmGameSystem::ResourceTypeSoundData_Register, dmGameSystem::ResourceTypeSoundData_Deregister);
