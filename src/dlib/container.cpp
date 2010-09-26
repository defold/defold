
#include <assert.h>
#include "container.h"
#include "hashtable.h"
#include "array.h"

const uint32_t const_reserve = 512;

namespace dmContainer
{
    // Local Root object dmContainers.
    class Root
    {
        public:
        Root()
        {
            // for now, just initialize to some default space
            m_Contexts.SetCapacity(const_reserve, const_reserve);
        };

        ~Root()
        {
        };

        dmHashTable<uint32_t, class Context *> m_Contexts;
    } m_Root;

    // Local context class
    class Context
    {
        public:
        ContextDesc m_Desc;
        class Container *m_PredicatedContainers[32];
        uint32_t m_ContainerMask;
        dmArray<class Container *> m_Containers;
        dmHashTable<uint32_t, class Object *> m_Objects;
    };

    // Local container class
    class Container
    {
        public:
        ContainerDesc m_Desc;
        dmArray<class Object *> m_Objects;
    };

    // Local container class
    class Object
    {
        public:
        ObjectDesc m_Desc;
        Container *m_Container;
        uint8_t *m_Instances;
        Instance *m_NextFreeInstance;
        Instance *m_InstanceHead;
        uint32_t m_InstanceCapacity;
    };

    // Local instance class
    class Instance
    {
        public:
        void *m_Data;
        Instance *m_Next;
        Instance *m_Prev;
        union
        {
            Instance *m_NextFree;
            Object *m_Object;
        };
    };


    ////////////////////////////////////////////////////
    Result RegisterContext(ContextDesc *desc, HContext *out_context)
    {
        assert(desc);
        assert(out_context);
        if(m_Root.m_Contexts.Get(desc->m_ID))
            return ALREADY_EXISTS;

        Context *context = new Context;
        memcpy(&context->m_Desc, desc, sizeof(ContextDesc));
        memset(context->m_PredicatedContainers, 0, sizeof(class Container *)*32);
        context->m_ContainerMask = 0;
        m_Root.m_Contexts.Put(context->m_Desc.m_ID, context);

        context->m_Containers.SetCapacity(128);
        context->m_Objects.SetCapacity(const_reserve, const_reserve);

        *out_context = (HContext) context;
        return SUCCESS;
    }
    
