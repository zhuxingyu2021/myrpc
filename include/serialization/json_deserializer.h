#ifndef MYRPC_JSON_DESERIALIZER_H
#define MYRPC_JSON_DESERIALIZER_H

#include <type_traits>

#include <array>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <set>
#include <unordered_set>
#include <tuple>
#include <string>
#include <map>
#include <unordered_map>
#include <optional>
#include <memory>

#include "utils.h"
#include "debug.h"

#include "net/exception.h"
#include "buffer/buffer.h"

#include "serialization/placeholder.h"

namespace MyRPC{
class JsonDeserializer{
public:
    JsonDeserializer(ReadBuffer& sb): buffer(sb){}

    template<class T>
    using arithmetic_type =  typename std::enable_if_t<std::is_arithmetic_v<std::decay_t<T>>,void>;

    template<class T>
    arithmetic_type<T> Load(T& t){
        std::string s;
        if constexpr(std::is_same_v<std::decay_t<T>, float> || std::is_same_v<std::decay_t<T>, double>) {
            // 浮点类型
            buffer.ReadUntil<',', '}', ']'>(s);
            t = std::stod(s);
        }
        else if constexpr(std::is_same_v<std::decay_t<T>, bool>) {
            // 布尔类型
            buffer.ReadUntil<',', '}', ']'>(s);
            t = (s == "true");
        }
        else if constexpr(std::is_unsigned_v<std::decay_t<T>>) {
            // 无符号类型
            buffer.ReadUntil<',', '}', ']'>(s);
            t = std::stoll(s);
        }
        else{
            // 有符号类型
            buffer.ReadUntil<',', '}', ']'>(s);
            t = std::stoull(s);
        }
    }

    void Load(std::string& s){
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == '\"', throw JsonDeserializerException(buffer.GetPos()));
        buffer.ReadUntil<'\"'>(s);
        MYRPC_ASSERT_EXCEPTION(buffer.GetChar() == '\"', throw JsonDeserializerException(buffer.GetPos()));
    }

private:
    /**
     * @brief 读取json数组，用于反序列化vector, list等类型
     */
    template<class Tval, class T>
    inline void deserialize_like_vector_impl_(T& t){
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == '[', throw JsonDeserializerException(buffer.GetPos()));
        while(buffer.PeekCharF() != ']'){
            Tval elem;
            Load(elem);
            t.emplace_back(std::move(elem));
            if(buffer.PeekCharF() == ','){
                buffer.GetChar();
            }
        }
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == ']', throw JsonDeserializerException(buffer.GetPos()));
    }

    /**
     * @brief 读取json数组，用于反序列化set, unordered_set等类型
     */
    template<class Tval, class T>
    inline void deserialize_like_set_impl_(T& t){
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == '[', throw JsonDeserializerException(buffer.GetPos()));
        while(buffer.PeekCharF() != ']'){
            Tval elem;
            Load(elem);
            t.insert(std::move(elem));
            if(buffer.PeekCharF() == ','){
                buffer.GetChar();
            }
        }
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == ']', throw JsonDeserializerException(buffer.GetPos()));
    }

    template<class T>
    using isnot_string_type =  typename std::enable_if_t<(!std::is_same_v<std::decay_t<T>, std::string>)&&
                                                         (!std::is_same_v<std::decay_t<T>, std::string_view>)&&
                                                         (!std::is_same_v<std::decay_t<T>, std::wstring>)&&
                                                         (!std::is_same_v<std::decay_t<T>, std::wstring_view>),void>;

    /**
     * @brief 读取json对象的key-value对
     */
    template<class Tkey, class Tval>
    inline isnot_string_type<Tkey> deserialize_key_val_impl_(Tkey& key, Tval& value){
        std::string s;
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == '{', throw JsonDeserializerException(buffer.GetPos()));
        buffer.PeekCharF();
        buffer.PeekString(s, 5);
        MYRPC_ASSERT_EXCEPTION(s == "\"key\"", throw JsonDeserializerException(buffer.GetPos()));
        buffer.Forward(5);
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == ':', throw JsonDeserializerException(buffer.GetPos()));
        Load(key);
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == ',', throw JsonDeserializerException(buffer.GetPos()));
        buffer.PeekCharF();
        buffer.PeekString(s, 7);
        MYRPC_ASSERT_EXCEPTION(s == "\"value\"", throw JsonDeserializerException(buffer.GetPos()));
        buffer.Forward(7);
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == ':', throw JsonDeserializerException(buffer.GetPos()));
        Load(value);
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == '}', throw JsonDeserializerException(buffer.GetPos()));
    }

    /**
     * @brief 读取json对象的key-value对（当key为string）
     */
    template<class Tval>
    inline void deserialize_key_val_impl_(std::string& key, Tval& value){
        Load(key);
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == ':', throw JsonDeserializerException(buffer.GetPos()));
        Load(value);
    }

    /**
     * @brief 读取json对象，用于反序列化map, unordered_map等类型
     */
    template<class Tkey, class Tval, class T>
    inline void deserialize_like_map_impl_(T& t){
        auto c1 = buffer.GetCharF();
        MYRPC_ASSERT_EXCEPTION(c1 == '{' || c1 == '[', throw JsonDeserializerException(buffer.GetPos()));

        char c2 = buffer.PeekCharF();
        while(c2 != '}' && c2 != ']'){
            Tkey key;
            Tval val;
            deserialize_key_val_impl_(key, val);
            t.emplace(std::move(key), std::move(val));
            if(buffer.PeekCharF() == ','){
                buffer.GetChar();
            }
            c2 = buffer.PeekCharF();
        }

        auto c3 = buffer.GetCharF();
        MYRPC_ASSERT_EXCEPTION(c3 == '}' || c3 == ']', throw JsonDeserializerException(buffer.GetPos()));
    }

    /**
     * @brief 构造tuple的各个元素
     */
    template<class T>
    inline T make_tuple_internal_(){
        T t;
        Load(t);
        buffer.GetCharF();
        return t;
    }

    /**
     * @brief 读取json对象，用于反序列化tuple类型
     */
    template<class Tuple, size_t... Is>
    inline void deserialize_tuple_impl_(Tuple& t,std::index_sequence<Is...>){
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == '[', throw JsonDeserializerException(buffer.GetPos()));
        ((std::get<Is>(t) = std::move(make_tuple_internal_<std::tuple_element_t<Is, Tuple>>())), ...);
    }

