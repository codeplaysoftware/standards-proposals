class buffer_allocator { int i; };
class custom_allocator {};
class context {};

namespace prop {

struct svm {};
struct use_host_ptr {};
struct context_bound {
  context_bound(context &c)
    : m_context(c) {}

  context get_context() { return m_context; }

  context m_context;
};

}

enum class __property_enum {
  allocator = 0,
  use_host_ptr = 1,
  context_bound = 2,
  svm = 3
};

template <typename propT>
struct __get_property_enum {
  static const __property_enum value = __property_enum::allocator;
};

template <>
struct __get_property_enum<prop::use_host_ptr> {
  static const __property_enum value = __property_enum::use_host_ptr;
};

template <>
struct __get_property_enum<prop::context_bound> {
  static const __property_enum value = __property_enum::context_bound;
};

template <>
struct __get_property_enum<prop::svm> {
  static const __property_enum value = __property_enum::svm;
};

template <typename dataT, int kElems, typename allocatorT = buffer_allocator>
class buffer {
public:
  template <typename... propTN>
  buffer(propTN... properties) {
    process_properties(properties...);
  }

  template <typename propT, typename... propTN>
  void process_properties(propT prop, propTN... properties) {
    constexpr property_enum e = __get_property_enum<propT>::value;
    m_hasProperty[static_cast<int>(e)] = true;
    process_property(prop);
    process_properties(properties...);
  }

  void process_properties() {
  }

  void process_property(prop::context_bound prop) {
    m_context = prop.get_context();
  }

  void process_property(prop::use_host_ptr prop) {
  }

  void process_property(prop::svm prop) {
  }

  void process_property(allocatorT prop) {
    m_allocator = prop;
  }

  template <typename propT>
  bool has_property() {
    constexpr property_enum e = __get_property_enum<propT>::value;
    return m_hasProperty[static_cast<int>(e)];
    }

private:

  bool m_hasProperty[4];
  context m_context;
  allocatorT m_allocator;
};

int main () {

  context myContext;

  std::vector<buffer<int, 1>> bufferList;

  bufferList.push_back(buffer<int, 1>());
  bufferList.push_back(buffer<int, 1>(prop::use_host_ptr()));
  bufferList.push_back(buffer<int, 1>(prop::context_bound(myContext)));
  bufferList.push_back(buffer<int, 1>(prop::svm()));
  bufferList.push_back(buffer<int, 1>(buffer_allocator{}, prop::context_bound(myContext)));

  std::vector<buffer<int, 1, std::allocator<int>>> customBufferList;
  customBufferList.push_back(buffer<int, 1, std::allocator<int>>());
  customBufferList.push_back(buffer<int, 1, std::allocator<int>>(prop::use_host_ptr()));

  for(auto& buf : bufferList) {
    if (buf.has_property<prop::use_host_ptr>()) {
      // ...
    } else if (buf.has_property<prop::svm>()) {
      // ...
    } else if (buf.has_property<prop::context_bound>()) {
      // ...
    }
  }
 }