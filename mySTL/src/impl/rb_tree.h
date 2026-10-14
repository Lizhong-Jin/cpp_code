#pragma once

#ifndef RB_TREE_H
#define RB_TREE_H

#include <cassert>
#include <cstddef>
#include <limits>
#include <optional>

#include "../utility.h"
#include "../functional.h"
#include "../iterator.h"
#include "../memory.h"
#include "../stdexcept.h"
#include "../initializer_list.h"

namespace mystl {

    enum class Color {RED, BLACK};

    // Store the color and pointers of the rb node
    struct rb_node_base {
        Color color;
        rb_node_base* parent;
        rb_node_base* left;
        rb_node_base* right;

        constexpr rb_node_base() : rb_node_base(Color::BLACK, nullptr, nullptr, nullptr) {}

        constexpr rb_node_base(Color c) : rb_node_base(c, nullptr, nullptr, nullptr) {}

        constexpr rb_node_base(Color c, rb_node_base* _parent, rb_node_base* _left, rb_node_base* _right) 
            : color(c), parent(_parent), left(_left), right(_right) {}

        constexpr rb_node_base(const rb_node_base& other) 
            : color(other.color), parent(other.parent), left(other.left), right(other.right) {}

        constexpr rb_node_base(rb_node_base&& other) 
            : color(other.color), parent(other.parent), left(other.left), right(other.right) {}

        constexpr void copy_ptr(rb_node_base& other) noexcept {
            parent = other.parent;
            left = other.left;
            right = other.right;
        }
    }; // class rb_node_base

    // Basic node in rb tree
    // Only responsible for constructing the value. 
    // The pointers and colors are constructed by the rb tree.
    template<typename Value>
    struct rb_node : rb_node_base {
        using value_type = Value;

        value_type value;

        template<class... Args>
            requires mystl::is_constructible_v<Value, Args&&...>
        explicit constexpr rb_node(Args&&... args)
            : rb_node_base(), value(mystl::forward<Args>(args)...) {}

        rb_node(const rb_node&) = delete;
        rb_node(rb_node&&) = delete;
        rb_node& operator=(const rb_node&) = delete;
        rb_node& operator=(rb_node&&) = delete;
    }; // calss rb_node

    template<typename Key, typename Value, typename KeyOfValue, typename Compare, typename Allocator>
    class rb_tree;

    template<typename Value, typename Allocator>
    class rb_node_handle {
        template<typename, typename, typename, typename, typename>
        friend class rb_tree;

        using alloc_traits = mystl::allocator_traits<Allocator>;
        using raw_node = rb_node<Value>;

        using node_allocator = typename alloc_traits::template rebind_alloc<raw_node>;
        using node_traits = mystl::allocator_traits<node_allocator>;

        static_assert(mystl::is_same_v<typename Allocator::value_type, Value>);
        static_assert(mystl::is_same_v<typename node_traits::pointer, raw_node*>);

        std::optional<Allocator> alloc_;
        raw_node* ptr_ = nullptr;

        // 仅允许 rb_tree 接管一个已经摘除、断开链接的节点。
        rb_node_handle(raw_node* p, const Allocator& alloc) noexcept
            : alloc_(alloc), ptr_(p)
        {
            assert(p && !p->parent && !p->left && !p->right);
        }

        // 使用自己的 allocator 销毁节点。
        // 保留 alloc_，供移动赋值判断 allocator 是否传播。
        void destroy_node() noexcept {
            if (!ptr_) return;

            node_allocator alloc(*alloc_);
            node_traits::destroy(alloc, ptr_);
            node_traits::deallocate(alloc, ptr_, 1);
            ptr_ = nullptr;
        }

        raw_node* get() const noexcept {
            return ptr_;
        }

        // 只转移所有权，不销毁节点。
        [[nodiscard]] raw_node* release() noexcept {
            auto* p = ptr_;
            ptr_ = nullptr;
            alloc_.reset();
            return p;
        }

    public:
        using value_type = Value;
        using allocator_type = Allocator;

        rb_node_handle() noexcept = default;

        rb_node_handle(const rb_node_handle&) = delete;
        rb_node_handle& operator=(const rb_node_handle&) = delete;

        rb_node_handle(rb_node_handle&& other) noexcept
            : alloc_(mystl::move(other.alloc_)),
            ptr_(other.ptr_)
        {
            other.ptr_ = nullptr;
            other.alloc_.reset();
        }

        rb_node_handle& operator=(rb_node_handle&& other) noexcept {
            if (this == &other)
                return *this;

            constexpr bool propagate =
                alloc_traits::propagate_on_container_move_assignment::value;

            // 目标没有 allocator，或允许传播，或双方 allocator 相等。
            assert(!alloc_ || propagate || (other.alloc_ && *alloc_ == *other.alloc_));

            // 必须先用旧 allocator 释放旧节点。
            destroy_node();

            if (!alloc_ || propagate)
                alloc_ = mystl::move(other.alloc_);

            ptr_ = other.ptr_;

            other.ptr_ = nullptr;
            other.alloc_.reset();
            return *this;
        }

        ~rb_node_handle() noexcept {
            destroy_node();
        }

        [[nodiscard]] bool empty() const noexcept {
            return ptr_ == nullptr;
        }

        explicit operator bool() const noexcept {
            return !empty();
        }

        value_type& value() const noexcept {
            assert(ptr_);
            return ptr_->value;
        }

        allocator_type get_allocator() const noexcept {
            assert(ptr_ && alloc_);
            return *alloc_;
        }
    };


    // rb tree
    // header_ is always be RED
    // The root of non-empty tree is always be BLACK.
    // root->parent == &header_
    // header_.parent == root
    template <typename Key, typename Value, typename KeyOfValue, 
        typename Compare = mystl::less<Key>, typename Allocator = mystl::allocator<Value>>
    class rb_tree {