public:
    /**
     * 以下是反序列化各种STL容器类型/智能指针的模板函数
     */

    template<class T, size_t Num>
    void Load(std::array<T, Num>& t){
        deserialize_like_vector_impl_<T>(t);
    }

    template<class T>
    void Load(std::vector<T>& t){
        deserialize_like_vector_impl_<T>(t);
    }

    template<class T>
    void Load(std::deque<T>& t){
        deserialize_like_vector_impl_<T>(t);
    }

    template<class T>
    void Load(std::list<T>& t){
        deserialize_like_vector_impl_<T>(t);
    }

    template<class T>
    void Load(std::forward_list<T>& t){
        deserialize_like_vector_impl_<T>(t);
    }

    template<class T>
    void Load(std::set<T>& t){
        deserialize_like_set_impl_<T>(t);
    }

    template<class T>
    void Load(std::unordered_set<T>& t){
        deserialize_like_set_impl_<T>(t);
    }

    template<class ...Args>
    void Load(std::tuple<Args...>& t){
        deserialize_tuple_impl_(t, std::index_sequence_for<Args...>{});
    }

    template<class Tkey, class Tval>
    void Load(std::map<Tkey, Tval>& t){
        deserialize_like_map_impl_<Tkey, Tval>(t);
    }

    template<class Tkey, class Tval>
    void Load(std::multimap<Tkey, Tval>& t){
        deserialize_like_map_impl_<Tkey, Tval>(t);
    }

    template<class Tkey, class Tval>
    void Load(std::unordered_map<Tkey, Tval>& t){
        deserialize_like_map_impl_<Tkey, Tval>(t);
    }

    template<class Tkey, class Tval>
    void Load(std::unordered_multimap<Tkey, Tval>& t){
        deserialize_like_map_impl_<Tkey, Tval>(t);
    }

    template<class Tkey, class Tval>
    void Load(std::pair<Tkey, Tval>& t){
        char c1 = buffer.GetCharF();
        MYRPC_ASSERT_EXCEPTION(c1 == '{' || c1 == '[', throw JsonDeserializerException(buffer.GetPos()));

        Tkey key;
        Tval val;
        deserialize_key_val_impl_(key, val);
        t.first = std::move(key);
        t.second = std::move(val);

        char c3 = buffer.GetCharF();
        MYRPC_ASSERT_EXCEPTION(c3 == '}' || c3 == ']', throw JsonDeserializerException(buffer.GetPos()));
    }

    template<class T>
    void Load(std::optional<T>& t){
        std::string s;
        buffer.PeekCharF();
        buffer.PeekString(s, 4);
        if(s == "null"){
            buffer.Forward(4);
            t = std::nullopt;
        }
        else{
            T val;
            Load(val);
            t.emplace(std::move(val));
        }
    }

    template<class T>
    void Load(std::shared_ptr<T>& t){
        std::string s;
        t = std::move(std::make_shared<T>());
        buffer.PeekCharF();
        buffer.PeekString(s, 4);
        if(s == "null")
            buffer.Forward(4);
        else
            Load(*t);
    }

    template<class T>
    void Load(std::unique_ptr<T>& t){
        std::string s;
        t = std::move(std::make_unique<T>());
        buffer.PeekCharF();
        buffer.PeekString(s, 4);
        if(s == "null")
            buffer.Forward(4);
        else
            Load(*t);
    }

