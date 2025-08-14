#pragma once

#include <mutex>

namespace pot::sync
{
    template<template<typename MutexType = std::mutex> typename LockType, typename ObjectType>
    class lock_object_ptr
    {
        ObjectType* m_object;
        MutexType* m_mutex;
    public:
        lock_object() = default;
        explicit lock_object(ObjectType& object, MutexType& mutex) : m_object(&object), m_mutex(&mutex) {}

        ObjectType* operator->()
        {
            LockType lock(*m_mutex);
            return m_object;
        }
        const ObjectType* operator->() const
        {
            LockType lock(*m_mutex);
            return m_object;
        }
        ObjectType& operator*()
        {
            LockType lock(*m_mutex);
            return *m_object;
        }
        const ObjectType& operator*() const
        {
            LockType lock(*m_mutex);
            return *m_object;
        }
    };

    template<typename ObjectType, typename MutexType = std::mutex>
    class sync_object
    {
        ObjectType m_object;
        MutexType m_mutex;
    public:
        sync_object() = default;
        explicit sync_object(const ObjectType& object) : m_object(object) {}

        explicit sync_object(sync_object&& other)
        {
            std::lock_guard<MutexType> lock(other.m_mutex);
            m_object = std::move(other.m_object);
        }
        sync_object& operator=(sync_object&& other)
        {
            if (this != &other)
            {
                std::scoped_lock<MutexType> lock(m_mutex, other.m_mutex);
                m_object = std::move(other.m_object);
            }
            return *this;
        }

        sync_object(const sync_object&) = delete;
        sync_object& operator=(const sync_object&) = delete;

        template<template<typename> typename LockType>
        auto lock() -> lock_object_ptr<LockType<MutexType>, ObjectType>
        {
            return { m_object, m_mutex };
        }

        auto scoped() { return lock<std::scoped_lock>(); }
        auto unique() { return lock<std::unique_lock>(); }
        auto shared() { return lock<std::shared_lock>(); }


        // auto scoped() -> lock_object_ptr<std::scoped_lock<MutexType>, ObjectType>
        // {
        //     return { m_object, m_mutex };
        // }
        // auto unique() -> lock_object_ptr<std::unique_lock<MutexType>, ObjectType>
        // {
        //     return { m_object, m_mutex };
        // }
        // auto shared() -> lock_object_ptr<std::shared_lock<MutexType>, ObjectType>
        // {
        //     return { m_object, m_mutex };
        // }

    };
}