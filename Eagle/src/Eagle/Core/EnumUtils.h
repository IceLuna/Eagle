#pragma once

#include <type_traits>

template <typename EnumType>
struct IsEnumFlags
{
    static const bool value = false;
};

#define DECLARE_FLAGS(EnumType)      \
template <>                          \
struct IsEnumFlags<EnumType>         \
{                                    \
    static const bool value = true;  \
};    

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, EnumType>::type operator|(EnumType lhs, EnumType rhs)
{
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    return static_cast<EnumType>(static_cast<UnderlyingType>(lhs) | static_cast<UnderlyingType>(rhs));
}

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, EnumType>::type operator&(EnumType lhs, EnumType rhs)
{
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    return static_cast<EnumType>(static_cast<UnderlyingType>(lhs) & static_cast<UnderlyingType>(rhs));
}

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, EnumType>::type operator^(EnumType lhs, EnumType rhs) {
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    return static_cast<EnumType>(static_cast<UnderlyingType>(lhs) ^ static_cast<UnderlyingType>(rhs));
}

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, EnumType>::type& operator|=(EnumType& lhs, EnumType rhs)
{
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    lhs = static_cast<EnumType>(static_cast<UnderlyingType>(lhs) | static_cast<UnderlyingType>(rhs));
    return lhs;
}

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, EnumType>::type& operator&=(EnumType& lhs, EnumType rhs)
{
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    lhs = static_cast<EnumType>(static_cast<UnderlyingType>(lhs) & static_cast<UnderlyingType>(rhs));
    return lhs;
}

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, EnumType>::type& operator^=(EnumType& lhs, EnumType rhs) {
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    lhs = static_cast<EnumType>(static_cast<UnderlyingType>(lhs) ^ static_cast<UnderlyingType>(rhs));
    return lhs;
}

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, EnumType>::type operator~(EnumType rhs) {
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    return static_cast<EnumType>(~static_cast<UnderlyingType>(rhs));
}

template <typename EnumType>
typename std::enable_if<IsEnumFlags<EnumType>::value, bool>::type HasFlags(EnumType lhs, EnumType rhs) {
    return (lhs & rhs) == rhs;
}
