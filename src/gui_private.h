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

    extern dmHashTable32<const dmDDF::Descriptor*> m_DDFDescriptors;

    struct Gui
    {
        lua_State*                              m_LuaState;
        uint32_t                                m_Socket;
    };

    struct InternalNode
    {
        Node     m_Node;
        uint64_t m_NameHash;
        uint16_t m_Version;
        uint16_t m_Index;
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

    struct Scene
    {
        int                   m_InitFunctionReference;
        int                   m_UpdateFunctionReference;
        int                   m_OnInputFunctionReference;
        int                   m_OnMessageFunctionReference;
        int                   m_SelfReference;
        uint16_t              m_RunInit : 1;
        Gui*                  m_Gui;
        dmIndexPool16         m_NodePool;
        dmArray<InternalNode> m_Nodes;
        dmArray<Animation>    m_Animations;
        dmHashTable64<void*>  m_Textures;
        dmHashTable64<void*>  m_Fonts;
        void*                 m_DefaultFont;
        void*                 m_UserData;
        uint16_t              m_NextVersionNumber;
    };

    InternalNode* GetNode(HScene scene, HNode node);
}

#endif
