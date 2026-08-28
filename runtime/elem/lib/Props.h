#pragma once

#include <initializer_list>
#include <optional>
#include <type_traits>
#include <utility>

#include "elem/Value.h"

namespace elem::lib {
    // Wraps a field type to make it mandatory in a DEFINE_PROPS_STRUCT: omitting
    // it from aggregate init falls back to value-init, which is ill-formed here
    // because the default constructor is deleted.
    template <typename T>
    struct Required {
        T value;
        Required() = delete;
        Required(T v) : value(std::move(v)) {}

        // Lets container-typed fields (e.g. Required<js::NumberArray>) be brace-initialized
        // directly, e.g. `.seq = {1.0, 2.0, 3.0}`, instead of needing `.seq = {{1.0, 2.0, 3.0}}`.
        template <typename U, typename = std::enable_if_t<std::is_constructible_v<T, std::initializer_list<U>>>>
        Required(std::initializer_list<U> il) : value(il) {}

        operator T&() { return value; }
        operator const T&() const { return value; }
    };

    namespace detail {
        template <typename T>
        struct is_optional : std::false_type {};
        template <typename T>
        struct is_optional<std::optional<T>> : std::true_type {};

        template <typename T>
        struct is_required : std::false_type {};
        template <typename T>
        struct is_required<Required<T>> : std::true_type {};

        template <typename T>
        void insertPropField(js::Object& obj, const char* name, T&& value) {
            using Decayed = std::decay_t<T>;
            if constexpr (is_optional<Decayed>::value) {
                if (value.has_value()) {
                    obj.insert({name, *std::forward<T>(value)});
                }
            } else if constexpr (is_required<Decayed>::value) {
                obj.insert({name, std::move(value.value)});
            } else {
                obj.insert({name, std::forward<T>(value)});
            }
        }
    }

    #define ELEM_DECLARE_FIELD(name, type) type name;
    #define ELEM_INSERT_FIELD(name, type) elem::lib::detail::insertPropField(jsProps, #name, std::move(name));

    // Counts total variadic args (2 per field)
    #define ELEM_ARG_COUNT(...) ELEM_ARG_COUNT_IMPL(__VA_ARGS__, \
        20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
    #define ELEM_ARG_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,N,...) N

    #define ELEM_CONCAT_(a, b) a##b
    #define ELEM_CONCAT(a, b) ELEM_CONCAT_(a, b)

    #define ELEM_FOR_EACH_PAIR_2(action, name, type)  action(name, type)
    #define ELEM_FOR_EACH_PAIR_4(action, name, type, ...)  action(name, type) ELEM_FOR_EACH_PAIR_2(action, __VA_ARGS__)
    #define ELEM_FOR_EACH_PAIR_6(action, name, type, ...)  action(name, type) ELEM_FOR_EACH_PAIR_4(action, __VA_ARGS__)
    #define ELEM_FOR_EACH_PAIR_8(action, name, type, ...)  action(name, type) ELEM_FOR_EACH_PAIR_6(action, __VA_ARGS__)
    #define ELEM_FOR_EACH_PAIR_10(action, name, type, ...) action(name, type) ELEM_FOR_EACH_PAIR_8(action, __VA_ARGS__)
    #define ELEM_FOR_EACH_PAIR_12(action, name, type, ...) action(name, type) ELEM_FOR_EACH_PAIR_10(action, __VA_ARGS__)
    #define ELEM_FOR_EACH_PAIR_14(action, name, type, ...) action(name, type) ELEM_FOR_EACH_PAIR_12(action, __VA_ARGS__)
    // Extend with _16, _18, ... if a Props struct ever needs more fields than this.

    #define ELEM_FOR_EACH_PAIR(action, ...) \
        ELEM_CONCAT(ELEM_FOR_EACH_PAIR_, ELEM_ARG_COUNT(__VA_ARGS__))(action, __VA_ARGS__)

    // Declares a struct with the given name/type pairs plus a takeJsObject() method
    // that moves each field into a js::Object, skipping unset std::optional fields.
    // takeJsObject() consumes the struct's fields, so it can only be called once.
    #define DEFINE_PROPS_STRUCT(StructName, ...)                    \
        struct StructName {                                        \
            ELEM_FOR_EACH_PAIR(ELEM_DECLARE_FIELD, __VA_ARGS__)     \
            js::Object takeJsObject() {                            \
                js::Object jsProps;                                 \
                ELEM_FOR_EACH_PAIR(ELEM_INSERT_FIELD, __VA_ARGS__)  \
                return jsProps;                                     \
            }                                                       \
        };
}
