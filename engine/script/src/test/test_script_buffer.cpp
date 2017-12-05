#include <gtest/gtest.h>

#include <stdio.h>
#include <stdint.h>

#include "script.h"
#include "script_buffer.h"

#include <dlib/buffer.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

class ScriptBufferTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmBuffer::NewContext();
        m_Context = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);

        const dmBuffer::StreamDeclaration streams_decl[] = {
            {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT16, 3},
            {dmHashString64("a"), dmBuffer::VALUE_TYPE_FLOAT32, 1},
        };

        m_Count = 256;
        dmBuffer::Create(m_Count, streams_decl, 2, &m_Buffer);
    }

    virtual void TearDown()
    {
    	if( m_Buffer )
        	dmBuffer::Destroy(m_Buffer);
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);

        dmBuffer::DeleteContext();
    }

    dmScript::HContext m_Context;
    lua_State* L;
    dmBuffer::HBuffer m_Buffer;
    uint32_t m_Count;
};

static bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

TEST_F(ScriptBufferTest, PushCheckBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    dmScript::LuaHBuffer* buffer_ptr = dmScript::CheckBuffer(L, -1);
    ASSERT_NE((void*)0x0, buffer_ptr);
    ASSERT_EQ(m_Buffer, buffer_ptr->m_Buffer);
    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, IsBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_pushstring(L, "not_a_buffer");
    lua_pushnumber(L, 1337);
    ASSERT_FALSE(dmScript::IsBuffer(L, -1));
    ASSERT_FALSE(dmScript::IsBuffer(L, -2));
    ASSERT_TRUE(dmScript::IsBuffer(L, -3));
    lua_pop(L, 3);
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, PrintBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "print(test_buffer)"));

    ASSERT_TRUE(RunString(L, "local stream = buffer.get_stream(test_buffer, \"rgb\"); print(stream)"));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, GetCount)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "assert(256 == #test_buffer)"));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, CreateBuffer)
{
    int top = lua_gettop(L);

    dmScript::LuaHBuffer* buffer = 0;
    uint32_t element_count = 0;

    dmBuffer::ValueType type;
    uint32_t typecount;

    RunString(L, "test_buffer = buffer.create( 12, { {name=hash(\"rgba\"), type=buffer.VALUE_TYPE_UINT8, count=4 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetCount(buffer->m_Buffer, &element_count));
    ASSERT_EQ(12, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("rgba"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_UINT8, type);
    ASSERT_EQ(4, typecount);
    lua_pop(L, 1);

    RunString(L, "test_buffer = buffer.create( 24, { {name=hash(\"position\"), type=buffer.VALUE_TYPE_FLOAT32, count=3 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetCount(buffer->m_Buffer, &element_count));
    ASSERT_EQ(24, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("position"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT32, type);
    ASSERT_EQ(3, typecount);
    lua_pop(L, 1);

    RunString(L, "test_buffer = buffer.create( 10, { {name=hash(\"position\"), type=buffer.VALUE_TYPE_UINT16, count=4 }, \
                                                    {name=hash(\"normal\"), type=buffer.VALUE_TYPE_FLOAT32, count=3 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetCount(buffer->m_Buffer, &element_count));
    ASSERT_EQ(10, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("position"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_UINT16, type);
    ASSERT_EQ(4, typecount);

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("normal"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT32, type);
    ASSERT_EQ(3, typecount);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, GetBytes)
{
    int top = lua_gettop(L);
    uint8_t* data;
    uint32_t datasize;
    dmBuffer::GetBytes(m_Buffer, (void**)&data, &datasize);

    for( uint32_t i = 0; i < datasize; ++i )
    {
        data[i] = i+1;
    }

    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    char str[1024];
    DM_SNPRINTF(str, sizeof(str), " local bytes = buffer.get_bytes(test_buffer, \"rgb\") \
                                    assert(#bytes == %u) \
                                    for i=1,#bytes do \
                                        assert( (i %% 256) == string.byte(bytes, i) )\
                                    end \
                                  ", datasize);
    bool run = RunString(L, str);
    ASSERT_TRUE(run);

    ASSERT_EQ(top, lua_gettop(L));
}


template<typename T>
static void memset_stream(T* data, uint32_t count, uint32_t components, uint32_t stride, T value)
{
    for(uint32_t i = 0; i < count; ++i) {
        for(uint32_t c = 0; c < components; ++c) {
            data[c] = value;
        }
        data += stride;
    }
}


TEST_F(ScriptBufferTest, Indexing)
{
    int top = lua_gettop(L);

    dmBuffer::Result r;
    uint16_t* stream_rgb = 0;
    uint32_t count_rgb = 0;
    uint32_t components_rgb = 0;
    uint32_t stride_rgb = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("rgb"), (void**)&stream_rgb, &count_rgb, &components_rgb, &stride_rgb);
    uint32_t size_rgb = count_rgb * components_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(uint16_t) * 3u, size_rgb);

    float* stream_a = 0;
    uint32_t count_a = 0;
    uint32_t components_a = 0;
    uint32_t stride_a = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("a"), (void**)&stream_a, &count_a, &components_a, &stride_a);
    uint32_t size_a = count_a * components_a * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_FLOAT32);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(float) * 1u, size_a);

    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    // Set full buffer (uint16)
    memset_stream(stream_rgb, count_rgb, components_rgb, stride_rgb, (uint16_t)0);

    RunString(L, "local stream = buffer.get_stream(test_buffer, hash(\"rgb\")) \
                  for i=1,#stream do \
                    stream[i] = i*3 + i \
                  end \
                  ");

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

    uint16_t* rgb = stream_rgb;
    for( uint32_t i = 0, x = 1; i < count_rgb; ++i )
    {
        for( uint32_t c = 0; c < components_rgb; ++c, ++x )
        {
            ASSERT_EQ(x*3 + x, (uint32_t)rgb[c]);
        }
        rgb += stride_rgb;
    }

    // Set full buffer (float)
    memset_stream(stream_a, count_a, components_a, stride_a, 0.0f);

    RunString(L, "local stream = buffer.get_stream(test_buffer, hash(\"a\")) \
                  for i=1,#stream do \
                    stream[i] = i*5 + 0.5 \
                  end \
                  ");

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

    float* a = stream_a;
    for( uint32_t i = 0, x = 1; i < count_a; ++i )
    {
        for( uint32_t c = 0; c < components_a; ++c, ++x )
        {
            ASSERT_EQ(x*5.0f + 0.5f, a[c]);
        }
        a += stride_a;
    }

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, CopyStream)
{
    int top = lua_gettop(L);

    dmBuffer::Result r;
    uint16_t* stream_rgb = 0;
    uint32_t count_rgb = 0;
    uint32_t components_rgb = 0;
    uint32_t stride_rgb = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("rgb"), (void**)&stream_rgb, &count_rgb, &components_rgb, &stride_rgb);
    uint32_t size_rgb = count_rgb * components_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(uint16_t) * 3u, size_rgb);
    ASSERT_EQ(m_Count, count_rgb);

    float* stream_a = 0;
    uint32_t count_a = 0;
    uint32_t components_a = 0;
    uint32_t stride_a = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("a"), (void**)&stream_a, &count_a, &components_a, &stride_a);
    uint32_t size_a = count_a * components_a * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_FLOAT32);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(float) * 1u, size_a);
    ASSERT_EQ(m_Count, count_a);


    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");


    // Copy one stream to another
    {
        memset_stream(stream_a, count_a, components_a, stride_a, 0.0f);

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        for i=1,#srcstream do \
                            srcstream[i] = i*4 \
                        end \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        float* a = stream_a;
        for(uint32_t i = 1, x = 1; i <= count_a; ++i)
        {
            for(uint32_t c = 0; c < components_a; ++c, ++x)
            {
                ASSERT_EQ(x*4, a[c]);
            }
            a += stride_a;
        }
    }

    // Copy first half of source to latter half of dest
    {
        memset_stream(stream_rgb, count_rgb, components_rgb, stride_rgb, (uint16_t)0);

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_UINT16, count=3 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        for i=1,#srcstream do \
                            srcstream[i] = i*3 \
                        end \
                        local dststream = buffer.get_stream(test_buffer, hash(\"rgb\")) \
                        buffer.copy_stream(dststream, #srcstream / 2, srcstream, 0, #srcstream / 2) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        uint16_t* rgb = stream_rgb;
        uint32_t halfcount = (count_rgb * components_rgb) / 2;
        for(uint32_t i = 1, x = 1; i <= count_rgb; ++i)
        {
            for(uint32_t c = 0; c < components_rgb; ++c, ++x)
            {
                if (x < halfcount)
                    ASSERT_EQ(0, rgb[c]);
                else
                    ASSERT_EQ((x - halfcount) * 3, rgb[c]);
            }
            rgb += stride_rgb;
        }
    }

    // Copy some elements offset from aligned boundaries, and a non multiple of components
    {
        memset_stream(stream_rgb, count_rgb, components_rgb, stride_rgb, (uint16_t)0);

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_UINT16, count=3 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        for i=1,#srcstream do \
                            srcstream[i] = i \
                        end \
                        local dststream = buffer.get_stream(test_buffer, hash(\"rgb\")) \
                        buffer.copy_stream(dststream, 4, srcstream, 8, 10) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        uint16_t* rgb = stream_rgb;
        for(uint32_t i = 0, x = 0; i <= 20; ++i)
        {
            for(uint32_t c = 0; c < components_rgb; ++c, ++x)
            {
                if (x < 4 || x >= 14)
                    ASSERT_EQ(0, rgb[c]);
                else
                    ASSERT_EQ(9 + (x-4), rgb[c]);
            }
            rgb += stride_rgb;
        }
    }


    dmLogWarning("Expected error outputs ->");

    // Too large count
    {
        memset_stream(stream_a, count_a, components_a, stride_a, 42.0f); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream + 10) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));
        float* a = (float*)stream_a;
        for(uint32_t i = 1; i <= count_a; ++i)
        {
            for(uint32_t c = 0; c < components_a; ++c)
            {
                ASSERT_EQ(42.0f, a[c]);
            }
            a += stride_a;
        }
        lua_pop(L, 1);
    }

    // Buffer overrun (write)
    {
        memset_stream(stream_a, count_a, components_a, stride_a, 76.0f); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 5, srcstream, 0, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));
        float* a = (float*)stream_a;
        for(uint32_t i = 1; i <= count_a; ++i)
        {
            for(uint32_t c = 0; c < components_a; ++c)
            {
                ASSERT_EQ(76.0f, a[c]);
            }
            a += stride_a;
        }
        lua_pop(L, 1);
    }


    // Buffer overrun (read)
    {
        memset(stream_a, 'X', size_a); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 5, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= m_Count; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
        }
        lua_pop(L, 1);
    }

    dmLogWarning("<- Expected error outputs end.");

    ASSERT_EQ(top, lua_gettop(L));
}