        static_assert(is_same_v<remove_cv_t<Key>, Key> && is_same_v<remove_cv_t<Value>, Value>,
                    "rb tree must have a non_const and non_volatile key and value type");
        static_assert(is_same_v<typename Allocator::value_type, Value>,
                    "rb tree must have the same value type as its allocator");

    public:
        // *************************************************************************************
        // tree_iterator
        template<bool IsConst>
        class tree_iterator {
            friend class rb_tree;
            template<bool> friend class tree_iterator;
            
            using internal_node_type = rb_node<Value>;
            using base_pointer = mystl::conditional_t<IsConst, const rb_node_base*, rb_node_base*>;
            using value_node_pointer = mystl::conditional_t<IsConst, const internal_node_type*, internal_node_type*>;

            base_pointer node_ = nullptr;

            explicit tree_iterator(base_pointer p) noexcept : node_(p) {}

        public:
            using iterator_category = mystl::bidirectional_iterator_tag;
            using value_type = Value;
            using difference_type = std::ptrdiff_t;
            using pointer = mystl::conditional_t<IsConst, const Value*, Value*>;
            using reference = mystl::conditional_t<IsConst, const Value&, Value&>;

            tree_iterator() noexcept = default;

            template<bool C>
                requires (IsConst && !C)
            tree_iterator(const tree_iterator<C>& other) noexcept
                : node_(other.node_) {}

            reference operator*() const noexcept {
                assert(node_ && !is_header(node_));
                return static_cast<value_node_pointer>(node_)->value;
            }

            pointer operator->() const noexcept {
                return mystl::addressof(operator*());
            }

            tree_iterator& operator++() noexcept {
                assert(node_ && !is_header(node_));

                if (node_->right) {
                    node_ = minimum(node_->right);
                } else {
                    auto* parent = node_->parent;
                    while (!is_header(parent) && node_ == parent->right) {
                        node_ = parent;
                        parent = parent->parent;
                    }
                    node_ = parent;
                }
                return *this;
            }

            tree_iterator& operator--() noexcept {
                assert(node_);

                if (is_header(node_)) {
                    assert(node_->parent);
                    node_ = node_->right;
                } else if (node_->left) {
                    node_ = maximum(node_->left);
                } else {
                    auto* parent = node_->parent;
                    while (!is_header(parent) && node_ == parent->left) {
                        node_ = parent;
                        parent = parent->parent;
                    }
                    assert(!is_header(parent));
                    node_ = parent;
                }
                return *this;
            }

            tree_iterator operator++(int) noexcept {
                auto old = *this;
                ++*this;
                return old;
            }

            tree_iterator operator--(int) noexcept {
                auto old = *this;
                --*this;
                return old;
            }

            template<bool C>
            bool operator==(const tree_iterator<C>& other) const noexcept {
                return node_ == other.node_;
            }

            template<bool C>
            bool operator!=(const tree_iterator<C>& other) const noexcept {
                return !(*this == other);
            }
        }; // class tree_iterator
        // *************************************************************************************

    public:
        using internal_node_type = rb_node<Value>;
        using node_type = rb_node_handle<Value, Allocator>;

        using allocator_type = Allocator;
        using Value_alloc_type = typename allocator_traits<Allocator>::template rebind_alloc<Value>;
        using Traits = allocator_traits<Value_alloc_type>;
        using pointer = typename Traits::pointer;
        using const_pointer = typename Traits::const_pointer;

        using key_type = Key;
        using value_type = Value;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using key_compare = Compare;
        using value_compare = Compare;
        using reference = Value&;
        using const_reference = const Value&;
        using iterator = tree_iterator<false>;
        using const_iterator = tree_iterator<true>;
        using reverse_iterator = mystl::reverse_iterator<iterator>;
        using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

        struct insert_return_type {
            iterator position;
            bool inserted;
            node_type node;
        };
    
    private:
        using alloc_traits = mystl::allocator_traits<allocator_type>;
        using node_allocator = typename alloc_traits::template rebind_alloc<internal_node_type>;
        using node_alloc_traits = mystl::allocator_traits<node_allocator>;
        using node_pointer = typename node_alloc_traits::pointer;
        using const_node_pointer = typename node_alloc_traits::const_pointer;

        [[no_unique_address]] Compare comp_;
        [[no_unique_address]] KeyOfValue key_of_value_;
        [[no_unique_address]] node_allocator node_alloc_;

        rb_node_base header_;
        size_type size_ = 0;

    public:
        allocator_type get_allocator() const noexcept {
            return allocator_type(node_alloc_);
        }
    
        // *************************************************************************************
        // construct
        constexpr rb_tree() : rb_tree(Compare{}, allocator_type{}, KeyOfValue{}) {}

        explicit constexpr rb_tree(const allocator_type& alloc)
            : rb_tree(Compare{}, alloc, KeyOfValue{}) {}

        explicit constexpr rb_tree(const Compare& comp, const allocator_type& alloc = allocator_type{}, 
            const KeyOfValue& key_of_value = KeyOfValue{})
            : comp_(comp), key_of_value_(key_of_value), node_alloc_(alloc)
        {
            reset_empty();
        }

        constexpr rb_tree(const rb_tree& other)
            : rb_tree(other, alloc_traits::select_on_container_copy_construction(other.get_allocator())) {}

        constexpr rb_tree(const rb_tree& other, const allocator_type& alloc)
            : rb_tree(other.comp_, alloc, other.key_of_value_)
        {
            if (other.size_ > max_size())
                throw mystl::length_error("rb_tree copy exceeds max_size");
            copy_tree(other);
        }

