#pragma once

#include <boost/describe.hpp>

template <typename T>
concept DescribeableStruct =
    boost::describe::has_describe_bases<T>::value &&
    boost::describe::has_describe_members<T>::value && !std::is_union_v<T>;

template <typename T>
concept DescribeableEnum = boost::describe::has_describe_enumerators<T>::value;