struct CopyBufferTestParams
{
    uint32_t m_Count;
    uint32_t m_DstOffset;
    uint32_t m_SrcOffset;
    uint32_t m_CopyCount;
    bool m_ExpectedOk;
};

class ScriptBufferCopyTest : public ::testing::TestWithParam<CopyBufferTestParams>
{
protected:
    virtual void SetUp()
    {
        dmBuffer::NewContext();
        m_Context = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);


        const dmBuffer::StreamDeclaration streams_decl[] = {
            {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT16, 3},
            {dmHashString64("a"), dmBuffer::VALUE_TYPE_FLOAT32, 1},
        };

        const CopyBufferTestParams& p = GetParam();
        dmBuffer::Create(p.m_Count, streams_decl, 2, &m_Buffer);
    }

    virtual void TearDown()
    {
        dmBuffer::Destroy(m_Buffer);

        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);

        dmBuffer::DeleteContext();
    }

    dmScript::HContext m_Context;
    lua_State* L;
    dmBuffer::HBuffer m_Buffer;
};


TEST_P(ScriptBufferCopyTest, CopyBuffer)
{
    const CopyBufferTestParams& p = GetParam();

    int top = lua_gettop(L);

    dmBuffer::Result r;
    uint16_t* stream_rgb = 0;
    uint32_t count_rgb = 0;
    uint32_t components_rgb = 0;
    uint32_t stride_rgb = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("rgb"), (void**)&stream_rgb, &count_rgb, &components_rgb, &stride_rgb);
    uint32_t size_rgb = count_rgb * components_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(p.m_Count * sizeof(uint16_t) * 3u, size_rgb);

    float* stream_a = 0;
    uint32_t count_a = 0;
    uint32_t components_a = 0;
    uint32_t stride_a = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("a"), (void**)&stream_a, &count_a, &components_a, &stride_a);
    uint32_t size_a = count_a * components_a * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_FLOAT32);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(p.m_Count * sizeof(float) * 1u, size_a);


    uint8_t* data;
    uint32_t datasize;
    dmBuffer::GetBytes(m_Buffer, (void**)&data, &datasize);
    uint32_t stride = stride_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    memset(data, 0, datasize);

    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "dstbuffer");

        // Copy one stream to another
    {
        memset(data, 0, datasize);

        char str[1024];
        DM_SNPRINTF(str, sizeof(str), " local srcbuffer = buffer.create( %u, { {name=hash(\"rgb\"), type=buffer.VALUE_TYPE_UINT16, count=3 }, \
                                                                                  {name=hash(\"a\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                                        local rgb = buffer.get_stream(srcbuffer, \"rgb\") \
                                        local a = buffer.get_stream(srcbuffer, \"a\") \
                                        for i=1,%u do \
                                            rgb[(i-1)*3 + 0 + 1] = i*3 \
                                            rgb[(i-1)*3 + 1 + 1] = i*5 \
                                            rgb[(i-1)*3 + 2 + 1] = i*7 \
                                            a[i] = i*3 +  i*5 + i*7 \
                                        end \
                                        buffer.copy_buffer(dstbuffer, %u, srcbuffer, %u, %u) \
                                      ", p.m_Count, p.m_Count, p.m_DstOffset, p.m_SrcOffset, p.m_CopyCount);

        if( !p.m_ExpectedOk )
        {
            dmLogWarning("Expected error outputs ->");
        }

        bool run = RunString(L, str);
        ASSERT_EQ(p.m_ExpectedOk, run);
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        if( p.m_ExpectedOk )
        {
            int j = 0;

            j = 0;

            // Check that the buffer before the write area is intact
            for( uint32_t i = 1; i <= p.m_DstOffset; ++i)
            {
                uint32_t index = (i-1) * stride;
                ASSERT_EQ(0, data[index]);
            }

            // Check that the buffer after the write area is intact
            for( uint32_t i = p.m_DstOffset + p.m_CopyCount + 1; i <= p.m_Count; ++i )
            {
                uint32_t index = (i-1) * stride;
                ASSERT_EQ(0, data[index]);
            }

            // Loop over RGB and A to make sure we copied the streams correctly from the source buffer to the target buffer
            uint16_t* rgb = stream_rgb + p.m_DstOffset * stride_rgb;
            float* a = stream_a + p.m_DstOffset * stride_a;
            // Loop variable "i" is the struct index (i.e. vertex number)
            for( uint32_t i = p.m_DstOffset+1; i <= p.m_DstOffset + p.m_CopyCount; ++i )
            {
                uint32_t srci = p.m_SrcOffset + i - p.m_DstOffset;

                for( uint32_t c = 0; c < components_rgb; ++c)
                {
                    ASSERT_EQ( srci * (3 + c*2), rgb[c] );
                }
                rgb += stride_rgb;

                ASSERT_EQ( srci*3 + srci*5 + srci*7, a[0] );
                a += stride_a;
            }
        }
        else
        {
            for( uint32_t i = 1; i <= p.m_Count; ++i )
            {
                ASSERT_EQ(0, stream_rgb[(i-1)*3 + 0]);
                ASSERT_EQ(0, stream_rgb[(i-1)*3 + 1]);
                ASSERT_EQ(0, stream_rgb[(i-1)*3 + 2]);
                ASSERT_EQ(0, stream_a[i-1]);
            }
            lua_pop(L, 1); // ?
        }


        if( !p.m_ExpectedOk )
        {
            dmLogWarning("<- Expected error outputs end.");
        }
    }

    ASSERT_EQ(top, lua_gettop(L));
}


