#pragma once
#include <cstddef>
#include <memory>
template <typename T> static constexpr size_t DefaultAlignment=std::max(std::max(alignof(T), sizeof(uint64_t)), alignof(void *));
struct ErasedType {
  void (*destroy)(void*);
  void (*constructMove)(void*src, void*dst);
  void (*constructCopy)(const void*src, void*dst);
  size_t (*size)();
  size_t (*alignment)();
};

template<class T>
static constexpr ErasedType erasedOpsFor = {
    [](void* ptr) {
      std::launder(static_cast<T*>(ptr))->~T();
    },
    [](void* src, void* dst)->void {
      new (dst) T(std::move(*static_cast<T*>(src)));
    },
    [](const void* src, void* dst)->void {
      new (dst) T(*static_cast<const T*>(src));
    },
    []() -> size_t {
      return sizeof(T);
    },
    []() -> size_t {
      return alignof(T);
    }
};
enum class StorageType {
  Empty,
  Inplace,
  //Allocated
};
template <typename T, size_t align> static constexpr size_t DataAlignment=align>0?align:DefaultAlignment<T>;
// template <size_t baseAlign> static constexpr size_t MinAlignment=std::max(std::max(baseAlign, alignof(StorageType)), alignof(ErasedType *));

// template <size_t baseAlign> static constexpr size_t MinAlignment=std::max(std::max(baseAlign, alignof(StorageType)), alignof(ErasedType *));

#include <utility>
#include <type_traits>

template <typename F>
class ScopeExit {
  F func_;
  bool active_=true;

  public:
  // Constructor takes a callable that will run on destruction
  explicit ScopeExit(F&& f) noexcept(std::is_nothrow_move_constructible_v<F>)
      : func_(std::forward<F>(f))
  {}

  ScopeExit(ScopeExit&& other)=delete;

  ScopeExit(const ScopeExit&) = delete;
  ScopeExit& operator=(const ScopeExit&) = delete;

  ~ScopeExit() noexcept {
    if (active_) {
      try {
        func_();
      } catch (...) {
        // Swallow exceptions: scope_exit shouldn't throw
      }
    }
  }

  void dismiss() noexcept {
    active_ = false;
  }
};

// Helper to deduce type
template <typename F>
ScopeExit<F> make_scope_exit(F&& f) {
  return ScopeExit<F>(std::forward<F>(f));
}


template <typename Base, size_t size, size_t align=0, template<class> typename allocator=std::allocator> class InplaceAnyCopy {
public:
  InplaceAnyCopy() {}
  InplaceAnyCopy(const InplaceAnyCopy &other) {
    _copy_from(other);
  }
  InplaceAnyCopy(InplaceAnyCopy &&other) {
    _move_from(std::move(other));
  }
  template <typename T> requires std::derived_from<T, Base> InplaceAnyCopy(T &&object) {
    emplace<T>(std::move(object));
  }
  template <typename T> requires std::derived_from<T, Base> InplaceAnyCopy(const T &object) {
    emplace<T>(object);
  }
  ~InplaceAnyCopy() { _internal_free(); }
  template <typename Type> void emplace(auto &&...Args);
  size_t allocated_size() { return m_type?m_type->size():0; }
  explicit operator bool() { return m_storageType!=StorageType::Empty; }
  InplaceAnyCopy &operator =(const InplaceAnyCopy &source) {
    if(&source!=this)
      _copy_from(source);
    return *this;
  }
  InplaceAnyCopy &operator =(InplaceAnyCopy &&source) {
    if(&source!=this)
      _move_from(std::move(source));
    return *this;
  }
  constexpr Base *data();
  constexpr Base &operator *() { return *data(); }
  constexpr Base *operator ->() { return data(); }
  constexpr const Base *data() const;
  constexpr const Base &operator *() const { return *data(); }
  constexpr const Base *operator ->() const { return data(); }
protected:
  //template <typename Base2, size_t size2, size_t align=0, template<class> typename allocator=std::allocator>;
  constexpr void _copy_from(const InplaceAnyCopy &other);
  constexpr void _move_from(InplaceAnyCopy &&other);
  void _internal_free();
  constexpr const void *_raw_data() const;
  alignas(align>0?align:DefaultAlignment<Base>) std::byte m_storage[size];
  StorageType m_storageType=StorageType::Empty;
  const ErasedType *m_type=nullptr;
};
template <typename Base, size_t size, size_t align, template<class> typename allocator> template <typename Type> void InplaceAnyCopy<Base, size, align, allocator>::emplace(auto &&...args) {
  static_assert(std::derived_from<Type, Base>, "Constructed Type must be derived from Base");
  static_assert(alignof(Type)<=DataAlignment<Base, align>, "Constructed Type alignment does not match");
  static_assert(std::constructible_from<Type, decltype(args)...>, "Invalid arguments for constructor of Type");
  _internal_free();
  //printf("About to emplace new!\n");
  //new (&m_storage) Type(std::forward<decltype(args)> args...);
  if constexpr(sizeof(Type)<=sizeof(m_storage)) {
    ::new (&m_storage) Type(std::forward<decltype(args)>(args)...);
    m_storageType=StorageType::Inplace;
    m_type=&erasedOpsFor<Type>;
  }
  else {
    static_assert(false, "Dynamic allocation not supported (for now)");
  }
};

