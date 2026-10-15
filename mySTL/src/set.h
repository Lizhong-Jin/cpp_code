#pragma once

#ifndef SET_H
#define SET_H


#include "impl/rb_tree.h"


namespace mystl {

    template <typename Key, typename Compare = mystl::less<Key>, 
            typename Allocator = allocator<Key>>
    class set {

        struct KeyOfValue {
            const Key& operator()(const Key& value) const noexcept {
                return value;
            }
        };

        using Base = rb_tree<Key, Key, KeyOfValue, Compare, Allocator>;

    public:
        using allocator_type = Allocator;
        using Value_alloc_type = typename allocator_traits<Allocator>::template rebind_alloc<Key>;
        using Traits = allocator_traits<Value_alloc_type>;
        using pointer = typename Traits::pointer;
        using const_pointer = typename Traits::const_pointer;

        using key_type = Key;
        using value_type = Key;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using key_compare = Compare;
        using value_compare = Compare;
        using reference = Value&;
        using const_reference = const Key&;
        using iterator = Base::const_iterator;
        using const_iterator = Base::const_iterator;
        using reverse_iterator = Base::const_reverse_iterator;
        using const_reverse_iterator = Base::const_reverse_iterator;

        using node_type = Base::node_type;
        using insert_return_type = Base::insert_return_type;

    protected:
        Base tree_;

    public:
        // *************************************************************************************
        // constructor
        constexpr set() : set(Compare()) {}

        explicit constexpr set(const Compare& comp, const Allocator& alloc = Allocator())
            : tree_(comp, alloc) {}

        explicit constexpr set(const Allocator& alloc) : set(Compare(), alloc) {}

        template <typename Input_Iterator>
        constexpr set(Input_Iterator first, Input_Iterator last, const Compare& comp = Compare(),
                    const Allocator& alloc = Allocator())
            : tree_(first, last, comp, alloc) {}

        template <typename Input_Iterator>
        constexpr set(Input_Iterator first, Input_Iterator last, const Allocator& alloc = Allocator())
            : tree_(first, last, alloc) {}

        constexpr set(const set& other) : tree_(other.tree_) {}

        constexpr set(const set& other, const Allocator& alloc) : tree_(other.tree_, alloc) {}

        constexpr set(set&& other) : tree_(mystl::move(other.tree_)) {}

        constexpr set(set&& other, const Allocator& alloc) : tree_(mystl::move(other.tree_), alloc) {}

        constexpr set(mystl::initializer_list<value_type> list, const Compare& comp = Compare(),
                    const Allocator& alloc = Allocator())
            : tree_(list, comp, alloc) {}

        constexpr set(mystl::initializer_list<value_type> list, const Allocator& alloc = Allocator())
            : tree_(list, alloc) {}


        // *************************************************************************************
        // destructor
        constexpr ~set() noexcept {
            ~tree_();
        }


    }; // class set

} // namespace mystl

#endif