        constexpr rb_tree(rb_tree&& other)
            noexcept(
                mystl::is_nothrow_copy_constructible_v<Compare> &&
                mystl::is_nothrow_copy_constructible_v<KeyOfValue> &&
                mystl::is_nothrow_move_constructible_v<node_allocator>)
            : comp_(other.comp_), key_of_value_(other.key_of_value_), node_alloc_(mystl::move(other.node_alloc_))
        {
            reset_empty();
            take_nodes_from(other);
        }

        constexpr rb_tree(rb_tree&& other, const allocator_type& alloc)
            : rb_tree(other.comp_, alloc, other.key_of_value_)
        {
            if (node_alloc_ == other.node_alloc_) {
                take_nodes_from(other);
                return;
            }
            if (other.empty())
                return;
            if (other.size_ > max_size())
                throw mystl::length_error("rb_tree move exceeds max_size");
            try {
                Guard_subtree guard(*this, clone_moving_subtree(other.header_.parent, &header_));
                auto* left = minimum(guard.root);
                auto* right = maximum(guard.root);

                header_.parent = guard.release();
                header_.left = left;
                header_.right = right;
                size_ = other.size_;
            }catch(...) {
                other.clear();
                throw;
            }
            other.clear();
        }

        template <typename Input_Iterator>
        constexpr rb_tree(Input_Iterator first, Input_Iterator last, const Compare& comp,
                        const Allocator& alloc, const KeyOfValue& key_of_value)
            : rb_tree(comp, alloc, key_of_value)
        {
            for (; first != last; ++first) {
                insert_unique(*first);
            }
        }

        template <typename Input_Iterator>
        constexpr rb_tree(Input_Iterator first, Input_Iterator last, const Compare& comp = Compare{},
                        const Allocator& alloc = Allocator())
            : rb_tree(first, last, comp, alloc, KeyOfValue{}) {}

        template <typename Input_Iterator>
        constexpr rb_tree(Input_Iterator first, Input_Iterator last, const Allocator& alloc)
            : rb_tree(first, last, Compare{}, alloc, KeyOfValue{}) {}


        constexpr rb_tree(mystl::initializer_list<value_type> list, const Compare& comp,
                        const Allocator& alloc, const KeyOfValue& key_of_value) 
            : rb_tree(comp, alloc, key_of_value)
        {
            for (auto iter = list.begin(); iter != list.end(); ++iter) {
                insert_unique(*iter);
            }
        }

        constexpr rb_tree(mystl::initializer_list<value_type> list, const Compare& comp = Compare{},
                        const Allocator& alloc = Allocator{}) 
            : rb_tree(list, comp, alloc, KeyOfValue{}) {}

        constexpr rb_tree(mystl::initializer_list<value_type> list, const Allocator& alloc) 
            : rb_tree(list, Compare{}, alloc, KeyOfValue{}) {}


        // *************************************************************************************
        // destruct
        constexpr ~rb_tree() noexcept {
            clear();
        }

        // *************************************************************************************
        // iterators
        constexpr iterator begin() noexcept {
            return iterator(header_.left); 
        }
        constexpr iterator end() noexcept {
            return iterator(&header_); 
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator(header_.left);
        }
        constexpr const_iterator end() const noexcept {
            return const_iterator(&header_);
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(header_.left);
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(&header_);
        }

        constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator(end()); 
        }
        constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(begin()); 
        }
        constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }
        constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }
        constexpr const_reverse_iterator crbegin() const noexcept {
            return const_reverse_iterator(end());
        }
        constexpr const_reverse_iterator crend() const noexcept {
            return const_reverse_iterator(begin());
        }

        // *************************************************************************************
        // capacity
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }

        [[nodiscard]] constexpr size_type max_size() const noexcept {
            return get_max_size();
        }

        // *************************************************************************************
        // operator ==
        constexpr rb_tree& operator=(const rb_tree& other) {
            if (this == &other) return *this;
            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value) {
                if (node_alloc_ != other.node_alloc_) {
                    clear();
                }
                comp_ = other.comp_;
                node_alloc_ = other.node_alloc_;
                key_of_value_ = other.key_of_value_;
            }
            copy_tree(other);
            return *this;
        }

        constexpr rb_tree& operator=(rb_tree&& other)
            noexcept((alloc_traits::propagate_on_container_move_assignment::value ||
                    alloc_traits::is_always_equal::value) && 
                    mystl::is_nothrow_copy_assignable_v<Compare> && 
                    mystl::is_nothrow_copy_assignable_v<KeyOfValue> && 
                    mystl::is_nothrow_move_assignable_v<node_allocator>)
        {
            if (this != &other) {
                move_assign(mystl::move(other), typename alloc_traits::propagate_on_container_move_assignment{});
            }
            return *this;
        }

        constexpr rb_tree& operator=(mystl::initializer_list<value_type> list) {
            clear();
            for (auto iter = list.begin(); iter != list.end(); ++iter) {
                insert_unique(*iter);
            }
            return *this;
        }


        // *************************************************************************************
        // modifiers
        constexpr void clear() noexcept {
            if (size_ == 0) return;
            delete_all(header_.parent);
            reset_empty();
        }

        // emplace_unique
        template <typename... Args>
        constexpr mystl::pair<iterator, bool> emplace_unique(Args&&... args) {
            Guard_subtree guard(*this, create_node(mystl::forward<Args>(args)...));
            
            if (size_ == 0) {
                auto* p = guard.release();
                p->color = Color::BLACK;
                p->parent = &header_;
                header_.parent = p;
                header_.left = p;
                header_.right = p;
                size_ = 1;
                return mystl::make_pair(iterator(p), true);
            }

            auto* p = guard.root;
            p->color = Color::RED;

            rb_node_base* node = header_.parent;
            rb_node_base* prev = &header_;
            bool insert_left;
            while (node != nullptr) {
                if (comp_(key_of_value_(p->value), key_of_value_(static_cast<internal_node_type*>(node)->value))) {
                    prev = node;
                    node = node->left;
                    insert_left = true;
                }else if (comp_(key_of_value_(static_cast<internal_node_type*>(node)->value), key_of_value_(p->value))) {
                    prev = node;
                    node = node->right;
                    insert_left = false;
                }else {
                    return mystl::make_pair(iterator(node), false);
                }
            }
            if (size_ >= max_size())
                throw mystl::length_error("rb_tree cannot be larger than max_size()");

            p = guard.release();
            p->parent = prev;

            if (insert_left) {
                prev->left = p;
                if (prev == header_.left)
                    header_.left = p;
            }else {
                prev->right = p;
                if (prev == header_.right)
                    header_.right = p;
            }
            ++size_;
            
            if (prev->color == Color::RED) {
                rebalance_after_insert(p);
            }
            return mystl::make_pair(iterator(p), true);
        }
        
    private:
        template <typename... Args>
        constexpr iterator insert_at_pos(const_iterator pos, Args&&... args) {
            if (size_ >= max_size())
                throw mystl::length_error("rb_tree cannot be larger than max_size()");

            Guard_subtree guard(*this, create_node(mystl::forward<Args>(args)...));
            auto* p = guard.release();
            p->color = Color::RED;
            ++size_;

            if (pos == cbegin()) {
                rb_node_base* old_begin = header_.left;
                old_begin->left = p;
                p->parent = old_begin;
                header_.left = p;
            }else if (pos == cend()) {
                rb_node_base* old_tail = header_.right;
                old_tail->right = p;
                p->parent = old_tail;
                header_.right = p;
            }else {
                iterator fa(const_cast<rb_node_base*>(pos.node_));
                if (pos.node_->left == nullptr) {
                    fa.node_->left = p;
                    p->parent = fa.node_;
                }else {
                    --fa;
                    fa.node_->right = p;
                    p->parent = fa.node_;
                }
            }
            if (p->parent->color == Color::RED) {
                rebalance_after_insert(p);
            }
            return iterator(p);
        }

        constexpr iterator insert_at_pos(const_iterator pos, internal_node_type* node) {
            if (size_ >= max_size())
                throw mystl::length_error("rb_tree cannot be larger than max_size()");

            node->color = Color::RED;
            ++size_;
            if (pos == cbegin()) {
                rb_node_base* old_begin = header_.left;
                old_begin->left = node;
                node->parent = old_begin;
                header_.left = node;
            }else if (pos == cend()) {
                rb_node_base* old_tail = header_.right;
                old_tail->right = node;
                node->parent = old_tail;
                header_.right = node;
            }else {
                iterator fa(const_cast<rb_node_base*>(pos.node_));
                if (pos.node_->left == nullptr) {
                    fa.node_->left = node;
                    node->parent = fa.node_;
                }else {
                    --fa;
                    fa.node_->right = node;
                    node->parent = fa.node_;
                }
            }
            if (node->parent->color == Color::RED) {
                rebalance_after_insert(node);
            }
            return iterator(node);
        }

        constexpr iterator insert_node_unique(internal_node_type* node) {
            if (size_ == 0) {
                node->color = Color::BLACK;
                node->parent = &header_;
                header_.parent = node;
                header_.left = node;
                header_.right = node;
                size_ = 1;
                return iterator(node);
            }

            node->color = Color::RED;
            rb_node_base* cur = header_.parent;
            rb_node_base* prev = &header_;
            bool insert_left;
            while (cur != nullptr) {
                if (comp_(key_of_value_(node->value), key_of_value_(static_cast<internal_node_type*>(cur)->value))) {
                    prev = cur;
                    cur = cur->left;
                    insert_left = true;
                }else if (comp_(key_of_value_(static_cast<internal_node_type*>(cur)->value), key_of_value_(node->value))) {
                    prev = cur;
                    cur = cur->right;
                    insert_left = false;
                }else {
                    return iterator(cur);
                }
            }
            if (size_ >= max_size())
                throw mystl::length_error("rb_tree cannot be larger than max_size()");

            node->parent = prev;

            if (insert_left) {
                prev->left = node;
                if (prev == header_.left)
                    header_.left = node;
            }else {
                prev->right = node;
                if (prev == header_.right)
                    header_.right = node;
            }
            ++size_;
            
            if (prev->color == Color::RED) {
                rebalance_after_insert(node);
            }
            return iterator(node);
        }

    public:
        // insert_unique
        constexpr mystl::pair<iterator, bool> insert_unique(const value_type& value) {
            return emplace_unique(value);
        }

        constexpr mystl::pair<iterator, bool> insert_unique(value_type&& value) {
            return emplace_unique(mystl::move(value));
        }

        constexpr iterator insert_unique(const_iterator pos, const value_type& value) {
            if (size_ == 0) {
                Guard_subtree guard(*this, create_node(value));
                auto* p = guard.release();
                p->color = Color::BLACK;
                p->parent = &header_;
                header_.parent = p;
                header_.left = p;
                header_.right = p;
                size_ = 1;
                return iterator(p);
            }
            if (pos == cbegin()) {
                if (comp_(key_of_value_(value), key_of_value_(*pos)))
                    return insert_at_pos(pos, value);
            }else {
                const_iterator prev = pos;
                --prev;
                if (comp_(key_of_value_(*prev), key_of_value_(value)) && 
                    (pos == cend() ? true : comp_(key_of_value_(value), key_of_value_(*pos)))) {
                    return insert_at_pos(pos, value);
                }
            }
            return emplace_unique(value).first;
        }

        constexpr iterator insert_unique(const_iterator pos, value_type&& value) {
            if (size_ == 0) {
                Guard_subtree guard(*this, create_node(mystl::move(value)));
                auto* p = guard.release();
                p->color = Color::BLACK;
                p->parent = &header_;
                header_.parent = p;
                header_.left = p;
                header_.right = p;
                size_ = 1;
                return iterator(p);
            }
            if (pos == cbegin()) {
                if (comp_(key_of_value_(value), key_of_value_(*pos)))
                    return insert_at_pos(pos, mystl::move(value));
            }else {
                const_iterator prev = pos;
                --prev;
                if (comp_(key_of_value_(*prev), key_of_value_(value)) && 
                    (pos == cend() ? true : comp_(key_of_value_(value), key_of_value_(*pos)))) {
                    return insert_at_pos(pos, mystl::move(value));
                }
            }
            return emplace_unique(mystl::move(value)).first;
        }

        constexpr insert_return_type insert_unique(node_type&& nh) {
            if (nh.empty()) {
                return {end(), false, node_type{}};
            }
            assert(get_allocator() == nh.get_allocator());
            auto* node = nh.get();
            iterator insert_pos = insert_node_unique(node);
            if (insert_pos.node_ == node) {
                (void)nh.release();
                return {insert_pos, true, node_type{}};
            }
            return {insert_pos, false, mystl::move(nh)};
        }

        constexpr iterator insert_unique(const_iterator pos, node_type&& nh) {
            if (nh.empty()) return end();
            assert(get_allocator() == nh.get_allocator());
            auto* node = nh.get();
            if (size_ == 0) {
                iterator insert_pos = insert_node_unique(node);
                (void)nh.release();
                return insert_pos;
            }
            if (pos == cbegin()) {
                if (comp_(key_of_value_(node->value), key_of_value_(*pos))) {
                    iterator insert_pos = insert_at_pos(pos, node);
                    (void)nh.release();
                    return insert_pos;
                }
            }else {
                const_iterator prev = pos;
                --prev;
                if (comp_(key_of_value_(*prev), key_of_value_(node->value)) && 
                    (pos == cend() ? true : comp_(key_of_value_(node->value), key_of_value_(*pos)))) {
                    iterator insert_pos = insert_at_pos(pos, node);
                    (void)nh.release();
                    return insert_pos;
                }
            }
            iterator insert_pos = insert_node_unique(node);
            if (insert_pos.node_ == node)
                (void)nh.release();
            return insert_pos;
        }

        // emplace_hint
        template <typename... Args>
        constexpr iterator emplace_hint(const_iterator hint, Args&&... args) {
            Guard_subtree guard(*this, create_node(mystl::forward<Args>(args)...));
            
            if (size_ == 0) {
                auto* p = guard.release();
                p->color = Color::BLACK;
                p->parent = &header_;
                header_.parent = p;
                header_.left = p;
                header_.right = p;
                size_ = 1;
                return iterator(p);
            }
            auto* p = guard.root;
            if (hint == cbegin()) {
                if (comp_(key_of_value_(p->value), key_of_value_(*hint))) {
                    auto result = insert_at_pos(hint, guard.root);
                    (void)guard.release();
                    return result;
                }
            }else {
                const_iterator prev = hint;
                --prev;
                if (comp_(key_of_value_(*prev), key_of_value_(p->value)) && 
                    (hint == cend() ? true : comp_(key_of_value_(p->value), key_of_value_(*hint)))) {
                    auto result = insert_at_pos(hint, guard.root);
                    (void)guard.release();
                    return result;
                }
            }
            iterator insert_pos = insert_node_unique(p);
            if (insert_pos.node_ == p)
                (void)guard.release();
            return insert_pos;
        }

        // erase
        constexpr iterator erase(const_iterator pos) {
            assert (size_ != 0 && pos != cend());
            auto* node = const_cast<rb_node_base*>(pos.node_);
            iterator post(node);
            ++post;
            erase_node(node);
            return post;
        }

        constexpr size_type erase(const Key& key) {
            auto pos = find(key);
            if (pos == end())
                return 0;
            erase(pos);
            return 1;
        }

        // extract
        constexpr node_type extract(const_iterator pos) {
            assert (size_ != 0 && pos != cend());
            auto* node = const_cast<rb_node_base*>(pos.node_);
            unlink_node(node);
            node->parent = nullptr;
            node->left = nullptr;
            node->right = nullptr;
            return node_type(static_cast<internal_node_type*>(node), get_allocator());
        }

        constexpr node_type extract(const Key& key) {
            auto pos = find(key);
            if (pos == end())
                return node_type();
            return extract(pos);
        }

        // swap
        constexpr void swap(rb_tree& other) 
            noexcept(alloc_traits::is_always_equal::value &&
                    mystl::is_nothrow_swappable_v<Compare> &&
                    mystl::is_nothrow_swappable_v<KeyOfValue>)
            requires (mystl::is_nothrow_swappable_v<Compare> &&
                      mystl::is_nothrow_swappable_v<KeyOfValue>)
        {
            if (this == &other) return;
            using mystl::swap;
            if constexpr (alloc_traits::propagate_on_container_swap::value) {
                swap(node_alloc_, other.node_alloc_);
            }else {
                assert(node_alloc_ == other.node_alloc_);
            }
            swap_tree(other);
            swap(comp_, other.comp_);
            swap(key_of_value_, other.key_of_value_);
        }

        // *************************************************************************************
        // lookup

        // find
        constexpr iterator find(const Key& key) {
            rb_node_base* node = header_.parent;
            while (node != nullptr) {
                auto& value = static_cast<internal_node_type*>(node)->value;
                if (comp_(key, key_of_value_(value))) {
                    node = node->left;
                }else if (comp_(key_of_value_(value), key)) {
                    node = node->right;
                }else {
                    return iterator(node);
                }
            }
            return end();
        }
        
        constexpr const_iterator find(const Key& key) const {
            const rb_node_base* node = header_.parent;
            while (node) {
                const auto& value = static_cast<const internal_node_type*>(node)->value;
                if (comp_(key, key_of_value_(value))) {
                    node = node->left;
                } else if (comp_(key_of_value_(value), key)) {
                    node = node->right;
                } else {
                    return const_iterator(node);
                }
            }
            return cend();
        }

        // count
        constexpr size_type count(const Key& key) const {
            return find(key) == cend() ? 0 : 1;
        }

        // contains
        constexpr bool contains(const Key& key) const {
            return find(key) != cend();
        }

        // lower_bound & upper_bound
        constexpr iterator lower_bound(const Key& key) {
            rb_node_base* node = header_.parent;
            rb_node_base* candidate = &header_;
            while (node) {
                const auto& value = static_cast<const internal_node_type*>(node)->value;
                if (!comp_(key_of_value_(value), key)) {
                    candidate = node;
                    node = node->left;
                }else {
                    node = node->right;
                }
            }
            return iterator(candidate);
        }

        constexpr const_iterator lower_bound(const Key& key) const {
            const rb_node_base* node = header_.parent;
            const rb_node_base* candidate = &header_;
            while (node) {
                const auto& value = static_cast<const internal_node_type*>(node)->value;
                if (!comp_(key_of_value_(value), key)) {
                    candidate = node;
                    node = node->left;
                }else {
                    node = node->right;
                }
            }
            return const_iterator(candidate);
        }

        constexpr iterator upper_bound(const Key& key) {
            rb_node_base* node = header_.parent;
            rb_node_base* candidate = &header_;
            while (node) {
                const auto& value = static_cast<const internal_node_type*>(node)->value;
                if (comp_(key, key_of_value_(value))) {
                    candidate = node;
                    node = node->left;
                }else {
                    node = node->right;
                }
            }
            return iterator(candidate);
        }

        constexpr const_iterator upper_bound(const Key& key) const {
            const rb_node_base* node = header_.parent;
            const rb_node_base* candidate = &header_;
            while (node) {
                const auto& value = static_cast<const internal_node_type*>(node)->value;
                if (comp_(key, key_of_value_(value))) {
                    candidate = node;
                    node = node->left;
                }else {
                    node = node->right;
                }
            }
            return const_iterator(candidate);
        }

        // equal_range
        constexpr mystl::pair<iterator, iterator> equal_range(const Key& key) {
            iterator lhs = lower_bound(key);
            if (lhs == end() || comp_(key, key_of_value_(*lhs))) {
                return mystl::make_pair(lhs, lhs);
            }
            iterator rhs = lhs;
            ++rhs;
            return mystl::make_pair(lhs, rhs); 
        }

        constexpr mystl::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
            const_iterator lhs = lower_bound(key);
            if (lhs == end() || comp_(key, key_of_value_(*lhs))) {
                return mystl::make_pair(lhs, lhs);
            }
            const_iterator rhs = lhs;
            ++rhs;
            return mystl::make_pair(lhs, rhs); 
        }

        // observers
        constexpr key_compare key_comp() const {
            return comp_;
        }
        


    private:
        static constexpr bool is_header(const rb_node_base* p) noexcept {
            return p &&
                p->color == Color::RED &&
                (!p->parent || p->parent->parent == p);
        }

        template<class... Args>
        constexpr internal_node_type* create_node(Args&&... args) {
            // 当前实现先支持普通指针 allocator
            static_assert(mystl::is_same_v<node_pointer, internal_node_type*>);

            auto* p = node_alloc_traits::allocate(node_alloc_, 1);
            try {
                node_alloc_traits::construct(node_alloc_, p, mystl::forward<Args>(args)...);
            } catch (...) {
                node_alloc_traits::deallocate(node_alloc_, p, 1);
                throw;
            }
            return p;
        }

        struct Guard_subtree {
            rb_tree& owner;
            internal_node_type* root;

            Guard_subtree(rb_tree& tree, internal_node_type* p) noexcept
                : owner(tree), root(p) {}

            Guard_subtree(const Guard_subtree&) = delete;
            Guard_subtree& operator=(const Guard_subtree&) = delete;
            Guard_subtree(Guard_subtree&&) = delete;
            Guard_subtree& operator=(Guard_subtree&&) = delete;

            ~Guard_subtree() noexcept {
                owner.delete_all(root);
            }

            [[nodiscard]] internal_node_type* release() noexcept {
                auto* p = root;
                root = nullptr;
                return p;
            }
        };

        constexpr void reset_empty() noexcept {
            header_.color = Color::RED;
            header_.parent = nullptr;
            header_.left = &header_;
            header_.right = &header_;
            size_ = 0;
        }
        
        // take over the node of other tree only when the current tree is empty
        constexpr void take_nodes_from(rb_tree& other) noexcept {
            if (other.size_ == 0) return;

            header_.parent = other.header_.parent;
            header_.left = other.header_.left;
            header_.right = other.header_.right;
            size_ = other.size_;

            header_.parent->parent = &header_;

            other.reset_empty();
        }

        constexpr void swap_tree(rb_tree& other) noexcept {
            using mystl::swap;
            rb_node_base* this_root = header_.parent;
            rb_node_base* other_root = other.header_.parent;
            if (this_root) {
                this_root->parent = &other.header_;
            }
            if (other_root) {
                other_root->parent = &header_;
            }
            swap(header_.parent, other.header_.parent);
            swap(header_.left, other.header_.left);
            swap(header_.right, other.header_.right);
            swap(size_, other.size_);
            
            if (!header_.parent)
                reset_empty();
            if (!other.header_.parent)
                other.reset_empty();
        }

        constexpr void copy_tree(const rb_tree& other) {
            if (this == &other) return;
            if (other.size_ == 0) {
                clear();
                return;
            }
            Guard_subtree guard(*this, clone_subtree(other.header_.parent, &header_));
            auto* left = minimum(guard.root);
            auto* right = maximum(guard.root);
            clear();

            header_.parent = guard.release();
            header_.left = left;
            header_.right = right;
            size_ = other.size_;
        }

        constexpr internal_node_type* clone_subtree(const rb_node_base* src, rb_node_base* parent) {
            if (src == nullptr) return nullptr;
            const auto* source = static_cast<const internal_node_type*>(src);

            Guard_subtree guard(*this, create_node(source->value));
            auto* p = guard.root;

            p->color = src->color;
            p->parent = parent;
            p->left = clone_subtree(src->left, p);
            p->right = clone_subtree(src->right, p);

            return guard.release();
        }

        constexpr internal_node_type* clone_moving_subtree(rb_node_base* src, rb_node_base* parent){
            if (!src) return nullptr;

            auto* source = static_cast<internal_node_type*>(src);

            Guard_subtree guard(*this, create_node(mystl::move(source->value)));

            auto* p = guard.root;
            p->parent = parent;
            p->color = src->color;

            p->left = clone_moving_subtree(src->left, p);
            p->right = clone_moving_subtree(src->right, p);

            return guard.release();
        }

        // move_assign
        constexpr void move_assign(rb_tree&& other, true_type) {
            clear();
            comp_ = other.comp_;
            node_alloc_ = mystl::move(other.node_alloc_);
            key_of_value_ = other.key_of_value_;
            swap_tree(other);
        }

        constexpr void move_assign(rb_tree&& other, false_type) {
            comp_ = other.comp_;
            key_of_value_ = other.key_of_value_;
            clear();
            if (node_alloc_ == other.node_alloc_) {
                swap_tree(other);
            }else {
                if (other.empty()) 
                    return;
                if (other.size() > max_size())
                    throw mystl::length_error("rb_tree cannot be larger than max_size()");
                try {
                    Guard_subtree guard(*this, clone_moving_subtree(other.header_.parent, &header_));
                    auto* left = minimum(guard.root);
                    auto* right = maximum(guard.root);

                    header_.parent = guard.release();
                    header_.left = left;
                    header_.right = right;
                    size_ = other.size_;
                }catch(...) {
                    other.clear();
                    throw;
                }
                other.clear();
            }
        }

        // erase one node in rb_tree
        constexpr void erase_node(rb_node_base* node) {
            unlink_node(node);
            auto* actual = static_cast<internal_node_type*>(node);
            node_alloc_traits::destroy(node_alloc_, actual);
            node_alloc_traits::deallocate(node_alloc_, actual, 1);
        }

        // unlink_node
        constexpr void unlink_node(rb_node_base* node) {
            if (size_ == 1) {
                reset_empty();
                return;
            }
            rb_node_base* new_begin = nullptr;
            rb_node_base* new_tail = nullptr;
            if (header_.left == node) {
                iterator next(node);
                new_begin = (++next).node_;
            }
            if (header_.right == node) {
                iterator prev(node);
                new_tail = (--prev).node_;
            }
            // Move a node with two children to its successor's position.
            if (node->left && node->right) {
                rb_node_base* succeed_node = minimum(node->right);
                swap_with_successor(node, succeed_node);
                mystl::swap(node->color, succeed_node->color);
            }
            // if the node to be deleted has only one non-null child,
            // then this node must be black and its child must be red.
            // simply replace it with the child node.
            if (node->left || node->right) {
                rb_node_base* sub_node = node->left ? node->left : node->right;
                sub_node->color = Color::BLACK;
                transplant(node, sub_node);
            }
            // if node is a leaf.
            else {
                // if node's color is red
                if (node->color == Color::RED) {
                    if (node->parent->left == node) {
                        node->parent->left = nullptr;
                    }else {
                        node->parent->right = nullptr;
                    }
                }else {
                    // mark node as double black
                    rb_node_base* double_black = node;
                    // adjust the rb_tree and remove the double black mark
                    while (double_black != header_.parent && double_black->color == Color::BLACK) {
                        rb_node_base* parent = double_black->parent;
                        if (parent->left == double_black) {
                            rb_node_base* bro = parent->right;
                            if (bro && bro->color == Color::RED) {
                                bro->color = Color::BLACK;
                                parent->color = Color::RED;
                                left_rotate(parent);
                                bro = parent->right;
                            }
                            if ((!bro->left || bro->left->color == Color::BLACK) 
                                && (!bro->right || bro->right->color == Color::BLACK)){
                                if (parent->color == Color::RED) {
                                    parent->color = Color::BLACK;
                                    bro->color = Color::RED;
                                    double_black = header_.parent;
                                }else {
                                    bro->color = Color::RED;
                                    double_black = parent;
                                }
                            }else {
                                if (!bro->right || bro->right->color == Color::BLACK) {
                                    bro->color = Color::RED;
                                    bro->left->color = Color::BLACK;
                                    right_rotate(bro);
                                    bro = parent->right;
                                }
                                bro->color = parent->color;
                                parent->color = Color::BLACK;
                                bro->right->color = Color::BLACK;
                                left_rotate(parent);
                                double_black = header_.parent;
                            }
                        }else {
                            rb_node_base* bro = parent->left;
                            if (bro && bro->color == Color::RED) {
                                bro->color = Color::BLACK;
                                parent->color = Color::RED;
                                right_rotate(parent);
                                bro = parent->left;
                            }
                            if ((!bro->left || bro->left->color == Color::BLACK) 
                                && (!bro->right || bro->right->color == Color::BLACK)){
                                if (parent->color == Color::RED) {
                                    parent->color = Color::BLACK;
                                    bro->color = Color::RED;
                                    double_black = header_.parent;
                                }else {
                                    bro->color = Color::RED;
                                    double_black = parent;
                                }
                            }else {
                                if (!bro->left || bro->left->color == Color::BLACK) {
                                    bro->color = Color::RED;
                                    bro->right->color = Color::BLACK;
                                    left_rotate(bro);
                                    bro = parent->left;
                                }
                                bro->color = parent->color;
                                parent->color = Color::BLACK;
                                bro->left->color = Color::BLACK;
                                right_rotate(parent);
                                double_black = header_.parent;
                            }
                        }
                    }
                    if (node->parent->left == node) {
                        node->parent->left = nullptr;
                    }else {
                        node->parent->right = nullptr;
                    }
                    header_.parent->color = Color::BLACK;
                }
            }
            --size_;
            if (new_begin) {
                header_.left = new_begin;
            }else if (new_tail) {
                header_.right = new_tail;
            }
            return;
        }


        static constexpr rb_node_base* minimum(rb_node_base* p) noexcept {
            while (p->left)
                p = p->left;
            return p;
        }

        static constexpr rb_node_base* maximum(rb_node_base* p) noexcept {
            while (p->right)
                p = p->right;
            return p;
        }

        constexpr void transplant(rb_node_base* old_node, rb_node_base* replacement) noexcept {
            auto* parent = old_node->parent;

            if (parent == &header_)
                header_.parent = replacement;
            else if (parent->left == old_node)
                parent->left = replacement;
            else
                parent->right = replacement;

            if (replacement)
                replacement->parent = parent;
        }

        // Only for erase: node has two children, successor == minimum(node->right).
        constexpr void swap_with_successor(rb_node_base* node, rb_node_base* successor) noexcept {
            assert(node && node->left && node->right);
            assert(successor == minimum(node->right));

            auto* node_left = node->left;
            auto* node_right = node->right;
            auto* successor_parent = successor->parent;
            auto* successor_right = successor->right;

            transplant(node, successor);
            if (successor_parent == node) {
                // The successor is the direct right child: node becomes its right child.
                successor->right = node;
                node->parent = successor;
            } else {
                // A deeper successor is always its parent's left child.
                successor_parent->left = node;
                node->parent = successor_parent;
                successor->right = node_right;
                node_right->parent = successor;
            }

            successor->left = node_left;
            node_left->parent = successor;
            node->left = nullptr;
            node->right = successor_right;
            if (successor_right)
                successor_right->parent = node;
        }

        // *************************************************************************************
        // rotate
        constexpr void left_rotate(rb_node_base* node) noexcept {
            rb_node_base* parent = node->parent;
            rb_node_base* right_son = node->right;

            node->right = right_son->left;
            node->parent = right_son;

            if (right_son->left != nullptr) {
                right_son->left->parent = node;
            }
            right_son->left = node;
            right_son->parent = parent;

            if (parent == &header_) {
                header_.parent = right_son;
            }else if (node == parent->left) {
                parent->left = right_son;
            }else {
                parent->right = right_son;
            }
        }

        constexpr void right_rotate(rb_node_base* node) noexcept {
            rb_node_base* parent = node->parent;
            rb_node_base* left_son= node->left;

            node->left = left_son->right;
            node->parent = left_son;

            if (left_son->right != nullptr) {
                left_son->right->parent = node;
            }
            left_son->right = node;
            left_son->parent = parent;

            if (parent == &header_) {
                header_.parent = left_son;
            }else if (node == parent->left) {
                parent->left = left_son;
            }else {
                parent->right = left_son;
            }
        }

        // *************************************************************************************
        // rebalance
        constexpr void rebalance_after_insert(rb_node_base* node) noexcept {
            while (node != header_.parent && node->parent->color == Color::RED) {
                if (node->parent->parent->left == node->parent) {
                    rb_node_base* uncle = node->parent->parent->right;
                    if (uncle && uncle->color == Color::RED) {
                        uncle->color = Color::BLACK;
                        node->parent->color = Color::BLACK;
                        node->parent->parent->color = Color::RED;
                        node = node->parent->parent;
                    }else {
                        if (node->parent->right == node) {
                            node = node->parent;
                            left_rotate(node);
                        }
                        node->parent->color = Color::BLACK;
                        node->parent->parent->color = Color::RED;
                        right_rotate(node->parent->parent);
                    }
                }else {
                    rb_node_base* uncle = node->parent->parent->left;
                    if (uncle && uncle->color == Color::RED) {
                        uncle->color = Color::BLACK;
                        node->parent->color = Color::BLACK;
                        node->parent->parent->color = Color::RED;
                        node = node->parent->parent;
                    }else {
                        if (node->parent->left == node) {
                            node = node->parent;
                            right_rotate(node);
                        }
                        node->parent->color = Color::BLACK;
                        node->parent->parent->color = Color::RED;
                        left_rotate(node->parent->parent);
                    }
                }
            }
            header_.parent->color = Color::BLACK;
        }


        constexpr void delete_all(rb_node_base* node) noexcept {
            if (node == nullptr)
                return;
            delete_all(node->left);
            delete_all(node->right);

            auto* actual = static_cast<internal_node_type*>(node);
            node_alloc_traits::destroy(node_alloc_, actual);
            node_alloc_traits::deallocate(node_alloc_, actual, 1);
        }

        constexpr size_type get_max_size() const noexcept {
            const size_type diff_limit = static_cast<size_type>((std::numeric_limits<difference_type>::max)());
            const size_type alloc_limit = node_alloc_traits::max_size(node_alloc_);
            return alloc_limit < diff_limit ? alloc_limit : diff_limit;
        }

    

    }; // class rb_tree



} // namespace mystl

#endif// RB_TREE.H