const CopyBufferTestParams buffer_copy_setups[] = {
    // VALID
    {64, 0, 0, 64, true}, // whole buffer
    {64, 0, 0, 16, true},  // copy first elements, to the start of the buffer
    {64, 64-16, 0, 16, true}, // copy first elements, to the end of the buffer
    {64, 64-16, 64-16, 16, true}, // copy last elements, to the end of the buffer
    {64, 0, 64-1, 1, true}, // single element
    // INVALID (leaves the buffer unchanged)
    {64, 0, 0, 100, false}, // too many elements
    {64, 0, 64-16, 32, false},  // overrun (read)
    {64, 64-16, 0, 32, false},  // overrun (write)
    {64, 0, 0, 0, false}, // zero elements
};

INSTANTIATE_TEST_CASE_P(ScriptBufferCopySequence, ScriptBufferCopyTest, ::testing::ValuesIn(buffer_copy_setups));


TEST_F(ScriptBufferTest, RefCount)
{
    bool run;
    run = RunString(L, "local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        srcbuffer = nil \
                        collectgarbage() \
                        srcstream[1] = 1.0 \
                       ");
    ASSERT_TRUE(run);

    // Create a buffer, store it globally, test that it works, remove buffer, test that the script usage throws an error
    dmScript::LuaHBuffer luabuf = {m_Buffer, false};
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "print(test_buffer)"));

    dmBuffer::Destroy(m_Buffer);
    m_Buffer = 0;

    dmLogWarning("Expected error outputs ->");

	ASSERT_FALSE(RunString(L, "print(test_buffer)"));

    dmLogWarning("<- Expected error outputs end.");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
