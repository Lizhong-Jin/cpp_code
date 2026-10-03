#include "../src/vector.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::fprintf(stderr, "vector check failed at line %d: %s\n", __LINE__, #__VA_ARGS__); \
    std::abort(); } } while (false)

struct AllocationRecord { int owner; std::size_t count; };
static std::map<void*, AllocationRecord> allocations;
static int allocator_swaps = 0;

template<class T, bool Copy, bool Move, bool Swap>
struct OwnerAllocator : mystl::allocator<T> {
    template<class U> struct rebind { using other = OwnerAllocator<U, Copy, Move, Swap>; };
    using propagate_on_container_copy_assignment = mystl::bool_constant<Copy>;
    using propagate_on_container_move_assignment = mystl::bool_constant<Move>;
    using propagate_on_container_swap = mystl::bool_constant<Swap>;
    using is_always_equal = mystl::false_type;
    int owner = 0;
    int state = 0;

    OwnerAllocator() = default;
    explicit OwnerAllocator(int id, int value = 0) : owner(id), state(value) {}
    template<class U>
    OwnerAllocator(const OwnerAllocator<U, Copy, Move, Swap>& other)
        : owner(other.owner), state(other.state) {}
    bool operator==(const OwnerAllocator& other) const { return owner == other.owner; }
    T* allocate(std::size_t n) {
        T* p = mystl::allocator<T>::allocate(n);
        CHECK(allocations.emplace(p, AllocationRecord{owner, n}).second);
        return p;
    }
    void deallocate(T* p, std::size_t n) {
        auto it = allocations.find(p);
        CHECK(it != allocations.end());
        CHECK(it->second.owner == owner && it->second.count == n);
        allocations.erase(it);
        mystl::allocator<T>::deallocate(p, n);
    }
    OwnerAllocator select_on_container_copy_construction() const {
        return OwnerAllocator(owner + 100, state);
    }
    friend void swap(OwnerAllocator& a, OwnerAllocator& b) noexcept {
        ++allocator_swaps;
        std::swap(a.owner, b.owner);
        std::swap(a.state, b.state);
    }
};

struct Tracked {
    static inline int live = 0;
    static inline int copies_before_throw = -1;
    static inline int assignments_before_throw = -1;
    static inline bool fail_default = false;
    int value;
    Tracked() : value(0) {
        if (fail_default) throw std::runtime_error("default");
        ++live;
    }
    explicit Tracked(int n) : value(n) { ++live; }
    Tracked(const Tracked& other) : value(other.value) {
        tick(copies_before_throw);
        ++live;
    }
    Tracked(Tracked&& other) noexcept(false) : value(other.value) {
        other.value = -1;
        ++live;
    }
    Tracked& operator=(const Tracked& other) {
        tick(assignments_before_throw);
        value = other.value;
        return *this;
    }
    Tracked& operator=(Tracked&& other) noexcept(false) {
        tick(assignments_before_throw);
        value = other.value;
        other.value = -1;
        return *this;
    }
    ~Tracked() { CHECK(live > 0); --live; }
    static void tick(int& remaining) {
        if (remaining == 0) throw std::runtime_error("injected failure");
        if (remaining > 0) --remaining;
    }
};

struct MoveOnly {
    int value;
    explicit MoveOnly(int n = 0) : value(n) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&& other) noexcept : value(other.value) { other.value = -1; }
    MoveOnly& operator=(MoveOnly&& other) noexcept {
        value = other.value; other.value = -1; return *this;
    }
};

// Construction can use copying, but shifting must use move assignment.
struct MoveAssignmentOnly {
    int value;
    explicit MoveAssignmentOnly(int n = 0) : value(n) {}
    MoveAssignmentOnly(const MoveAssignmentOnly&) = default;
    MoveAssignmentOnly(MoveAssignmentOnly&& other) noexcept(false) : value(other.value) {}
    MoveAssignmentOnly& operator=(const MoveAssignmentOnly&) = delete;
    MoveAssignmentOnly& operator=(MoveAssignmentOnly&& other) noexcept {
        value = other.value; other.value = -1; return *this;
    }
};

