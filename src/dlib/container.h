#ifndef DM_CONTAINER_H
#define DM_CONTAINER_H

#include <stdint.h>

namespace dmContainer
{
    /*!
    *   \enum Result
    *   \ingroup dmContainer
    *   \brief Result values enumeration
    */
    enum Result
    {
        OUT_OF_MEMORY = -7,             //! Call parameter failed, out of memory
        INVALID_PARAMETER = -6,         //! Call parameter inconsistent or out of range
        CONTEXT_NOT_REGISTERED = -5,    //! Trying to access context not registered
        CONTAINER_NOT_REGISTERED = -4,  //! Trying to access container not registered
        OBJECT_NOT_REGISTERED = -3,     //! Trying to access object not registered
        NOT_FOUND = -2,                 //! Query not found
        ALREADY_EXISTS = -1,            //! Already exists
        SUCCESS = 0,                    //! Call succesful
    };

    /*!
    *   \enum UpdateFlags
    *   \ingroup dmContainer
    *   \brief UpdateFlags values enumeration
    */
    enum Updateflags
    {
        UPDATE_FLAG_PREDICATED_NONE         = 0,
        UPDATE_FLAG_PREDICATED_ONLY         = 1<<0,     //! Only update predicated objects
        UPDATE_FLAG_NON_PREDICATED_ONLY     = 1<<1,     //! Only update objects not predicated
    };

    /*!
    *   \enum EContextAttributes
    *   \ingroup dmContainer
    *   \brief EContextAttributes values enumeration
    */
    enum ContextAttributes
    {
        ACTIVE              = 1<<0,     //! Context is active
        STATISTICS          = 1<<1,     //! Statistics is enabled
    };

    /*!
    *   \Handle HContext
    *   \ingroup dmContainer
    *   \brief Handle of context
    */
    typedef class Context * HContext;

    /*!
    *   \struct ContextDesc
    *   \ingroup dmContainer
    *   \brief Context data desc describing context attributes
    */
    struct ContextDesc
    {
        uint32_t m_ID;                                  //! Unique ID of this context
        uint32_t m_Attributes;                          //! Attributes as defined by EContextAttributes
        void	 *m_WorkSpace;                          //! User allocated workspace (optional)
        uint32_t m_WorkSpaceSize;                       //! User allocated workspace size in bytes (ignored if m_WorkSpace is zero)
        void	(*m_BeginFunc)(const ContextDesc *);    //! Function callback when before any updating is done
        void	(*m_EndFunc)  (const ContextDesc *);    //! Function callback when all updating is done
    };


    /*!
    *   \Handle HContainer
    *   \ingroup dmContainer
    *   \brief Handle of container
    */
    typedef class Container * HContainer;

    /*!
    *   \struct ContainerDesc
    *   \ingroup dmContainer
    *   \brief Container data desc describing container attributes
    */
    struct ContainerDesc
    {
        HContext m_HContext;                //! Handle of context this container is registered to
        uint32_t m_Mask;                    //! Exclusive bitmask of container
        void     *m_Data;                   //! User data ptr
        void     (*m_BeginFunc)(const ContainerDesc *); //! Function callback when before container updating is done
        void     (*m_EndFunc)  (const ContainerDesc *); //! Function callback when all container is done
    };

    /*!
    *   \struct ObjectDesc
    *   \ingroup dmContainer
    *   \brief Object type data desc describing object attributes
    */
    struct ObjectDesc
    {
        HContext m_HContext;            //! Handle of context this object is registered to
        uint32_t m_ID;                  //! Unique ID of this object type in the scope of the context registered to
        uint32_t m_ContainerMask;       //! Bitmask of containers this object is enabled to (bit 0-31 corresponds to index 0-31)
        void	 *m_Data;               //! User data ptr
        void	(*m_BeginFunc) (const ContainerDesc *, const ObjectDesc *);         //! Function callback when beginning to update this object with a container
        void	(*m_EndFunc)   (const ContainerDesc *, const ObjectDesc *);         //! Function callback when done updating this object with a container
        void	(*m_UpdateFunc)(const ContainerDesc *, const ObjectDesc *, const void *[], const uint32_t count);   //! Function callback for each instance array updated with this object
    };

    /*!
    *   \Handle HInstance
    *   \ingroup dmContainer
    *   \brief Handle of instance
    */
    typedef class Instance * HInstance;

    /*
    * \fn RegisterContext
    * \param desc ContextDesc data structure describing the context to register
    * \param out_context HContext ptr receiving the registered context if successful
    * \return see Result
    */
    Result RegisterContext(ContextDesc *desc, HContext *out_context);

    /*
    * \fn UnregisterContext
    * \param context Handle of context to unregister
    * \return see Result
    */
    Result UnregisterContext(HContext context);

    /*
    * \fn RegisterContainer
    * \param desc ContainerDesc data structure describing the container to register
    * \param out_container HContainer ptr receiving the registered container if successful
    * \brief Containers with ContainerDesc m_Mask member set to zero are unaffected by predicate (always updated)
    * \return see Result
    */
    Result RegisterContainer(ContainerDesc *desc, HContainer *out_container);

    /*
    * \fn UnregisterContainer
    * \param container_handle Handle of container to unregister
    */
    Result UnregisterContainer(HContainer container_handle);

    /*
    * \fn RegisterObject
    * \param desc ObjectDesc data structure describing the object to register
    * \return see Result
    */
    Result RegisterObject(ObjectDesc *desc);

    /*
    * \fn RegisterObject
    * \param desc ObjectDesc data structure describing the object to register
    * \param container_handle HContainer handle of container. m_ContainerMask of the ObjectDesc structure must be zero.
    * \return see Result
    */
    Result RegisterObject(ObjectDesc *desc, HContainer container_handle);

    /*
    * \fn UnregisterObject
    * \param context_handle Handle of context this object is registered to
    * \param object_id Unique id passed as m_ID of the object desc when registered
    * \return see Result
    */
    Result UnregisterObject(HContext context_handle, uint32_t object_id);

    /*
    * \fn AddInstances
    * \param context_handle Handle of context this instance array is added to
    * \param object_id Unique ID of object this instance array is to be added to
    * \param instace_array instance pointer array of the instances to add
    * \param count Number of instances
    * \param out_instance_handles Ptr to array which receives handles of instances added
    * \return see Result
    */
    Result AddInstances(HContext context_handle, uint32_t object_id, void *instace_data_array[], uint32_t count, HInstance *out_instance_handles[]);

    /*
    * \fn RemoveInstances
    * \param handle Ptr to array of instance handles to remove
    * \param count Number of instances in instace_array
    * \return see Result
    */
    Result RemoveInstances(HInstance *instace_array[], uint32_t count);

    /*
    * \fn Update
    * \param context_handle to update
    * \param predicate Bitmask of containers to update. Containers with m_Mask member set to zero are always updated.
    * \param flags UpdateFlags bitmask defining behaviour. 0 is default.
    * \brief The predicate bitmask describes what containers to update. The bitmask corresponds to container index by bitweight.
    * \Update order is from 0-31. Containers with m_Mask member set to zero are always updated last, in creation order. 
    * \return see Result
    */
    Result Update(HContext context_handle, uint32_t predicate, uint32_t flags);

};

#endif // DM_CONTAINER_H