    ////////////////////////////////////////////////////
    Result UnregisterContext(HContext context_handle)
    {
        assert(context_handle);
        Context *context = (Context *) context_handle;

        if(!m_Root.m_Contexts.Get(context->m_Desc.m_ID))
            return CONTEXT_NOT_REGISTERED;

        m_Root.m_Contexts.Erase(context->m_Desc.m_ID);

        delete context;
        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result RegisterContainer(ContainerDesc *desc, HContainer *out_container)
    {
        assert(desc);
        assert(out_container);
        Context *context = (Context *) desc->m_HContext;

        // get index
        uint32_t mask = desc->m_Mask;
        uint32_t index = 0;
        if(mask)
        {
            for(; index < 32; index++)
            {
                if(mask & (1<<index))
                {
                    if(mask & ~(1<<index))
                        return INVALID_PARAMETER;		// check exclusive (pow2)
                    if(context->m_PredicatedContainers[index])
                        return ALREADY_EXISTS;
                    break;
                }
            }
        }

        Container *container = new Container;
        memcpy(&container->m_Desc, desc, sizeof(ContainerDesc));
        container->m_Objects.SetCapacity(const_reserve);

        if(mask)
        {
            context->m_ContainerMask |= mask;
            context->m_PredicatedContainers[index] = container;
        }
        else
            context->m_Containers.Push(container);

        *out_container = (Container *) container;
        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result UnregisterContainer(HContainer container_handle)
    {
        assert(container_handle);
        Container *container = (Container *) container_handle;
        Context *context = (Context *) container->m_Desc.m_HContext;

        if(container->m_Desc.m_Mask)
        {
            uint32_t index = 0;
            for(; index < 32; index++)
                if(context->m_PredicatedContainers[index] == container)
                    break;
            if(index == 32)
                return CONTEXT_NOT_REGISTERED;

            context->m_PredicatedContainers[index] = 0;
            context->m_ContainerMask &= ~(1<<index);
        }
        else
        {
            uint32_t index = 0;
            for(; index < context->m_Containers.Size(); index++)
            {
                if(container == context->m_Containers[index])
                    break;
            }
            if(index == context->m_Containers.Size())
                return CONTEXT_NOT_REGISTERED;

            dmArray<class Object *> &objects = container->m_Objects;
            for(uint32_t i = 0; i < objects.Size(); i++ )
            {
                Object *object = objects[i];
                delete[] object->m_Instances;
                context->m_Objects.Erase(object->m_Desc.m_ID);
            }

            context->m_Containers.EraseSwap(index);
        }

        delete container;
        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result RegisterObject(ObjectDesc *desc)
    {
        assert(desc);

        if(!desc->m_ContainerMask)
            return INVALID_PARAMETER;

        Context *context = (Context *) desc->m_HContext;

        if(!(context->m_ContainerMask & desc->m_ContainerMask))
            return CONTAINER_NOT_REGISTERED;

        Object *object = 0;
        if(!context->m_Objects.Empty())
        {
            if(context->m_Objects.Get(desc->m_ID))
                return ALREADY_EXISTS;
        }

        object = new Object;
        memcpy(&object->m_Desc, desc, sizeof(ObjectDesc));
        context->m_Objects.Put(desc->m_ID, object);
        object->m_Container = 0;
        object->m_InstanceCapacity = const_reserve;
        object->m_Instances = new uint8_t[const_reserve*sizeof(Instance)];
        object->m_NextFreeInstance = (Instance *) object->m_Instances;
        object->m_InstanceHead = 0;
        Instance *inst = object->m_NextFreeInstance;
        for(uint32_t i = 0; i < const_reserve-1; i++)
        {
            inst->m_NextFree = inst + 1;
            ++inst;
        }
        inst->m_NextFree = 0;

        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result RegisterObject(ObjectDesc *desc, HContainer container_handle)
    {
        assert(desc);
        assert(container_handle);

        Context *context = (Context *) desc->m_HContext;
        Container *container = (Container *) container_handle;

        if(container->m_Desc.m_Mask)
            return INVALID_PARAMETER;

        Object *object = 0;
        if(!context->m_Objects.Empty())
        {
            if(context->m_Objects.Get(desc->m_ID))
                return ALREADY_EXISTS;
        }

        object = new Object;
        memcpy(&object->m_Desc, desc, sizeof(ObjectDesc));
        container->m_Objects.Push(object);
        object->m_Container = container;
        context->m_Objects.Put(desc->m_ID, object);

        object->m_InstanceCapacity = const_reserve;
        object->m_Instances = new uint8_t[const_reserve*sizeof(Instance)];
        object->m_NextFreeInstance = (Instance *) object->m_Instances;
        object->m_InstanceHead = 0;
        Instance *inst = object->m_NextFreeInstance;
        for(uint32_t i = 0; i < const_reserve-1; i++)
        {
            inst->m_NextFree = inst + 1;
            ++inst;
        }
        inst->m_NextFree = 0;

        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result UnregisterObject(HContext context_handle, uint32_t object_id)
    {
        assert(context_handle);

        Context *context = (Context *) context_handle;
        if(context->m_Objects.Empty())
            return OBJECT_NOT_REGISTERED;
        Object **obj_ptr = context->m_Objects.Get(object_id);
        if(!obj_ptr)
            return OBJECT_NOT_REGISTERED;

        context->m_Objects.Erase(object_id);

        Object *object = *obj_ptr;
        delete[] object->m_Instances;

        Container *container = (Container *) object->m_Container;
        if(container)
        {
            dmArray<class Object *> &objects = container->m_Objects;
            for(uint32_t i = 0; i < objects.Size(); i++ )
            {
                if(object == objects[i])
                {
                    objects.EraseSwap(i);
                    break;
                }
            }
        }

        delete object;
        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result AddInstances(HContext context_handle, uint32_t object_id, void *instace_data_array[], uint32_t count, HInstance *out_instance_handles[])
    {
        assert(context_handle);
        assert(instace_data_array);
        if(!count)
            return SUCCESS;

        Context *context = (Context *) context_handle;
        if(context->m_Objects.Empty())
            return OBJECT_NOT_REGISTERED;
        Object **obj_ptr = context->m_Objects.Get(object_id);
        if(!obj_ptr)
            return OBJECT_NOT_REGISTERED;

        Object *object = *obj_ptr;

        Instance *inst = object->m_NextFreeInstance;
        if(!inst)
            return OUT_OF_MEMORY;
        Instance *first = inst;
        Instance *last = 0;

        for(uint32_t i = 0; i < count; i++)
        {
            void *data = instace_data_array[i];
            inst->m_Data = data;
            Instance *next_free = inst->m_NextFree;
            inst->m_Next = next_free;
            inst->m_Prev = last;
            inst->m_Object = object;
            out_instance_handles[i] = (HInstance *) inst;

            last = inst;
            inst = next_free;
            if(!inst)
            {
                assert(false && "out of buffer memory");
                return OUT_OF_MEMORY;
            }
        }

        if(object->m_InstanceHead)
            object->m_InstanceHead->m_Prev = last;
        last->m_Next = object->m_InstanceHead;
        object->m_InstanceHead = first;
        object->m_NextFreeInstance = inst;
        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result RemoveInstances(HInstance *instace_array[], uint32_t count)
    {
        assert(instace_array);
        if(!count)
            return SUCCESS;

        for(uint32_t i = 0; i < count; i++)
        {
            Instance *inst = (Instance *) instace_array[i];
            Object *object = inst->m_Object;
            inst->m_NextFree = object->m_NextFreeInstance;
            object->m_NextFreeInstance = inst;

            if(inst->m_Next)
                inst->m_Next->m_Prev = inst->m_Prev;
            if(inst->m_Prev)
                inst->m_Prev->m_Next = inst->m_Next;
        }
    
        return SUCCESS;
    }

    ////////////////////////////////////////////////////
    Result Update(HContext context_handle, uint32_t predicate, uint32_t flags)
    {
        Context *context = (Context *) context_handle;
        context->m_Desc.m_BeginFunc(&context->m_Desc);
    
        if(!context->m_Objects.Empty())
        {
            // do predicated update
            uint32_t container_mask = context->m_ContainerMask;
            if((container_mask & predicate) && (!(flags & UPDATE_FLAG_NON_PREDICATED_ONLY)))	
            {
                
                size_t object_count = context->m_Objects.Size();
                dmHashTable<uint32_t, class Object *>::Entry *entry, *first_entry, *base_entry;
                base_entry = context->m_Objects.GetFirstEntry();
                const void *data_array[const_reserve];

                // parse predication
                for(uint32_t index = 0; index < 32; index ++)
                {
                    // predicate
                    if(!((container_mask & predicate) & (1<<index)))
                        continue;

                    // begin container
                    Container *container = context->m_PredicatedContainers[index];
                    ContainerDesc *container_desc = &container->m_Desc;
                    container_desc->m_BeginFunc(container_desc);

                    first_entry = entry = base_entry;
                    for(uint32_t i = 0; i < object_count; i++)
                    {
                        Object *object = entry->m_Value;

                        if(object->m_Desc.m_ContainerMask & (1<<index))
                        {
                            // begin object
                            ObjectDesc *object_desc = &object->m_Desc;
                            object_desc->m_BeginFunc(container_desc, object_desc);

                            // update instances
                            Instance *inst = object->m_InstanceHead;
                            if(inst)
                            {
                                uint32_t inst_count = 0;
                                while(inst)
                                {
                                    data_array[inst_count++] = inst->m_Data;
                                    inst = inst->m_Next;
                                }
                                object_desc->m_UpdateFunc(container_desc, object_desc, data_array, inst_count);
                            }

                            // end object
                            object_desc->m_EndFunc(container_desc, object_desc);
                        }

                        // get next object
                        if(entry->m_Next != 0xffff)
                            entry = &entry[entry->m_Next];
                        else
                            entry = ++first_entry;
                    }

                    // end container
                    container_desc->m_EndFunc(container_desc);
                }

            }


            // do unpredicated update
            if(!(flags & UPDATE_FLAG_PREDICATED_ONLY))	
            {
                dmArray<class Container *> &containers = context->m_Containers;
                if(!containers.Empty())
                {
                    const void *data_array[const_reserve];

                    for(uint32_t c = 0; c < containers.Size(); c++)
                    {
                        Container &container = *containers[c];
                        dmArray<class Object *> &objects = container.m_Objects;
                        if(objects.Empty())
                            continue;

                        // begin container
                        ContainerDesc *container_desc = &container.m_Desc;
                        container_desc->m_BeginFunc(container_desc);
                        for(uint32_t i = 0; i < objects.Size(); i++)
                        {
                            // begin object
                            Object *object = objects[i];
                            ObjectDesc *object_desc = &object->m_Desc;
                            object_desc->m_BeginFunc(container_desc, object_desc);

                            // update instances
                            Instance *inst = object->m_InstanceHead;
                            if(inst)
                            {
                                uint32_t inst_count = 0;
                                while(inst)
                                {
                                    data_array[inst_count++] = inst->m_Data;
                                    inst = inst->m_Next;
                                }
                                object_desc->m_UpdateFunc(container_desc, object_desc, data_array, inst_count);
                            }

                            // end object
                            object_desc->m_EndFunc(container_desc, object_desc);
                        }

                        // end container
                        container_desc->m_EndFunc(container_desc);
                    }
                }
            }
        }

        context->m_Desc.m_EndFunc(&context->m_Desc);

        return SUCCESS;
    }
}