struct CopyOnly {
    int value = 0;
    CopyOnly() = default;
    explicit CopyOnly(int n) : value(n) {}
    CopyOnly(const CopyOnly&) = default;
    CopyOnly(CopyOnly&&) = delete;
    CopyOnly& operator=(const CopyOnly&) = default;
    CopyOnly& operator=(CopyOnly&&) = delete;
};

static_assert(mystl::is_copy_assignable_v<CopyOnly>);
static_assert(mystl::is_copy_assignable_v<int>);
static_assert(!mystl::is_copy_assignable_v<const int>);
static_assert(!mystl::is_copy_assignable_v<MoveOnly>);
static_assert(!mystl::is_move_constructible_v<CopyOnly>);
static_assert(mystl::is_move_constructible_v<MoveOnly>);
static_assert(!mystl::is_move_constructible_v<void>);

struct ThrowingMoveOnly {
    static inline int live = 0;
    static inline int moves_before_throw = -1;
    int value;
    explicit ThrowingMoveOnly(int n = 0) : value(n) { ++live; }
    ThrowingMoveOnly(const ThrowingMoveOnly&) = delete;
    ThrowingMoveOnly& operator=(const ThrowingMoveOnly&) = delete;
    ThrowingMoveOnly(ThrowingMoveOnly&& other) : value(other.value) {
        Tracked::tick(moves_before_throw);
        other.value = -1;
        ++live;
    }
    ThrowingMoveOnly& operator=(ThrowingMoveOnly&& other) {
        Tracked::tick(moves_before_throw);
        value = other.value; other.value = -1; return *this;
    }
    ~ThrowingMoveOnly() { CHECK(live > 0); --live; }
};

// Copies share the cursor, unlike a multipass iterator with an input category tag.
template<class T>
struct SinglePassIterator {
    using iterator_category = mystl::input_iterator_tag;
    using value_type = T;
    using pointer = const T*;
    using reference = const T&;
    using difference_type = std::ptrdiff_t;
    struct State { const T* current; const T* last; const T* throw_at = nullptr; };
    State* state = nullptr;
    reference operator*() const {
        if (state->current == state->throw_at) throw std::runtime_error("input read");
        return *state->current;
    }
    SinglePassIterator& operator++() { ++state->current; return *this; }
    bool operator==(const SinglePassIterator& other) const {
        const bool done = !state || state->current == state->last;
        const bool other_done = !other.state || other.state->current == other.state->last;
        return done || other_done ? done == other_done : state == other.state;
    }
};

template<class T>
struct LimitedAllocator : mystl::allocator<T> {
    template<class U> struct rebind { using other = LimitedAllocator<U>; };
    using propagate_on_container_move_assignment = mystl::false_type;
    using is_always_equal = mystl::false_type;
    std::size_t limit = 3;
    explicit LimitedAllocator(std::size_t n = 3) : limit(n) {}
    std::size_t max_size() const { return limit; }
    bool operator==(const LimitedAllocator& other) const { return limit == other.limit; }
    T* allocate(std::size_t n) {
        CHECK(n <= limit);
        return mystl::allocator<T>::allocate(n);
    }
};

template<class Category>
struct IntIterator {
    using iterator_category = Category;
    using value_type = int;
    using pointer = const int*;
    using reference = const int&;
    using difference_type = std::ptrdiff_t;
    const int* p;
    reference operator*() const { return *p; }
    IntIterator& operator++() { ++p; return *this; }
    IntIterator operator++(int) { auto old = *this; ++*this; return old; }
    bool operator==(const IntIterator&) const = default;
};

template<class Function>
void expect_throw(Function f) {
    bool caught = false;
    try { f(); } catch (const std::runtime_error&) { caught = true; }
    CHECK(caught);
}

