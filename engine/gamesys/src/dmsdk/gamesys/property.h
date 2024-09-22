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

#ifndef DMSDK_GAMESYSTEM_PROPERTY_H
#define DMSDK_GAMESYSTEM_PROPERTY_H

#include <dmsdk/dlib/hash.h>
#include <dmsdk/gameobject/gameobject.h>

namespace dmResource
{
    typedef struct ResourceFactory* HFactory;
}

namespace dmGameSystem
{
    struct PropVector3
    {
        dmhash_t m_Vector;
        dmhash_t m_X;
        dmhash_t m_Y;
        dmhash_t m_Z;
        bool m_ReadOnly;

        PropVector3(dmhash_t v, dmhash_t x, dmhash_t y, dmhash_t z, bool readOnly)
        {
            m_Vector = v;
            m_X = x;
            m_Y = y;
            m_Z = z;
            m_ReadOnly = readOnly;
        }
    };

    struct PropVector4
    {
        dmhash_t m_Vector;
        dmhash_t m_X;
        dmhash_t m_Y;
        dmhash_t m_Z;
        dmhash_t m_W;
        bool m_ReadOnly;

        PropVector4(dmhash_t v, dmhash_t x, dmhash_t y, dmhash_t z, dmhash_t w, bool readOnly)
        {
            m_Vector = v;
            m_X = x;
            m_Y = y;
            m_Z = z;
            m_W = w;
            m_ReadOnly = readOnly;
        }
    };

    /*#
     * Checks if the name matches any field in the property
     * @name IsReferencingProperty
     * @param property [type: const PropVector3&] the property
     * @param query [type: dmhash_t] the name to look for (e.g. hash("pos.x"))
     * @return result [type: bool] true if the property contains the name
     */
    inline bool IsReferencingProperty(const PropVector3& property, dmhash_t query)
    {
        return property.m_Vector == query || property.m_X == query || property.m_Y == query || property.m_Z == query;
    }

    /*#
     * Checks if the name matches any field in the property
     * @name IsReferencingProperty
     * @param property [type: const PropVector4&] the property
     * @param query [type: dmhash_t] the name to look for (e.g. hash("pos.x"))
     * @return result [type: bool] true if the property contains the name
     */
    inline bool IsReferencingProperty(const PropVector4& property, dmhash_t query)
    {
        return property.m_Vector == query || property.m_X == query || property.m_Y == query || property.m_Z == query || property.m_W == query;
    }

    /*#
     * Gets the resource path hash
     * @name GetResourceProperty
     * @param property [type: const PropVector4&] the property
     * @param resource [type: void*] the resource to get the
     * @param out_value [type: dmGameObject::PropertyDesc&] the out property
     * @return result [type: dmGameObject::PropertyResult] RESULT_OK if successful
     */
    dmGameObject::PropertyResult GetResourceProperty(dmResource::HFactory factory, void* resource, dmGameObject::PropertyDesc& out_value);

    /*#
     * Updates the reference count of the resources, and returns the new resource.
     *
     * @name SetResourceProperty
     * @param factory [type: dmGameObject::HFactory] the factory
     * @param value [type: const dmGameObject::PropertyVar&] the property containing the hash of the resources to get
     * @param ext [type: dmhash_t] the hash of the resource file suffix (without the "."). E.g. hash("spritec")
     * @param out_resource [type: void**] pointer to the current resource. Will also get the pointer to the new resource.
     * @return result [type: dmGameObject::PropertyResult] RESULT_OK if successful
     */
    dmGameObject::PropertyResult SetResourceProperty(dmResource::HFactory factory, const dmGameObject::PropertyVar& value, dmhash_t ext, void** out_resource);

    dmGameObject::PropertyResult SetResourceProperty(dmResource::HFactory factory, const dmGameObject::PropertyVar& value, dmhash_t* exts, uint32_t ext_count, void** out_resource);

#define DM_GAMESYS_PROP_VECTOR3(var_name, prop_name, readOnly)\
    static const dmGameSystem::PropVector3 var_name(dmHashString64(#prop_name),\
            dmHashString64(#prop_name ".x"),\
            dmHashString64(#prop_name ".y"),\
            dmHashString64(#prop_name ".z"),\
            readOnly);

#define DM_GAMESYS_PROP_VECTOR4(var_name, prop_name, readOnly)\
    static const dmGameSystem::PropVector4 var_name(dmHashString64(#prop_name),\
            dmHashString64(#prop_name ".x"),\
            dmHashString64(#prop_name ".y"),\
            dmHashString64(#prop_name ".z"),\
            dmHashString64(#prop_name ".w"),\
            readOnly);


}

#endif // DMSDK_GAMESYSTEM_PROPERTY_H
