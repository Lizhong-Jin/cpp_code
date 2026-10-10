#pragma once

#ifndef RB_TREE_H
#define RB_TREE_H

#include <cassert>
#include <cstddef>
#include <limits>

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
    }; // class rb_node_base

    // Basic node in rb tree
    // Only responsible for constructing the value. 
    // The pointers and colors are constructed by the rb tree.
    template<class Value>
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


    // rb tree
    // header_ is always RED
    // The root of non-empty tree is always BLACK.
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
            
            using node_type = rb_node<Value>;
            using base_pointer = mystl::conditional_t<IsConst, const rb_node_base*, rb_node_base*>;
            using value_node_pointer = mystl::conditional_t<IsConst, const node_type*, node_type*>;

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
    
    private:
        template <typename Iterator, typename Node_Type>
        struct _insert_return {
            Iterator position;
            bool inserted;
            Node_Type node;
        };

    public:
        using node_type = rb_node<Value>;
        template <typename Iterator, typename Node_Type>
        using insert_return_type = _insert_return<Iterator, Node_Type>;

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
    
    private:
        using alloc_traits = mystl::allocator_traits<allocator_type>;
        using node_allocator = typename alloc_traits::template rebind_alloc<node_type>;
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
                Guard_subtree guard(*this, clone_moving_subtree(other));
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
                if (comp_(key_of_value_(p->value), key_of_value_(static_cast<node_type*>(node)->value))) {
                    prev = node;
                    node = node->left;
                    insert_left = true;
                }else if (comp_(key_of_value_(static_cast<node_type*>(node)->value), key_of_value_(p->value))) {
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
                iterator fa = pos;
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

    public:
        // insert_unique
        constexpr mystl::pair<iterator, bool> insert_unique(const value_type& value) {
            return emplace_unique(value);
        }

        constexpr mystl::pair<iterator, bool> insert_unique(value_type&& value) {
            return emplace_unique(mystl::move(value));
        }

        constexpr iterator insert_unique(const_iterator pos, const value_type& value) {
            if (size_ >= max_size())
                throw mystl::length_error("rb_tree cannot be larger than max_size()");
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
            if (size_ >= max_size())
                throw mystl::length_error("rb_tree cannot be larger than max_size()");
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


        



    private:
        static bool is_header(const rb_node_base* p) noexcept {
            return p &&
                p->color == Color::RED &&
                (!p->parent || p->parent->parent == p);
        }

        template<class... Args>
        node_type* create_node(Args&&... args) {
            // 当前实现先支持普通指针 allocator
            static_assert(mystl::is_same_v<node_pointer, node_type*>);

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
            node_type* root;

            Guard_subtree(rb_tree& tree, node_type* p) noexcept
                : owner(tree), root(p) {}

            Guard_subtree(const Guard_subtree&) = delete;
            Guard_subtree& operator=(const Guard_subtree&) = delete;
            Guard_subtree(Guard_subtree&&) = delete;
            Guard_subtree& operator=(Guard_subtree&&) = delete;

            ~Guard_subtree() noexcept {
                owner.delete_all(root);
            }

            [[nodiscard]] node_type* release() noexcept {
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

        constexpr node_type* clone_subtree(const rb_node_base* src, rb_node_base* parent) {
            if (src == nullptr) return nullptr;
            const auto* source = static_cast<const node_type*>(src);

            Guard_subtree guard(*this, create_node(source->value));
            auto* p = guard.root;

            p->color = src->color;
            p->parent = parent;
            p->left = clone_subtree(src->left, p);
            p->right = clone_subtree(src->right, p);

            return guard.release();
        }

        constexpr node_type* clone_moving_subtree(rb_node_base* src, rb_node_base* parent){
            if (!src) return nullptr;

            auto* source = static_cast<node_type*>(src);

            Guard_subtree guard(*this, create_node(mystl::move(source->value)));

            auto* p = guard.root;
            p->parent = parent;
            p->color = src->color;

            p->left = clone_moving_subtree(src->left, p);
            p->right = clone_moving_subtree(src->right, p);

            return guard.release();
        }

        static rb_node_base* minimum(rb_node_base* p) noexcept {
            while (p->left)
                p = p->left;
            return p;
        }

        static rb_node_base* maximum(rb_node_base* p) noexcept {
            while (p->right)
                p = p->right;
            return p;
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

            auto* actual = static_cast<node_type*>(node);
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