void allocator_tests() {
    using A = OwnerAllocator<int, true, true, true>;
    {
        mystl::vector<int, A> a({1, 2}, A(1, 10)), b({3, 4, 5}, A(2, 20));
        a = b;
        CHECK(a.get_allocator().owner == 2 && a.get_allocator().state == 20);
        CHECK(a.size() == 3 && a[2] == 5);
        mystl::vector<int, A> c(b);
        CHECK(c.get_allocator().owner == 102 && c[0] == 3);
        mystl::vector<int, A> explicit_copy(b, A(9));
        CHECK(explicit_copy.get_allocator().owner == 9);
        mystl::vector<int, A> same_owner({7}, A(2, 99));
        a = same_owner;
        CHECK(a.get_allocator().state == 99); // equality must not suppress propagation
        auto* source = c.data();
        a = mystl::move(c);
        CHECK(a.get_allocator().owner == 102 && a.data() == source && c.empty());
        auto* before_swap = b.data();
        a.swap(b);
        CHECK(a.get_allocator().owner == 2 && a.data() == before_swap);
        CHECK(b.get_allocator().owner == 102 && allocator_swaps == 1);
        auto& self = a;
        a = self;
        a = mystl::move(self);
        CHECK(a.size() == 3 && a[0] == 3);
    }
    CHECK(allocations.empty());

    using B = OwnerAllocator<int, false, false, false>;
    {
        mystl::vector<int, B> a({1}, B(1)), b({2, 3}, B(2));
        a = b;
        CHECK(a.get_allocator().owner == 1 && a[1] == 3);
        a = mystl::move(b);
        CHECK(a.get_allocator().owner == 1 && a.size() == 2 && b.empty());
        mystl::vector<int, B> same({8}, B(1));
        auto* p = same.data();
        a = mystl::move(same);
        CHECK(a.data() == p && same.empty());
        mystl::vector<int, B> other({9}, B(1));
        a.swap(other);
        CHECK(a[0] == 9 && other[0] == 8);
        mystl::vector<int, B> moved(mystl::move(a), B(7));
        CHECK(moved[0] == 9 && a.empty());
    }
    CHECK(allocations.empty());

    using M = OwnerAllocator<MoveOnly, false, false, false>;
    for (bool reserve_first : {false, true}) {
        mystl::vector<MoveOnly, M> a(M(1)), b(M(2));
        a.emplace_back(7);
        if (reserve_first) a.reserve(8);
        b.emplace_back(1); b.emplace_back(2); b.emplace_back(3);
        a = mystl::move(b);
        CHECK(a.size() == 3 && a[0].value == 1 && a[2].value == 3 && b.empty());
    }
    CHECK(allocations.empty());
    static_assert(noexcept(mystl::vector<int>(mystl::vector<int>{}, mystl::allocator<int>{})));
    mystl::vector<int> standard_allocator{1, 2};
    mystl::vector<int> moved(mystl::move(standard_allocator), mystl::allocator<int>{});
    CHECK(moved.size() == 2 && standard_allocator.empty());
}

