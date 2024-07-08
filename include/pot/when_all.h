#pragma once

namespace pot
{

    template<typename Iterator>
    void when_all(Iterator begin, Iterator end)
        requires std::forward_iterator<Iterator> &&
            requires(typename std::iterator_traits<Iterator>::value_type future) { future.get(); }
    {
        for (auto it = begin; it != end; ++it)
            (*it).get();
    }

    template<template<class, class... > class Container, typename FutureType, typename... OtherTypes>
    void when_all(Container<FutureType, OtherTypes...>& futures)
        requires requires(FutureType& future) { future.get(); }
    {
        return when_all(std::begin(futures), std::end(futures));
    }
}