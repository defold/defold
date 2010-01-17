
#ifndef DM_RENDER_H
#define DM_RENDER_H

#include <stdint.h>
#include <dlib/container.h>

namespace dmRender
{
    using namespace dmContainer;

    /*!
    *   \enum Result
    *   \ingroup dmRender
    *   \brief Result values enumeration
    */
    typedef dmContainer::Result Result;

    /*!
    *   \enum ContextAttributes
    *   \ingroup dmRender
    *   \brief ContextAttributes values enumeration
    */
    typedef dmContainer::ContextAttributes ContextAttributes;

    /*!
    *   \Handle HContext
    *   \ingroup dmRender
    *   \brief Handle of context
    */
    typedef dmContainer::HContext HContext;

    /*!
    *   \struct ContextDesc
    *   \ingroup dmRender
    *   \brief Context data desc describing context attributes
    */
    typedef dmContainer::ContextDesc ContextDesc;

    /*!
    *   \Handle HContainer
    *   \ingroup dmRender
    *   \brief Handle of container
    */
    typedef dmContainer::HContainer HContainer;

    /*!
    *   \struct ContainerDesc
    *   \ingroup dmRender
    *   \brief Container data desc describing container attributes
    */
    typedef dmContainer::ContainerDesc ContainerDesc;

    /*!
    *   \struct ObjectDesc
    *   \ingroup dmRender
    *   \brief Object type data desc describing object attributes
    */
    typedef dmContainer::ObjectDesc ObjectDesc;

    /*!
    *   \Handle HInstance
    *   \ingroup dmRender
    *   \brief Handle of instance
    */
    typedef dmContainer::HInstance HInstance;

    /*
    * \fn RegisterContext
    * \param desc ContextDesc data structure describing the context to register
    * \param out_context HContext ptr receiving the registered context if successful
    * \return see Result
    */
    Result RegisterContext(ContextDesc *desc, HContext *out_context)
    {
        return dmContainer::RegisterContext(desc, out_context);
    }

    /*
    * \fn UnregisterContext
    * \param context Handle of context to unregister
    * \return see Result
    */
    Result UnregisterContext(HContext context)
    {
        return dmContainer::UnregisterContext(context);
    }

    /*
    * \fn RegisterContainer
    * \param desc ContainerDesc data structure describing the container to register
    * \param out_container HContainer ptr receiving the registered container if successful
    * \brief Containers with ContainerDesc m_Mask member set to zero are unaffected by predicate (always updated)
    * \return see Result
    */
    Result RegisterContainer(ContainerDesc *desc, HContainer *out_container)
    {
        return dmContainer::RegisterContainer(desc, out_container);
    }

    /*
    * \fn UnregisterContainer
    * \param container_handle Handle of container to unregister
    */
    Result UnregisterContainer(HContainer container_handle)
    {
        return dmContainer::UnregisterContainer(container_handle);
    }

    /*
    * \fn RegisterObject
    * \param desc ObjectDesc data structure describing the object to register
    * \return see Result
    */
    Result RegisterObject(ObjectDesc *desc)
    {
        return dmContainer::RegisterObject(desc);
    }

    /*
    * \fn RegisterObject
    * \param desc ObjectDesc data structure describing the object to register
    * \param container_handle HContainer handle of container. m_ContainerMask of the ObjectDesc structure must be zero.
    * \brief Objects registered in with a handle are unregistered when the parent container is unregistered. If you
    * \try to unregister an object already unregistered by a container, result will be OBJECT_NOT_REGISTERED.
    * \return see Result
    */
    Result RegisterObject(ObjectDesc *desc, HContainer container_handle)
    {
        return dmContainer::RegisterObject(desc, container_handle);
    }

    /*
    * \fn UnregisterObject
    * \param context_handle Handle of context this object is registered to
    * \param object_id Unique id passed as m_ID of the object desc when registered
    * \return see Result
    */
    Result UnregisterObject(HContext context_handle, uint32_t object_id)
    {
        return dmContainer::UnregisterObject(context_handle, object_id);
    }

    /*
    * \fn AddInstances
    * \param context_handle Handle of context this instance array is added to
    * \param object_id Unique ID of object this instance array is to be added to
    * \param instace_array instance pointer array of the instances to add
    * \param count Number of instances
    * \param out_instance_handles Ptr to array which receives handles of instances added
    * \return see Result
    */
    Result AddInstances(HContext context_handle, uint32_t object_id, void *instace_data_array[], uint32_t count, HInstance *out_instance_handles[])
    {
        return  dmContainer::AddInstances(context_handle, object_id, instace_data_array, count, out_instance_handles);
    }

    /*
    * \fn RemoveInstances
    * \param handle Ptr to array of instance handles to remove
    * \param count Number of instances in instace_array
    * \return see Result
    */
    Result RemoveInstances(HInstance *instace_array[], uint32_t count)
    {
        return dmContainer::RemoveInstances(instace_array, count);
    }

    /*
    * \fn Update
    * \param context_handle to update
    * \param predicate Bitmask of containers to update. Containers with m_Mask member set to zero are always updated.
    * \param flags EUpdateflags bitmask defining behaviour. 0 is default.
    * \brief The predicate bitmask describes what containers to update. The bitmask corresponds to container index by bitweight.
    * \Update order is from 0-31. Containers with m_Mask member set to zero are always updated last, in creation order. 
    * \return see Result
    */
    Result Update(HContext context_handle, uint32_t predicate, uint32_t flags)
    {
        return dmContainer::Update(context_handle, predicate, flags);
    }

};

#endif // DM_RENDER_H
