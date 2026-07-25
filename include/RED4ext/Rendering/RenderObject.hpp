#pragma once

#include <RED4ext/Platform.hpp>

#include <RED4ext/Memory/Utils.hpp>

namespace RED4ext
{
class IRenderObject
{
public:
    template<std::derived_from<IRenderObject> T>
    friend class TRenderPtr;

    using AllocatorType = Memory::RenderDataAllocator;

    virtual Memory::IAllocator* GetAllocator()
    {
        return AllocatorType::Get();
    }

    virtual void Destroy()
    {
        if (this)
        {
            Memory::Delete(this);
        }
    }

    virtual ~IRenderObject() = default;

protected:
    void Release()
    {
        if (Detail::Platform::AtomicAddFetch(&m_refCount, static_cast<uint32_t>(-1)) == 0)
        {
            Destroy();
        }
    }

    void AddRef()
    {
        Detail::Platform::AtomicAddFetch(&m_refCount, 1u);
    }

private:
    uint32_t m_refCount{1};

    // Explicit trailing padding, deliberately named rather than left implicit.
    //
    // Without it the four bytes after m_refCount are tail padding, and the Itanium ABI
    // reuses a base class's tail padding for the first member of a derived class while
    // MSVC does not. That silently places CRenderMesh::quantizationScale at offset 0x0C
    // instead of 0x10 on non-MSVC compilers -- and the size assertions cannot catch it,
    // because the explicit unkXX[] arrays further down re-anchor the later members and
    // keep sizeof() correct.
    //
    // Naming the padding makes the layout identical on both ABIs. sizeof(IRenderObject)
    // is unchanged at 0x10, so this is inert on Windows.
    uint32_t unk0C{0};
};
RED4EXT_ASSERT_SIZE(IRenderObject, 0x10);

template<std::derived_from<IRenderObject> T = IRenderObject>
class TRenderPtr
{
public:
    TRenderPtr() = default;

    TRenderPtr(std::nullptr_t) noexcept
    {
    }

    explicit TRenderPtr(T* aPointer) noexcept
        : m_instance(aPointer)
    {
    }

    TRenderPtr(const TRenderPtr& aOther) noexcept
        : m_instance(aOther.m_instance)
    {
        if (m_instance)
        {
            m_instance->AddRef();
        }
    }

    TRenderPtr(TRenderPtr&& aOther) noexcept
    {
        Swap(aOther);
    }

    ~TRenderPtr()
    {
        Release();
    }

    explicit operator bool() const noexcept
    {
        return m_instance != nullptr;
    }

    TRenderPtr& operator=(const TRenderPtr& aRhs) noexcept
    {
        TRenderPtr(aRhs).Swap(*this);
        return *this;
    }

    TRenderPtr& operator=(TRenderPtr&& aRhs) noexcept
    {
        Swap(aRhs);
        return *this;
    }

    T* operator->() const noexcept
    {
        return m_instance;
    }

    T& operator*() const noexcept
    {
        return *m_instance;
    }

    void Swap(TRenderPtr& aOther) noexcept
    {
        std::swap(m_instance, aOther.m_instance);
    }

    T* GetPtr() const noexcept
    {
        return m_instance;
    }

private:
    void Release()
    {
        if (m_instance)
        {
            m_instance->Release();
        }
    }

    T* m_instance{nullptr};
};
RED4EXT_ASSERT_SIZE(TRenderPtr<>, 0x8);
} // namespace RED4ext