void iterator_and_insert_tests() {
    const int values[] = {1, 2, 3, 4};
    using F = IntIterator<mystl::forward_iterator_tag>;
    using I = IntIterator<mystl::input_iterator_tag>;
    mystl::vector<int> v(3, 7);
    CHECK(v.size() == 3 && v[2] == 7);
    v.assign(2, 8);
    v.reserve(8);
    v.assign(F{values}, F{values + 4}); // needs advance; F has no operator+
    CHECK(v.size() == 4 && v[3] == 4);
    v.assign(F{values}, F{values + 1});
    CHECK(v.size() == 1 && v[0] == 1);
    v.assign(I{values}, I{values + 4});
    CHECK(v.size() == 4 && v[3] == 4);
    v.assign({9, 8});
    CHECK(v.size() == 2 && v[0] == 9);
    mystl::vector<int> f(F{values}, F{values + 4}), input(I{values}, I{values + 4});
    CHECK(f.size() == 4 && input[3] == 4);
    v.clear(); v.shrink_to_fit();
    CHECK(v.empty() && v.capacity() == 0);
    v.assign(F{values}, F{values});
    v.reserve(4); v.emplace(v.end(), 7);
    CHECK(v.size() == 1 && v[0] == 7);

    mystl::vector<std::string> s{"a", "b", "c"};
    s.reserve(4);
    auto* address = s.data();
    s.insert(s.begin(), std::size_t(1), s[1]);
    CHECK(s.data() == address && s.size() == 4);
    CHECK(s[0] == "b" && s[1] == "a" && s[2] == "b" && s[3] == "c");
    s.reserve(12);
    s.insert(s.end() - 1, std::size_t(3), s[1]);
    CHECK(s.size() == 7 && s[3] == "a" && s[5] == "a" && s[6] == "c");
    s.emplace(s.begin(), s.back());
    CHECK(s.front() == "c");
    CHECK(*s.rbegin() == s.back());
    std::string words[] = {"first", "second"};
    mystl::vector<std::string> range(words, words + 2), assigned;
    assigned = range; // ADL must not select std::distance in the range constructor
    CHECK(assigned.size() == 2 && assigned[1] == "second");
    assigned.reserve(8);
    assigned.assign(words, words + 1);
    assigned = range;
    CHECK(assigned.size() == 2 && assigned[0] == "first");

    mystl::vector<MoveOnly> m;
    m.emplace_back(1); m.emplace_back(2); m.reserve(8);
    m.emplace(m.begin(), 3);
    m.insert(m.begin() + 1, MoveOnly(4));
    CHECK(m.size() == 4 && m[0].value == 3 && m[1].value == 4 && m[3].value == 2);
    mystl::vector<MoveAssignmentOnly> mixed;
    mixed.emplace_back(1); mixed.emplace_back(2); mixed.reserve(8);
    mixed.emplace(mixed.begin(), 3);
    CHECK(mixed[0].value == 3 && mixed[2].value == 2);
}

void exception_tests() {
    using A = OwnerAllocator<Tracked, false, false, false>;
    {
        mystl::vector<Tracked, A> v(std::size_t(2), A(1));
        auto* old = v.data();
        Tracked::copies_before_throw = 0;
        expect_throw([&] { v.emplace_back(9); });
        CHECK(v.size() == 2 && v.data() == old && Tracked::live == 2);
        expect_throw([&] { v.resize(4); });
        CHECK(v.size() == 2 && v.data() == old && Tracked::live == 2);
        Tracked::copies_before_throw = 1; // prefix succeeds, suffix fails
        expect_throw([&] { v.emplace(v.begin() + 1, 9); });
        CHECK(v.size() == 2 && v.data() == old && Tracked::live == 2);
        Tracked::copies_before_throw = -1;
        Tracked::fail_default = true;
        expect_throw([&] { v.resize(4); });
        CHECK(v.data() == old && Tracked::live == 2);
        Tracked::fail_default = false;
        v.reserve(8);
        Tracked value(7);
        Tracked::copies_before_throw = 3; // temporary + two extra values; suffix fails
        expect_throw([&] { v.insert(v.end() - 1, std::size_t(3), value); });
        CHECK(v.size() == 2 && Tracked::live == 3);
        Tracked::copies_before_throw = -1;
        Tracked::assignments_before_throw = 0;
        expect_throw([&] { v.emplace(v.begin(), 9); });
        CHECK(Tracked::live == static_cast<int>(v.size()) + 1);
        Tracked::assignments_before_throw = -1;
        v.clear();
        CHECK(Tracked::live == 1);
    }
    CHECK(Tracked::live == 0 && allocations.empty());
}