template <typename Base, size_t size, size_t align, template<class> typename allocator>
constexpr void InplaceAnyCopy<Base, size, align, allocator>::_copy_from(const InplaceAnyCopy &source)
{
  _internal_free();
  StorageType storageType=source.m_storageType;
  switch(storageType) {
  case StorageType::Inplace:
    source.m_type->constructCopy(source._raw_data(), m_storage);
    m_storageType=StorageType::Inplace;
    m_type=source.m_type;
    source._internal_free();
    break;
  case StorageType::Empty:
    break;
  }
}

template <typename Base, size_t size, size_t align, template<class> typename allocator>
constexpr void InplaceAnyCopy<Base, size, align, allocator>::_move_from(InplaceAnyCopy &&source)
{
  _internal_free();
  StorageType storageType=source.m_storageType;
  switch(storageType) {
  case StorageType::Inplace:
    source.m_type->constructMove(const_cast<void *>(source._raw_data()), m_storage);
    m_storageType=StorageType::Inplace;
    m_type=source.m_type;
    source._internal_free();
    break;
  case StorageType::Empty:
    break;
  }
}

// template <typename Base, size_t size, size_t align, template<class> typename allocator>
// inline InplaceAnyCopy<Base, size, align, allocator> &InplaceAnyCopy<Base, size, align, allocator>::operator =(InplaceAnyCopy &&source)
// {
//   if(&source!=this) {
//     _internal_free();
//     if(source.m_storageType==StorageType::Inplace) {
//       source.m_type->constructMove(&source.m_storage, m_storage);
//       m_storageType=StorageType::Inplace;
//       m_type=source.m_type;
//       source._internal_free();
//     }
//     else if(source.m_storageType==StorageType::Allocated) {
//       // TODO
//     }
//   }
//   return *this;
// }

template <typename Base, size_t size, size_t align, template<class> typename allocator> void InplaceAnyCopy<Base, size, align, allocator>::_internal_free() {
  switch(m_storageType)
  {
  case StorageType::Inplace:
  {
    ScopeExit finally([this]() noexcept {
      //printf("Setting empty content\n");
      m_storageType=StorageType::Empty;
      m_type=nullptr;
    });
    m_type->destroy(&m_storage);
    //printf("Delete...\n");
  }
  break;
  case StorageType::Empty:
    break;
  }
}


template <typename Base, size_t size, size_t align, template<class> typename allocator> constexpr const void *InplaceAnyCopy<Base, size, align, allocator>::_raw_data() const {
  switch(m_storageType) {
  case StorageType::Inplace:
    return &m_storage;
  default:
    return nullptr;
  }
}

template <typename Base, size_t size, size_t align, template<class> typename allocator> constexpr Base *InplaceAnyCopy<Base, size, align, allocator>::data() {
  void *raw=const_cast<void *>(_raw_data());
  return raw?std::launder(static_cast<Base *>(raw)):nullptr;
}
template <typename Base, size_t size, size_t align, template<class> typename allocator> constexpr const Base *InplaceAnyCopy<Base, size, align, allocator>::data() const {
  const void *raw=_raw_data();
  return raw?std::launder(static_cast<const Base *>(raw)):nullptr;
}
