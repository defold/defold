#ifndef DM_GUI_PRIVATE_H
#define DM_GUI_PRIVATE_H

#include <dlib/index_pool.h>
#include <dlib/array.h>
#include <dlib/hashtable.h>

#include "gui.h"

extern "C"
{
#include "lua/lua.h"
}

namespace dmGui
{
    const uint32_t MAX_MESSAGE_DATA_SIZE = 512;

    extern dmHashTable64<const dmDDF::Descriptor*> m_DDFDescriptors;

    enum ScriptFunction
    {
        SCRIPT_FUNCTION_INIT,
        SCRIPT_FUNCTION_FINAL,
        SCRIPT_FUNCTION_UPDATE,
        SCRIPT_FUNCTION_ONMESSAGE,
        SCRIPT_FUNCTION_ONINPUT,
        SCRIPT_FUNCTION_ONRELOAD,
        MAX_SCRIPT_FUNCTION_COUNT
    };

    struct Context
    {
        lua_State*         m_LuaState;
        dmMessage::HSocket m_Socket;
    };

    struct InternalNode
    {
        Node     m_Node;
        dmhash_t m_NameHash;
        uint16_t m_Version;
        uint16_t m_Index;
        uint16_t m_Deleted : 1; // Set to true for deferred deletion
    };

    struct NodeProxy
    {
        HScene m_Scene;
        HNode  m_Node;
    };

    struct Animation
    {
        HNode    m_Node;
        Vector4* m_Value;
        Vector4  m_From;
        Vector4  m_To;
        float    m_Delay;
        float    m_Elapsed;
        float    m_Duration;
        float    m_BezierControlPoints[4];
        AnimationComplete m_AnimationComplete;
        void*    m_Userdata1;
        void*    m_Userdata2;
        uint16_t m_FirstUpdate : 1;
        uint16_t m_AnimationCompleteCalled : 1;
    };

    struct Script
    {
        Script();

        int         m_FunctionReferences[MAX_SCRIPT_FUNCTION_COUNT];
        Context*    m_Context;
    };

    struct Scene
    {
        int                     m_SelfReference;
        Context*                m_Context;
        Script*                 m_Script;
        dmIndexPool16           m_NodePool;
        dmArray<InternalNode>   m_Nodes;
        dmArray<Animation>      m_Animations;
        dmHashTable64<void*>    m_Textures;
        dmHashTable64<void*>    m_Fonts;
        void*                   m_DefaultFont;
        void*                   m_UserData;
        uint16_t                m_NextVersionNumber;
    };

    InternalNode* GetNode(HScene scene, HNode node);
}

#endif