void erase_tests() {
    mystl::vector<int> empty;
    CHECK(empty.erase(empty.begin(), empty.end()) == empty.end());
    mystl::vector<int> v{1, 2, 3, 4, 5};
    auto* old = v.data();
    const auto cap = v.capacity();
    CHECK(v.erase(v.begin() + 1) == v.begin() + 1);
    CHECK(v.size() == 4 && v[1] == 3 && v[3] == 5);
    CHECK(v.erase(v.end() - 1) == v.end());
    CHECK(v.erase(v.begin() + 1, v.begin() + 1) == v.begin() + 1);
    CHECK(v.erase(v.begin(), v.begin() + 2) == v.begin());
    CHECK(v.size() == 1 && v[0] == 4 && v.data() == old && v.capacity() == cap);
    CHECK(v.erase(v.begin(), v.end()) == v.end());
    CHECK(v.empty() && v.data() == old && v.capacity() == cap);

    mystl::vector<MoveOnly> m;
    for (int i = 0; i < 5; ++i) m.emplace_back(i);
    m.erase(m.begin() + 1, m.begin() + 3);
    CHECK(m.size() == 3 && m[0].value == 0 && m[1].value == 3 && m[2].value == 4);
    mystl::vector<CopyOnly> c(std::size_t(4));
    for (int i = 0; i < 4; ++i) c[i].value = i;
    c.erase(c.begin());
    c.erase(c.begin(), c.begin() + 1);
    CHECK(c.size() == 2 && c[0].value == 2 && c[1].value == 3);
    c.reserve(8);
    CopyOnly value(9);
    c.insert(c.begin(), value);
    c.insert(c.begin() + 1, std::size_t(2), value);
    CHECK(c.size() == 5 && c[0].value == 9 && c[3].value == 2);

    using A = OwnerAllocator<Tracked, false, false, false>;
    {
        mystl::vector<Tracked, A> tracked(std::size_t(5), A(1));
        for (int i = 0; i < 5; ++i) tracked[i].value = i;
        tracked.erase(tracked.begin() + 1, tracked.begin() + 3);
        CHECK(Tracked::live == 3 && tracked[1].value == 3);
        Tracked::assignments_before_throw = 0;
        expect_throw([&] { tracked.erase(tracked.begin()); });
        CHECK(tracked.size() == 3 && Tracked::live == 3);
        expect_throw([&] { tracked.erase(tracked.begin(), tracked.begin() + 1); });
        CHECK(tracked.size() == 3 && Tracked::live == 3);
        Tracked::assignments_before_throw = -1;
        tracked.erase(tracked.begin(), tracked.end());
        CHECK(Tracked::live == 0);
    }
    CHECK(allocations.empty());
}

void input_and_failure_tests() {
    using Input = SinglePassIterator<int>;
    const int data[] = {1, 2, 3, 4};
    Input::State state{data, data + 4};
    mystl::vector<int> v(Input{&state}, Input{});
    CHECK(v.size() == 4 && v[0] == 1 && v[3] == 4 && state.current == state.last);
    state.current = data;
    v.assign(Input{&state}, Input{});
    CHECK(v.size() == 4 && v[3] == 4);
    {
        Tracked source[] = {Tracked(1), Tracked(2), Tracked(3)};
        using It = SinglePassIterator<Tracked>;
        It::State failing{source, source + 3, source + 2};
        expect_throw([&] { mystl::vector<Tracked> doomed(It{&failing}, It{}); });
        CHECK(Tracked::live == 3);
        mystl::vector<Tracked> target;
        failing.current = source;
        expect_throw([&] { target.assign(It{&failing}, It{}); });
        CHECK(target.size() == 2 && Tracked::live == 5);
    }
    CHECK(Tracked::live == 0);
    using A = OwnerAllocator<ThrowingMoveOnly, false, false, false>;
    {
        mystl::vector<ThrowingMoveOnly, A> m(A(1));
        m.emplace_back(1); m.emplace_back(2);
        ThrowingMoveOnly::moves_before_throw = 1;
        expect_throw([&] { m.emplace_back(3); });
        // Values may have changed; every original object must remain alive and destructible.
        CHECK(m.size() == 2 && ThrowingMoveOnly::live == 2 && allocations.size() == 1);
        ThrowingMoveOnly::moves_before_throw = -1;
        m.clear(); m.emplace_back(4);
        CHECK(m[0].value == 4);
    }
    CHECK(ThrowingMoveOnly::live == 0 && allocations.empty());

    using B = OwnerAllocator<Tracked, false, false, false>;
    for (int fail_after = 0; fail_after < 4; ++fail_after) {
        mystl::vector<Tracked, B> source(std::size_t(4), B(1));
        Tracked::copies_before_throw = fail_after;
        expect_throw([&] { mystl::vector<Tracked, B> copy(source); });
        CHECK(Tracked::live == 4 && allocations.size() == 1);
        auto* old = source.data();
        Tracked::copies_before_throw = fail_after;
        expect_throw([&] { source.reserve(10); });
        CHECK(source.data() == old && source.size() == 4 && Tracked::live == 4);
        Tracked::copies_before_throw = -1;
    }
    CHECK(Tracked::live == 0 && allocations.empty());
}