private:
    ReadBuffer& buffer;

public:
    /**
     * @brief 以下是针对struct的反序列化
     */

    inline void deserialize_struct_begin_impl_(){
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == '{', throw JsonDeserializerException(buffer.GetPos()));
    }
    inline void deserialize_struct_end_impl_(){
    }

    template<class T>
    inline void deserialize_item_impl_(const std::string_view key, T& val){
        std::string key_str;

        deserialize_key_val_impl_(key_str, val);
        MYRPC_ASSERT_EXCEPTION(key == key_str, throw JsonDeserializerException(buffer.GetPos()));

        char c = buffer.GetCharF();
        MYRPC_ASSERT_EXCEPTION(c == ',' || c == '}', throw JsonDeserializerException(buffer.GetPos()));
    }

    inline void deserialize_item_key_beg_impl_(const std::string_view key){
        std::string key_str;

        Load(key_str);
        MYRPC_ASSERT_EXCEPTION(buffer.GetCharF() == ':', throw JsonDeserializerException(buffer.GetPos()));
        MYRPC_ASSERT_EXCEPTION(key == key_str, throw JsonDeserializerException(buffer.GetPos()));
    }

    inline void deserialize_item_key_end_impl_(){
        char c = buffer.GetCharF();
        MYRPC_ASSERT_EXCEPTION(c == ',' || c == '}', throw JsonDeserializerException(buffer.GetPos()));
    }

    template<class T>
    using struct_class_type =  typename std::enable_if_t<(std::is_class_v<std::decay_t<T>>)&&
                                                         (!std::is_same_v<std::decay_t<T>, std::string>)&&
                                                         (!std::is_same_v<std::decay_t<T>, std::string_view>),void>;

    template<class T>
    struct_class_type<T> Load(T& t) {
        t.Load(*this);
    }
};
}

#define LOAD_BEGIN_DEF void Load(MyRPC::JsonDeserializer& deserializer){
#define LOAD_BEGIN_READ deserializer.deserialize_struct_begin_impl_();
#define LOAD_BEGIN LOAD_BEGIN_DEF LOAD_BEGIN_READ

#define LOAD_ITEM(x) deserializer.deserialize_item_impl_(#x, x);
#define LOAD_ALIAS_ITEM(alias, x) deserializer.deserialize_item_impl_(#alias, x);

#define LOAD_KEY_BEG(alias) deserializer.deserialize_item_key_beg_impl_( #alias);

#define LOAD_KEY_END deserializer.deserialize_item_key_end_impl_();

#define LOAD_END_READ deserializer.deserialize_struct_end_impl_();
#define LOAD_END_DEF }
#define LOAD_END LOAD_END_READ LOAD_END_DEF

#endif //MYRPC_JSON_DESERIALIZER_H