#pragma once

#ifndef SET_H
#define SET_H


#include "impl/rb_tree.h"


namespace mystl {

    template <typename Key, typename Compare = mystl::less<Key>, 
            typename Allocator = allocator<Key>>
    class set {

        using Base = rb_tree<Key, Key, Key, Compare, Allocator>;
        

    }; // class set

} // namespace mystl

#endif