void capacity_limit_tests() {
    using V = mystl::vector<int, LimitedAllocator<int>>;
    V small({1, 2}, LimitedAllocator<int>(3));
    V large({3, 4, 5, 6}, LimitedAllocator<int>(8));
    auto expect_length_error = [](auto f) {
        bool caught = false;
        try { f(); } catch (const mystl::length_error&) { caught = true; }
        CHECK(caught);
    };
    expect_length_error([&] { small.resize(4); });
    expect_length_error([&] { small.reserve(4); });
    expect_length_error([&] { small.insert(small.begin(), std::size_t(2), 9); });
    expect_length_error([&] { small = mystl::move(large); });
    expect_length_error([&] { V copy(large, LimitedAllocator<int>(3)); });
    expect_length_error([&] { V moved(mystl::move(large), LimitedAllocator<int>(3)); });
    CHECK(small.size() == 2 && small[0] == 1 && large.size() == 4);
    small.emplace_back(3);
    expect_length_error([&] { small.emplace_back(4); });
    CHECK(small.size() == 3 && small.capacity() == 3);
}

void differential_tests() {
    mystl::vector<std::string> actual;
    std::vector<std::string> expected;
    unsigned random = 1234567;
    for (int step = 0; step < 3000; ++step) {
        random = random * 1664525u + 1013904223u;
        auto value = std::to_string(step);
        auto offset = expected.empty() ? 0 : random % (expected.size() + 1);
        auto pos = actual.empty() ? actual.begin() : actual.begin() + offset;
        switch ((random >> 16) % 12) {
            case 0: actual.push_back(value); expected.push_back(value); break;
            case 1: actual.insert(pos, value); expected.insert(expected.begin() + offset, value); break;
            case 2: {
                std::size_t n = (random >> 8) % 4;
                actual.insert(pos, n, value); expected.insert(expected.begin() + offset, n, value); break;
            }
            case 3: actual.resize(random % 15); expected.resize(random % 15); break;
            case 4: actual.assign(std::size_t(random % 12), value); expected.assign(random % 12, value); break;
            case 5: actual.reserve(random % 30); expected.reserve(random % 30); break;
            case 6: actual.shrink_to_fit(); expected.shrink_to_fit(); break;
            case 7: if (!expected.empty()) { actual.pop_back(); expected.pop_back(); } break;
            case 8: if (!expected.empty()) {
                auto index = offset % expected.size();
                auto a = actual.erase(actual.begin() + index);
                auto b = expected.erase(expected.begin() + index);
                CHECK((a == actual.end()) == (b == expected.end()));
                if (b != expected.end()) CHECK(*a == *b);
            } break;
            case 9: if (!expected.empty()) {
                const auto last = offset + ((random >> 8) % (expected.size() - offset + 1));
                auto a = actual.erase(actual.begin() + offset, actual.begin() + last);
                auto b = expected.erase(expected.begin() + offset, expected.begin() + last);
                CHECK((a == actual.end()) == (b == expected.end()));
                if (b != expected.end()) CHECK(*a == *b);
            } break;
            case 10: {
                mystl::vector<std::string> copy(actual);
                actual = copy;
                break;
            }
            case 11: if (!expected.empty()) {
                const auto index = (random >> 8) % expected.size();
                actual.insert(pos, actual[index]);
                expected.insert(expected.begin() + offset, expected[index]);
            } break;
        }
        CHECK(actual.size() == expected.size() && actual.size() <= actual.capacity());
        for (std::size_t i = 0; i < expected.size(); ++i) CHECK(actual[i] == expected[i]);
    }
}

int main() {
    allocator_tests();
    iterator_and_insert_tests();
    exception_tests();
    erase_tests();
    input_and_failure_tests();
    capacity_limit_tests();
    differential_tests();
    CHECK(allocations.empty() && Tracked::live == 0);
    std::puts("vector regression tests passed");
}
