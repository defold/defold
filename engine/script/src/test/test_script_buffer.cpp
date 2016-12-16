#include <gtest/gtest.h>

#include <stdio.h>
#include <stdint.h>

#include "script.h"
#include "script_buffer.h"

#include <dlib/buffer.h>
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
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "print(test_buffer)"));

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
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "assert(1337 == #test_buffer)"));

    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
