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
        m_Context = dmScript::NewContext(0, 0);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);
    }

    virtual void TearDown()
    {
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);
    }

    dmScript::HContext m_Context;
    lua_State* L;
};

bool RunString(lua_State* L, const char* script)
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
    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
        {dmHashString64("velocity"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
        {dmHashString64("color"), dmBuffer::VALUE_TYPE_UINT8, 4},
    };

    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::Result r = dmBuffer::Allocate(4, streams_decl, 3, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    int top = lua_gettop(L);
    dmScript::PushBuffer(L, buffer);
    dmBuffer::HBuffer* buffer_ptr = dmScript::CheckBuffer(L, -1);
    ASSERT_NE((void*)0x0, buffer_ptr);
    ASSERT_EQ(buffer, *buffer_ptr);
    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, IsBuffer)
{
    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3}
    };

    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::Result r = dmBuffer::Allocate(4, streams_decl, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    int top = lua_gettop(L);
    dmScript::PushBuffer(L, buffer);
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
    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3}
    };

    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::Result r = dmBuffer::Allocate(4, streams_decl, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    int top = lua_gettop(L);
    dmScript::PushBuffer(L, buffer);
    dmBuffer::HBuffer* buffer_ptr = dmScript::CheckBuffer(L, -1);
    (void)buffer_ptr;
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "print(test_buffer)"));

    ASSERT_TRUE(RunString(L, "local stream = buffer.get_stream(test_buffer, \"position\"); print(stream)"));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, GetElementCount)
{
    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3}
    };

    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::Allocate(1337, streams_decl, 1, &buffer);

    int top = lua_gettop(L);
    dmScript::PushBuffer(L, buffer);
    dmBuffer::HBuffer* buffer_ptr = dmScript::CheckBuffer(L, -1);
    (void)buffer_ptr;
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "assert(1337 == #test_buffer)"));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, CreateBuffer)
{
    int top = lua_gettop(L);

    dmBuffer::HBuffer* buffer = 0;
    uint32_t element_count = 0;

    dmBuffer::ValueType type;
    uint32_t typecount;

    RunString(L, "test_buffer = buffer.create( 12, { {name=hash(\"rgba\"), type=buffer.VALUE_TYPE_UINT8, count=4 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetElementCount(*buffer, &element_count));
    ASSERT_EQ(12, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(*buffer, dmHashString64("rgba"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_UINT8, type);
    ASSERT_EQ(4, typecount);
    lua_pop(L, 1);

    RunString(L, "test_buffer = buffer.create( 24, { {name=hash(\"position\"), type=buffer.VALUE_TYPE_FLOAT32, count=3 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetElementCount(*buffer, &element_count));
    ASSERT_EQ(24, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(*buffer, dmHashString64("position"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT32, type);
    ASSERT_EQ(3, typecount);
    lua_pop(L, 1);

    RunString(L, "test_buffer = buffer.create( 10, { {name=hash(\"position\"), type=buffer.VALUE_TYPE_FLOAT64, count=4 }, \
                                                    {name=hash(\"normal\"), type=buffer.VALUE_TYPE_FLOAT32, count=3 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetElementCount(*buffer, &element_count));
    ASSERT_EQ(10, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(*buffer, dmHashString64("position"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT64, type);
    ASSERT_EQ(4, typecount);

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(*buffer, dmHashString64("normal"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT32, type);
    ASSERT_EQ(3, typecount);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}


TEST_F(ScriptBufferTest, Indexing)
{
    int top = lua_gettop(L);

    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT16, 3},
        {dmHashString64("a"), dmBuffer::VALUE_TYPE_FLOAT32, 1},
    };

    uint32_t size = 16;

    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::Result r = dmBuffer::Allocate(size*size, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    uint8_t* stream_rgb = 0;
    uint32_t size_rgb = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("rgb"), (void**)&stream_rgb, &size_rgb);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(size * size * sizeof(uint16_t) * 3u, size_rgb);

    uint8_t* stream_a = 0;
    uint32_t size_a = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("a"), (void**)&stream_a, &size_a);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(size * size * sizeof(float) * 1u, size_a);


    dmScript::PushBuffer(L, buffer);
    lua_setglobal(L, "test_buffer");

    // Set full buffer (uint16)
    memset(stream_rgb, 'X', size_rgb); // memset the RGB stream

    RunString(L, "local stream = buffer.get_stream(test_buffer, hash(\"rgb\")) \
                  for i=1,#stream do \
                    stream[i] = i*3 + i \
                  end \
                  ");

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

    uint16_t* rgb = (uint16_t*)stream_rgb;
    for( uint32_t i = 1; i <= size*size; ++i )
    {
        ASSERT_EQ(i*3 + i, (uint32_t)rgb[i-1]);
    }

    // Set full buffer (float)
    memset(stream_a, 'X', size_a); // memset the A stream

    RunString(L, "local stream = buffer.get_stream(test_buffer, hash(\"a\")) \
                  for i=1,#stream do \
                    stream[i] = i*5 + 0.5 \
                  end \
                  ");

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

    float* a = (float*)stream_a;
    for( uint32_t i = 1; i <= size*size; ++i )
    {
        ASSERT_EQ(i*5.0f + 0.5f, a[i-1]);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, CopyStream)
{
    int top = lua_gettop(L);

    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT16, 3},
        {dmHashString64("a"), dmBuffer::VALUE_TYPE_FLOAT32, 1},
    };

    uint32_t size = 16;

    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::Result r = dmBuffer::Allocate(size*size, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    uint8_t* stream_rgb = 0;
    uint32_t size_rgb = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("rgb"), (void**)&stream_rgb, &size_rgb);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(size * size * sizeof(uint16_t) * 3u, size_rgb);

    uint8_t* stream_a = 0;
    uint32_t size_a = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("a"), (void**)&stream_a, &size_a);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(size * size * sizeof(float) * 1u, size_a);


    dmScript::PushBuffer(L, buffer);
    lua_setglobal(L, "test_buffer");

    // Copy one stream to another
    {
        memset(stream_a, 'X', size_a); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        for i=1,#srcstream do \
                            srcstream[i] = i*4 + 1.5 \
                        end \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

        float* a = (float*)stream_a;
        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ(i*4.0f + 1.5f, a[i-1]);
        }
    }

    dmLogWarning("Expected error outputs ->");

    // Too large count
    {
        memset(stream_a, 'X', size_a); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream + 10) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
        }
        lua_pop(L, 1);
    }

    // Buffer overrun (write)
    {
        memset(stream_a, 'X', size_a); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 5, srcstream, 0, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
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
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
        }
        lua_pop(L, 1);
    }

    dmLogWarning("<- Expected error outputs end.");

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, CopyBuffer)
{
    int top = lua_gettop(L);

    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT16, 3},
        {dmHashString64("a"), dmBuffer::VALUE_TYPE_FLOAT32, 1},
    };

    uint32_t size = 16;

    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::Result r = dmBuffer::Allocate(size*size, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    uint16_t* stream_rgb = 0;
    uint32_t size_rgb = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("rgb"), (void**)&stream_rgb, &size_rgb);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(size * size * sizeof(uint16_t) * 3u, size_rgb);

    float* stream_a = 0;
    uint32_t size_a = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("a"), (void**)&stream_a, &size_a);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(size * size * sizeof(float) * 1u, size_a);

    dmScript::PushBuffer(L, buffer);
    lua_setglobal(L, "dstbuffer");

    // Copy one stream to another
    {
        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"rgb\"), type=buffer.VALUE_TYPE_UINT16, count=3 }, \
                                                                  {name=hash(\"a\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local rgb = buffer.get_stream(srcbuffer, \"rgb\") \
                        local a = buffer.get_stream(srcbuffer, \"a\") \
                        for i=1,#srcbuffer do \
                            print('index', i, i*3 + 0 - 3 + 1, i*3 + 1 - 3 + 1, i*3 + 2 - 3 + 1) \
                            rgb[i*3 + 0 - 3 + 1] = i*3 \
                            rgb[i*3 + 1 - 3 + 1] = i*5 \
                            rgb[i*3 + 2 - 3 + 1] = i*7 \
                            a[i] = i*3 +  i*5 + i*7 \
                        end \
                        buffer.copy_buffer(dstbuffer, 0, srcbuffer, 0, #srcbuffer) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ( i*3, stream_rgb[(i-1)*3 + 0]);
            ASSERT_EQ( i*5, stream_rgb[(i-1)*3 + 1]);
            ASSERT_EQ( i*7, stream_rgb[(i-1)*3 + 2]);
            ASSERT_EQ( i*3 + i*5 + i*7, stream_a[(i-1)]);
        }

    }

    dmLogWarning("Expected error outputs ->");

/*
    // Too large count
    {
        memset(stream_a, 'X', size_a); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream + 10) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
        }
        lua_pop(L, 1);
    }

    // Buffer overrun (write)
    {
        memset(stream_a, 'X', size_a); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 5, srcstream, 0, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
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
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= size*size; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
        }
        lua_pop(L, 1);
    }
*/
    dmLogWarning("<- Expected error outputs end.");

    ASSERT_EQ(top, lua_gettop(L));
}


TEST_F(ScriptBufferTest, RefCount)
{
    RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                    local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                    srcbuffer = nil \
                    collectgarbage() \
                   ");
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
