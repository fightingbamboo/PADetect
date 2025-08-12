#ifndef MY_META_H
#define MY_META_H

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <typeinfo>

// C++17 std::any compatibility
#if __cplusplus >= 201703L
    #include <any>
#else
    // Fallback for pre-C++17: use void* with type erasure
    #include <memory>
    #include <type_traits>
    
    namespace std {
        class any {
        private:
            struct holder_base {
                virtual ~holder_base() = default;
                virtual std::unique_ptr<holder_base> clone() const = 0;
                virtual const std::type_info& type() const = 0;
            };
            
            template<typename T>
            struct holder : holder_base {
                T value;
                holder(T&& v) : value(std::forward<T>(v)) {}
                std::unique_ptr<holder_base> clone() const override {
                    return std::make_unique<holder<T>>(value);
                }
                const std::type_info& type() const override {
                    return typeid(T);
                }
            };
            
            std::unique_ptr<holder_base> ptr;
            
        public:
            any() = default;
            
            template<typename T>
            any(T&& value) : ptr(std::make_unique<holder<typename std::decay<T>::type>>(std::forward<T>(value))) {}
            
            any(const any& other) : ptr(other.ptr ? other.ptr->clone() : nullptr) {}
            
            any& operator=(const any& other) {
                if (this != &other) {
                    ptr = other.ptr ? other.ptr->clone() : nullptr;
                }
                return *this;
            }
            
            any(any&&) = default;
            any& operator=(any&&) = default;
            
            template<typename T>
            T& any_cast() {
                auto* h = dynamic_cast<holder<T>*>(ptr.get());
                if (!h) throw std::runtime_error("bad any cast");
                return h->value;
            }
            
            template<typename T>
            const T& any_cast() const {
                auto* h = dynamic_cast<const holder<T>*>(ptr.get());
                if (!h) throw std::runtime_error("bad any cast");
                return h->value;
            }
            
            const std::type_info& type() const {
                return ptr ? ptr->type() : typeid(void);
            }
            
            bool has_value() const { return ptr != nullptr; }
        };
        
        template<typename T>
        T any_cast(const any& a) {
            return a.any_cast<T>();
        }
        
        template<typename T>
        T any_cast(any& a) {
            return a.any_cast<T>();
        }
    }
#endif

class MyMeta {
public:
    // 安全的设置方法
    template <typename T>
    void set(const std::string& key, T&& value) {
        m_values[key] = std::forward<T>(value);
    }

    // 核心的默认值获取模板（保持通用性）
    template <typename T>
    T getOr(const std::string& key, T defaultValue) const noexcept {
        return getOrDefaultImpl(key, std::forward<T>(defaultValue));
    }

    // ==== 类型安全的默认值获取方法 ====

    // int32_t 类型的默认值获取
    int32_t getInt32OrDefault(const std::string& key, int32_t defaultValue) const noexcept {
        return getOrDefaultImpl<int32_t>(key, defaultValue);
    }

    // int64_t 类型的默认值获取
    int64_t getInt64OrDefault(const std::string& key, int64_t defaultValue) const noexcept {
        return getOrDefaultImpl<int64_t>(key, defaultValue);
    }

    // double 类型的默认值获取
    double getDoubleOrDefault(const std::string& key, double defaultValue) const noexcept {
        return getOrDefaultImpl<double>(key, defaultValue);
    }

    // bool 类型的默认值获取
    bool getBoolOrDefault(const std::string& key, bool defaultValue) const noexcept {
        return getOrDefaultImpl<bool>(key, defaultValue);
    }

    // std::string 类型的默认值获取
    std::string getStringOrDefault(const std::string& key, const std::string& defaultValue) const noexcept {
        return getOrDefaultImpl<std::string>(key, defaultValue);
    }

    // 宽字符串类型的默认值获取
    std::wstring getWStringOrDefault(const std::string& key, const std::wstring& defaultValue) const noexcept {
        return getOrDefaultImpl<std::wstring>(key, defaultValue);
    }

    // 其他显式类型获取
    int32_t getInt32(const std::string& key) const { return get<int32_t>(key); }
    int64_t getInt64(const std::string& key) const { return get<int64_t>(key); }
    double getDouble(const std::string& key) const { return get<double>(key); }
    bool getBool(const std::string& key) const { return get<bool>(key); }
    std::string getString(const std::string& key) const { return get<std::string>(key); }
    std::wstring getWString(const std::string& key) const { return get<std::wstring>(key); }

    // 键存在检查
    bool contains(const std::string& key) const {
        return m_values.find(key) != m_values.end();
    }

    // 类型检查方法 (新增)
    template <typename T>
    bool isType(const std::string& key) const {
        auto it = m_values.find(key);
        return (it != m_values.end()) && (it->second.type() == typeid(T));
    }

    // 移除键值对
    void remove(const std::string& key) {
        m_values.erase(key);
    }

    // 清空所有数据
    void clear() {
        m_values.clear();
    }

    // 获取值类型信息 (调试用)
    const std::type_info& type(const std::string& key) const {
        auto it = m_values.find(key);
        if (it == m_values.end()) {
            throw std::out_of_range("Key not found: " + key);
        }
        return it->second.type();
    }
public:
    MyMeta() = default;
    ~MyMeta() = default;
    // 禁止拷贝构造和赋值
    MyMeta(const MyMeta&) = delete;
    MyMeta& operator=(const MyMeta&) = delete;

    // 支持移动语义（可选）
    MyMeta(MyMeta&&) = default;
    MyMeta& operator=(MyMeta&&) = default;
private:
    // 内部实现函数（封装类型转换）
    template <typename T>
    T getOrDefaultImpl(const std::string& key, T defaultValue) const noexcept {
        auto it = m_values.find(key);
        if (it == m_values.end()) {
            return defaultValue;
        }

        try {
            return std::any_cast<T>(it->second);
        }
        catch (const std::runtime_error&) {
            return defaultValue;
        }
    }

    // 显式类型获取基础方法
    template <typename T>
    T get(const std::string& key) const {
        auto it = m_values.find(key);
        if (it == m_values.end()) {
            throw std::out_of_range("Key not found: " + key);
        }

        try {
            return std::any_cast<T>(it->second);
        }
        catch (const std::runtime_error& e) {
            throw std::runtime_error(
                "Type mismatch for key '" + key + "': " +
                e.what() + " (expected " + typeid(T).name() +
                ", actual " + it->second.type().name() + ")"
            );
        }
    }

    // 成员变量
    std::unordered_map<std::string, std::any> m_values;
};

#endif // MY_META_H