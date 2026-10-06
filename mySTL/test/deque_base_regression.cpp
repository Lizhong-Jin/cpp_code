#include "../src/deque.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::fprintf(stderr, "deque base check failed at line %d: %s\n", __LINE__, #__VA_ARGS__); \
    std::abort(); } } while (false)

struct Allocation { int owner; std::size_t count; };
struct Audit {
    static inline std::map<void*, Allocation> allocations;
    static inline int allocate_before_throw = -1;
    static inline int construct_before_throw = -1;
    static inline int pointer_objects = 0;
    static inline bool throw_map_copy = false;
    static inline std::size_t map_limit = (std::numeric_limits<std::size_t>::max)();
    static void tick(int& counter) {
        if (counter == 0) throw std::bad_alloc();
        if (counter > 0) --counter;
    }
    static void clean() {
        CHECK(allocations.empty());
        CHECK(pointer_objects == 0);
        allocate_before_throw = construct_before_throw = -1;
        throw_map_copy = false;
        map_limit = (std::numeric_limits<std::size_t>::max)();
    }
};

template <class T>
struct OwnerAllocator : mystl::allocator<T> {
    using is_always_equal = mystl::false_type;
    template <class U> struct rebind { using other = OwnerAllocator<U>; };
    int owner = 0;
    OwnerAllocator() = default;
    explicit OwnerAllocator(int id) : owner(id) {}
    OwnerAllocator(const OwnerAllocator& other) : owner(other.owner) {
        if constexpr (std::is_pointer_v<T>)
            if (Audit::throw_map_copy) throw std::runtime_error("map allocator copy");
    }
    template <class U>
    OwnerAllocator(const OwnerAllocator<U>& other) : owner(other.owner) {}
    bool operator==(const OwnerAllocator& other) const noexcept { return owner == other.owner; }
    T* allocate(std::size_t n) {
        CHECK(n <= max_size());
        Audit::tick(Audit::allocate_before_throw);
        T* p = mystl::allocator<T>::allocate(n);
        CHECK(Audit::allocations.emplace(p, Allocation{owner, n}).second);
        return p;
    }
    void deallocate(T* p, std::size_t n) noexcept {
        auto found = Audit::allocations.find(p);
        CHECK(found != Audit::allocations.end());
        CHECK(found->second.owner == owner && found->second.count == n);
        Audit::allocations.erase(found);
        mystl::allocator<T>::deallocate(p, n);
    }
    std::size_t max_size() const noexcept {
        if constexpr (std::is_pointer_v<T>) return Audit::map_limit;
        return (std::numeric_limits<std::size_t>::max)() / sizeof(T);
    }
    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        Audit::tick(Audit::construct_before_throw);
        std::construct_at(p, std::forward<Args>(args)...);
        if constexpr (std::is_pointer_v<U>) ++Audit::pointer_objects;
    }
    template <class U>
    void destroy(U* p) noexcept {
        if constexpr (std::is_pointer_v<U>) --Audit::pointer_objects;
        std::destroy_at(p);
    }
};

// Exposes only raw storage operations for tests. Tests explicitly manage any T
// objects they construct, matching the base/derived-container lifetime contract.
template <class T, class Alloc = OwnerAllocator<T>>
struct Storage : mystl::deque_base<T, Alloc> {
    using Base = mystl::deque_base<T, Alloc>;
    using typename Base::iterator;
    using typename Base::const_iterator;
    static constexpr std::size_t B = Base::block_size;
    explicit Storage(std::size_t n = 0, const Alloc& alloc = Alloc()) : Base(n, alloc) {}
    Storage(Storage&& other) : Base(std::move(other)) {}
    Storage(Storage&& other, const Alloc& alloc) : Base(std::move(other), alloc) {}
    iterator begin() { return this->M_start; }
    iterator end() { return this->M_finish; }
    const_iterator begin() const { return this->M_start; }
    const_iterator end() const { return this->M_finish; }
    std::size_t size() const { return this->M_storage_size(); }
    auto map() const { return this->M_map; }
    std::size_t capacity() const { return this->M_map_size; }
    std::size_t blocks() const {
        std::size_t n = 0;
        for (std::size_t i = 0; i < capacity(); ++i) n += map()[i] != nullptr;
        return n;
    }
    void front(std::size_t n) { this->M_reserve_map_at_front(n); }
    void back(std::size_t n) { this->M_reserve_map_at_back(n); }
    void initialize(std::size_t n) { this->M_initialize_map(n); }
    // Call after destroying the prefix. Keep the first block allocated.
    void drop_prefix_in_first_block(std::size_t n) {
        CHECK(n < B && n <= size());
        this->M_start += static_cast<std::ptrdiff_t>(n);
    }
};

using It = mystl::deque<int>::iterator;
using CIt = mystl::deque<int>::const_iterator;
static_assert(std::is_same_v<CIt::value_type, int>);
static_assert(std::is_same_v<CIt::reference, const int&>);
static_assert(std::is_convertible_v<It, CIt>);
static_assert(!std::is_convertible_v<CIt, It>);
static_assert(mystl::is_random_access_iterator_v<It>);
static_assert(!mystl::is_contiguous_iterator_v<It>);
static_assert(!std::is_copy_constructible_v<Storage<int>>);
static_assert(!std::is_move_assignable_v<Storage<int>>);
static_assert(std::is_same_v<
    mystl::allocator_traits<mystl::allocator<int>>::rebind_alloc<int*>, mystl::allocator<int*>>);

struct alignas(1024) Large {
    static inline int constructed = 0;
    static inline int destroyed = 0;
    int value;
    Large() = delete;
    explicit Large(int v) : value(v) { ++constructed; }
    ~Large() { ++destroyed; }
};

void test_storage_boundaries() {
    constexpr auto B = Storage<int>::B;
    for (std::size_t n : {std::size_t(0), B - 1, B, B + 1, 3 * B}) {
        {
            Storage<int> s(n, OwnerAllocator<int>(7));
            CHECK(s.size() == n);
            CHECK(s.blocks() == n / B + 1);
            CHECK(s.capacity() >= s.blocks() + 2);
            CHECK(s.end() - s.begin() == static_cast<std::ptrdiff_t>(n));
            CHECK(s.begin() + static_cast<std::ptrdiff_t>(n) == s.end());
            CHECK(s.get_allocator().owner == 7);
        }
        Audit::clean();
    }
    {
        Storage<Large> s(3);
        CHECK(s.B == 1 && s.blocks() == 4);
        CHECK(Large::constructed == 0 && Large::destroyed == 0);
        for (int i = 0; i < 3; ++i) {
            auto p = (s.begin() + i).operator->();
            CHECK(reinterpret_cast<std::uintptr_t>(p) % alignof(Large) == 0);
            std::construct_at(p, i);
        }
        CHECK((s.end() - 1)->value == 2);
        auto it = s.end();
        CHECK((--it)->value == 2);
        CHECK((it - 2)->value == 0);
        for (auto p = s.begin(); p != s.end(); ++p) std::destroy_at(p.operator->());
    }
    CHECK(Large::constructed == 3 && Large::destroyed == 3);
    Audit::clean();
}

void test_iterators_and_map_growth() {
    {
        constexpr int B = Storage<int>::B;
        const int n = 3 * B + 7;
        Storage<int> s(n);
        for (int i = 0; i < n; ++i) std::construct_at((s.begin() + i).operator->(), i);
        const auto& cs = s;
        for (int i = 0; i <= n; ++i) {
            It a = s.begin() + i;
            CIt c = a;
            CHECK(a == c && c == a);
            CHECK(c - s.begin() == i && s.end() - c == n - i);
            for (int j = 0; j <= n; ++j) {
                auto b = a + (j - i);
                CHECK(b == s.begin() + j);
                CHECK(a - (i - j) == b);
                CHECK((a < b) == (i < j));
                CHECK((c >= b) == (i >= j));
                CHECK((a <= b) == (i <= j));
                CHECK((c > b) == (i > j));
            }
            if (i < n) CHECK(a[0] == i && (i + cs.begin())[0] == i);
        }
        auto it = s.begin();
        for (int i = 0; i < n; ++i) CHECK(*it++ == i);
        CHECK(it == s.end());
        for (int i = n - 1; i >= 0; --i) { auto old = it--; CHECK(*it == i); CHECK(old == it + 1); }
        auto forward = s.begin();
        mystl::advance(forward, B + 2);
        CHECK(*forward == B + 2);
        CHECK(mystl::distance(s.begin(), s.end()) == n);
        mystl::reverse_iterator<It> reverse(s.end());
        CHECK(*reverse == n - 1 && reverse[B] == n - B - 1);
        // Grow in both directions; element addresses and offsets must not change.
        int* saved = (s.begin() + B + 5).operator->();
        s.front(100);
        CHECK(s.size() == static_cast<std::size_t>(n));
        CHECK((s.begin() + B + 5).operator->() == saved && *saved == B + 5);
        s.back(300);
        CHECK(s.end()[-1] == n - 1 && s.blocks() == 4);
        CHECK((s.begin() + B + 5).operator->() == saved);
        // Exercise recentering within the current capacity and the no-op path.
        auto old_capacity = s.capacity();
        s.front(old_capacity / 2);
        CHECK(s.capacity() == old_capacity);
        auto old_map = s.map();
        auto old_begin = s.begin();
        s.front(0);
        s.front(1);
        CHECK(s.map() == old_map && s.begin() == old_begin);
        for (auto p = s.begin(); p != s.end(); ++p) std::destroy_at(p.operator->());
    }
    Audit::clean();
}

void test_moves() {
    {
        Storage<int> source(200, OwnerAllocator<int>(1));
        auto original_map = source.map();
        Storage<int> same(std::move(source), OwnerAllocator<int>(1));
        CHECK(same.map() == original_map && source.map() == nullptr && source.size() == 0);
        CHECK(source.begin() + 0 == source.end());
        source.initialize(0); // moved-from storage remains reusable
        CHECK(source.blocks() == 1);
        Storage<int> unequal(std::move(same), OwnerAllocator<int>(2));
        CHECK(same.map() == original_map && same.size() == 200);
        CHECK(unequal.map() != original_map && unequal.size() == 200);
        CHECK(unequal.get_allocator().owner == 2);
        auto unequal_map = unequal.map();
        Storage<int> moved(std::move(unequal));
        CHECK(moved.map() == unequal_map && unequal.size() == 0);
        CHECK(moved.get_allocator().owner == 2);
        Storage<int> singular(std::move(unequal));
        CHECK(singular.size() == 0 && singular.map() == nullptr);
    }
    Audit::clean(); // deallocate checks validate owner AND exact allocation size
    {
        Storage<int> source(200, OwnerAllocator<int>(1));
        auto map = source.map();
        Audit::throw_map_copy = true;
        bool threw = false;
        try { Storage<int> target(std::move(source)); }
        catch (const std::runtime_error&) { threw = true; }
        Audit::throw_map_copy = false;
        CHECK(threw && source.map() == map && source.size() == 200);
    }
    Audit::clean();
}

void test_nonzero_start_offset() {
    {
        constexpr int B = Storage<int>::B;
        Storage<int> s(2 * B + 3);
        for (int i = 0; i < 2 * B + 3; ++i)
            std::construct_at((s.begin() + i).operator->(), i);
        for (int i = 0; i < B - 1; ++i)
            std::destroy_at((s.begin() + i).operator->());
        s.drop_prefix_in_first_block(B - 1);
        int* first = s.begin().operator->();
        CHECK(*first == B - 1 && s.size() == B + 4);
        s.front(100);
        s.back(300);
        CHECK(s.begin().operator->() == first && *s.begin() == B - 1);
        CHECK(s.end()[-1] == 2 * B + 2 && s.end() - s.begin() == B + 4);
        auto it = s.begin();
        CHECK(*++it == B && *--it == B - 1);
        for (auto p = s.begin(); p != s.end(); ++p) std::destroy_at(p.operator->());
    }
    Audit::clean();
}

void test_exceptions() {
    // Map allocation, followed by each of three block allocations.
    for (int failure = 0; failure < 4; ++failure) {
        Audit::allocate_before_throw = failure;
        bool threw = false;
        try { Storage<int> s(2 * Storage<int>::B); }
        catch (const std::bad_alloc&) { threw = true; }
        CHECK(threw);
        Audit::clean();
    }
    // Every pointer construction position in the initial eight-slot map.
    for (int failure = 0; failure < 8; ++failure) {
        Audit::construct_before_throw = failure;
        bool threw = false;
        try { Storage<int> s; }
        catch (const std::bad_alloc&) { threw = true; }
        CHECK(threw);
        Audit::clean();
    }
    {
        Storage<int> s(200, OwnerAllocator<int>(5));
        auto map = s.map();
        auto begin = s.begin();
        auto end = s.end();
        const auto count = Audit::allocations.size();
        const int pointers = Audit::pointer_objects;
        for (int mode = 0; mode < 3; ++mode) {
            bool threw = false;
            Audit::allocate_before_throw = mode == 0 ? 0 : -1;
            Audit::construct_before_throw = mode == 1 ? 3 : -1;
            try {
                if (mode == 2) s.front((std::numeric_limits<std::size_t>::max)());
                else s.back(100);
            } catch (const std::bad_alloc&) { threw = true; }
              catch (const mystl::length_error&) { threw = true; }
            CHECK(threw && s.map() == map && s.begin() == begin && s.end() == end);
            CHECK(Audit::allocations.size() == count && Audit::pointer_objects == pointers);
        }
        Audit::allocate_before_throw = Audit::construct_before_throw = -1;
        bool threw = false;
        Audit::allocate_before_throw = 1; // unequal move: new map succeeds, first block fails
        try { Storage<int> target(std::move(s), OwnerAllocator<int>(6)); }
        catch (const std::bad_alloc&) { threw = true; }
        Audit::allocate_before_throw = -1;
        CHECK(threw && s.map() == map && s.begin() == begin && s.end() == end);
        CHECK(Audit::allocations.size() == count && Audit::pointer_objects == pointers);
    }
    Audit::clean();
    {
        bool threw = false;
        try { Storage<int> s((std::numeric_limits<std::size_t>::max)()); }
        catch (const mystl::length_error&) { threw = true; }
        CHECK(threw);
    }
    Audit::clean();
    {
        Audit::map_limit = 3;
        Storage<int> s;
        CHECK(s.capacity() == 3);
        auto map = s.map();
        bool threw = false;
        try { s.back(2); } catch (const mystl::length_error&) { threw = true; }
        CHECK(threw && s.map() == map && s.size() == 0);
    }
    Audit::clean();
}

struct MoveOnlyElement {
    static inline int live = 0;
    static inline int moves = 0;
    static inline int moves_before_throw = -1;
    int value = 0;
    MoveOnlyElement() { ++live; }
    MoveOnlyElement(const MoveOnlyElement&) = delete;
    MoveOnlyElement(MoveOnlyElement&& other) : value(other.value) {
        if (moves_before_throw == 0) throw std::runtime_error("element move");
        if (moves_before_throw > 0) --moves_before_throw;
        other.value = -1;
        ++moves;
        ++live;
    }
    ~MoveOnlyElement() { --live; }
};

// Inspect the public constructor without requiring unfinished deque accessors.
struct MoveDeque : mystl::deque<MoveOnlyElement, OwnerAllocator<MoveOnlyElement>> {
    using Alloc = OwnerAllocator<MoveOnlyElement>;
    using Container = mystl::deque<MoveOnlyElement, Alloc>;
    MoveDeque(std::size_t n, const Alloc& alloc) : Container(n, alloc) {}
    MoveDeque(MoveDeque&& other, const Alloc& alloc)
        : Container(mystl::move(static_cast<Container&>(other)), alloc) {}
    auto first() { return this->M_start; }
    auto count() const { return this->M_finish - this->M_start; }
};

void test_allocator_extended_move() {
    constexpr int B = mystl::deque_block_size<MoveOnlyElement>;
    constexpr int n = 2 * B + 3;
    for (bool equal : {true, false}) {
        MoveOnlyElement::moves = 0;
        {
            MoveDeque source(n, MoveDeque::Alloc(1));
            for (int i = 0; i < n; ++i) source.first()[i].value = i;
            auto original_address = source.first().operator->();
            MoveDeque target(mystl::move(source), MoveDeque::Alloc(equal ? 1 : 2));
            CHECK(target.count() == n && source.count() == 0);
            CHECK(MoveOnlyElement::live == n);
            CHECK(MoveOnlyElement::moves == (equal ? 0 : n));
            if (equal) CHECK(target.first().operator->() == original_address);
            for (int i = 0; i < n; ++i) CHECK(target.first()[i].value == i);
        }
        CHECK(MoveOnlyElement::live == 0);
        Audit::clean();
    }
    for (int failure : {0, B + 1}) {
        {
            MoveDeque source(n, MoveDeque::Alloc(1));
            for (int i = 0; i < n; ++i) source.first()[i].value = i;
            const auto original_begin = source.first();
            const auto allocation_count = Audit::allocations.size();
            const auto pointer_count = Audit::pointer_objects;
            MoveOnlyElement::moves = 0;
            MoveOnlyElement::moves_before_throw = failure;
            bool threw = false;
            try { MoveDeque target(mystl::move(source), MoveDeque::Alloc(2)); }
            catch (const std::runtime_error&) { threw = true; }
            MoveOnlyElement::moves_before_throw = -1;
            CHECK(threw && source.count() == n && source.first() == original_begin);
            CHECK(MoveOnlyElement::live == n && MoveOnlyElement::moves == failure);
            CHECK(Audit::allocations.size() == allocation_count);
            CHECK(Audit::pointer_objects == pointer_count);
            for (int i = 0; i < n; ++i)
                CHECK(source.first()[i].value == (i < failure ? -1 : i));
        }
        CHECK(MoveOnlyElement::live == 0);
        Audit::clean();
    }
    {
        MoveDeque empty(0, MoveDeque::Alloc(1));
        MoveDeque target(mystl::move(empty), MoveDeque::Alloc(2));
        CHECK(empty.count() == 0 && target.count() == 0);
        // Moving again from the now singular source must remain safe.
        MoveDeque again(mystl::move(empty), MoveDeque::Alloc(2));
        CHECK(again.count() == 0 && MoveOnlyElement::live == 0);
    }
    Audit::clean();
}

struct GuardElement {
    static inline std::map<const GuardElement*, bool> live;
    static inline int copies_before_throw = -1;
    static inline int assignments_before_throw = -1;
    int value = 0;

    explicit GuardElement(int n = 0) : value(n) { CHECK(live.emplace(this, true).second); }
    GuardElement(const GuardElement& other) : value(other.value) {
        tick(copies_before_throw);
        CHECK(live.emplace(this, true).second);
    }
    GuardElement& operator=(const GuardElement& other) {
        CHECK(live.count(this) && live.count(&other));
        tick(assignments_before_throw);
        value = other.value;
        return *this;
    }
    ~GuardElement() { CHECK(live.erase(this) == 1); }
    static void tick(int& n) {
        if (n == 0) throw std::runtime_error("guard element failure");
        if (n > 0) --n;
    }
};

template <class T>
struct AssignDeque : mystl::deque<T, OwnerAllocator<T>> {
    using Container = mystl::deque<T, OwnerAllocator<T>>;
    explicit AssignDeque(std::size_t n = 0) : Container(n, OwnerAllocator<T>(17)) {}
    AssignDeque(AssignDeque&& other) : Container(mystl::move(static_cast<Container&>(other))) {}
    auto first() { return this->M_start; }
    auto map() { return this->M_map; }
    auto map_size() { return this->M_map_size; }
};

void test_assign_guards() {
    constexpr int B = mystl::deque_block_size<GuardElement>;
    constexpr int n = B + 5;
    constexpr int target = 2 * B + 20; // grows at BOTH ends (11 head elements for B=128)
    // Snapshot copy, partial head, first/partial tail construction failures.
    for (int failure : {0, 1, 7, 12, 15}) {
        {
            AssignDeque<GuardElement> d(n);
            GuardElement value(42);
            auto address = d.first().operator->();
            auto allocations = Audit::allocations.size();
            auto pointers = Audit::pointer_objects;
            GuardElement::copies_before_throw = failure;
            bool threw = false;
            try { d.assign(target, value); } catch (const std::runtime_error&) { threw = true; }
            GuardElement::copies_before_throw = -1;
            CHECK(threw && d.size() == n && d.first().operator->() == address);
            CHECK(GuardElement::live.size() == n + 1);
            CHECK(Audit::allocations.size() == allocations && Audit::pointer_objects == pointers);
            // A second operation must be able to reuse the slots cleared by rollback.
            d.assign(target, value);
            CHECK(d.size() == target && GuardElement::live.size() == target + 1);
        }
        CHECK(GuardElement::live.empty());
        Audit::clean();
    }
    // Assignment fails after head objects were already successfully constructed.
    for (int failure : {0, 7, n - 1}) {
        {
            AssignDeque<GuardElement> d(n);
            GuardElement value(42);
            auto allocations = Audit::allocations.size();
            GuardElement::assignments_before_throw = failure;
            bool threw = false;
            try { d.assign(target, value); } catch (const std::runtime_error&) { threw = true; }
            GuardElement::assignments_before_throw = -1;
            CHECK(threw && d.size() == n && GuardElement::live.size() == n + 1);
            CHECK(Audit::allocations.size() == allocations);
            // Shrinking must also leave every original object alive on assignment failure.
            GuardElement::assignments_before_throw = 1;
            threw = false;
            try { d.assign(3, value); } catch (const std::runtime_error&) { threw = true; }
            GuardElement::assignments_before_throw = -1;
            CHECK(threw && d.size() == n && GuardElement::live.size() == n + 1);
        }
        CHECK(GuardElement::live.empty());
        Audit::clean();
    }
    // Failure in the first or second block allocation, and allocator construct hooks.
    for (int mode = 0; mode < 4; ++mode) {
        {
            AssignDeque<GuardElement> d(n);
            GuardElement value(42);
            const auto allocations = Audit::allocations.size();
            if (mode < 2) Audit::allocate_before_throw = mode;
            else Audit::construct_before_throw = mode == 2 ? 0 : 12;
            bool threw = false;
            try { d.assign(target, value); } catch (const std::bad_alloc&) { threw = true; }
            Audit::allocate_before_throw = Audit::construct_before_throw = -1;
            CHECK(threw && d.size() == n && GuardElement::live.size() == n + 1);
            CHECK(Audit::allocations.size() == allocations);
        }
        CHECK(GuardElement::live.empty());
        Audit::clean();
    }
    // A map expansion may remain committed under the basic exception guarantee,
    // but the old elements and only their blocks must remain after rollback.
    {
        AssignDeque<GuardElement> d(n);
        GuardElement value(42);
        auto address = d.first().operator->();
        const auto allocations = Audit::allocations.size();
        GuardElement::copies_before_throw = 300;
        bool threw = false;
        try { d.assign(20 * B, value); } catch (const std::runtime_error&) { threw = true; }
        GuardElement::copies_before_throw = -1;
        CHECK(threw && d.size() == n && d.first().operator->() == address);
        CHECK(GuardElement::live.size() == n + 1 && Audit::allocations.size() == allocations);
        CHECK(Audit::pointer_objects == int(d.map_size()));
        d.assign(20 * B, value);
        CHECK(d.size() == 20 * B);
        // An aliased value survives shrinking and freeing its original block.
        d.assign(1, d.first()[0]);
        CHECK(d.size() == 1 && d.first()[0].value == 42);
        d.assign(0, d.first()[0]);
        CHECK(d.empty() && GuardElement::live.size() == 1);
    }
    CHECK(GuardElement::live.empty());
    Audit::clean();
}

void test_assign_sequences() {
    {
        AssignDeque<int> d;
        std::uint32_t random = 941;
        for (int step = 0; step < 1000; ++step) {
            random = random * 1664525u + 1013904223u;
            const std::size_t count = random % 4000;
            d.assign(count, step);
            CHECK(d.size() == count);
            for (std::size_t i = 0; i < count; ++i) CHECK(d.first()[i] == step);
            if (step % 13 == 0) { d.clear(); CHECK(d.empty()); }
        }
        d.assign(128, 3);
        d.assign(129, d.first()[0]);
        CHECK(d.first()[128] == 3);
        AssignDeque<int> moved(mystl::move(d));
        d.assign(300, 9);
        CHECK(d.size() == 300 && d.first()[299] == 9 && moved.size() == 129);
    }
    Audit::clean();
    struct Big {
        char padding[512]{};
        int value = 0;
    };
    static_assert(mystl::deque_block_size<Big> == 1);
    {
        AssignDeque<Big> d;
        Big value; value.value = 17;
        for (std::size_t n : {1u, 2u, 20u, 3u, 0u, 7u}) {
            d.assign(n, value);
            CHECK(d.size() == n);
            for (std::size_t i = 0; i < n; ++i) CHECK(d.first()[i].value == 17);
        }
    }
    Audit::clean();
}

// Audit both the allocator that releases storage and the allocator that ends
// each object's lifetime (including the map's pointer objects).
struct AssignmentAudit {
    static inline std::map<void*, int> objects;
};

template <class T, bool Copy, bool Move>
struct AssignmentAllocator : OwnerAllocator<T> {
    using propagate_on_container_copy_assignment = mystl::bool_constant<Copy>;
    using propagate_on_container_move_assignment = mystl::bool_constant<Move>;
    template <class U> struct rebind { using other = AssignmentAllocator<U, Copy, Move>; };
    int tag = 0; // Propagation is observable even when owner IDs compare equal.
    AssignmentAllocator() = default;
    AssignmentAllocator(int owner, int state = 0) : OwnerAllocator<T>(owner), tag(state) {}
    AssignmentAllocator(const AssignmentAllocator&) = default;
    template <class U>
    AssignmentAllocator(const AssignmentAllocator<U, Copy, Move>& a)
        : OwnerAllocator<T>(a.owner), tag(a.tag) {}
    AssignmentAllocator& operator=(const AssignmentAllocator& a) noexcept {
        this->owner = a.owner;
        tag = a.tag;
        return *this;
    }
    AssignmentAllocator& operator=(AssignmentAllocator&& a) noexcept {
        this->owner = a.owner;
        tag = a.tag;
        a.owner = -1;
        a.tag = -1;
        return *this;
    }
    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        OwnerAllocator<T>::construct(p, std::forward<Args>(args)...);
        CHECK(AssignmentAudit::objects.emplace(p, this->owner).second);
    }
    template <class U>
    void destroy(U* p) noexcept {
        auto it = AssignmentAudit::objects.find(p);
        CHECK(it != AssignmentAudit::objects.end() && it->second == this->owner);
        AssignmentAudit::objects.erase(it);
        OwnerAllocator<T>::destroy(p);
    }
};

template <bool Copy, bool Move>
void test_assignment_propagation() {
    using Alloc = AssignmentAllocator<GuardElement, Copy, Move>;
    using D = mystl::deque<GuardElement, Alloc>;
    for (bool equal : {false, true}) {
        {
            const int source_owner = equal ? 1 : 2;
            D source(130, Alloc(source_owner, 22));
            D target(3, Alloc(1, 11));
            for (int i = 0; i < 130; ++i) source[i].value = i;
            target = source;
            CHECK(target.get_allocator().owner == (Copy ? source_owner : 1));
            CHECK(target.get_allocator().tag == (Copy ? 22 : 11));
            for (int i = 0; i < 130; ++i) CHECK(target[i].value == i && source[i].value == i);

            // A subsequent non-propagating transfer catches a stale map allocator
            // left behind by copy propagation.
            D receiver(2, Alloc(target.get_allocator().owner, 33));
            auto address = target.begin().operator->();
            receiver = mystl::move(target);
            CHECK(receiver.begin().operator->() == address && target.empty());
            CHECK(receiver.get_allocator().tag == (Move ? (Copy ? 22 : 11) : 33));
            target.emplace_back(7);
            CHECK(target.size() == 1 && target.front().value == 7);
            auto* self = &receiver;
            receiver = *self;
            receiver = mystl::move(*self);
            CHECK(receiver.size() == 130 && receiver[129].value == 129);
        }
        CHECK(GuardElement::live.empty() && AssignmentAudit::objects.empty());
        Audit::clean();
        {
            D source(130, Alloc(equal ? 1 : 2, 22));
            D target(3, Alloc(1, 11));
            for (int i = 0; i < 130; ++i) source[i].value = i;
            auto address = source.begin().operator->();
            target = mystl::move(source);
            CHECK(target.get_allocator().owner == (Move && !equal ? 2 : 1));
            CHECK(target.get_allocator().tag == (Move ? 22 : 11));
            CHECK(source.empty() && target.size() == 130);
            CHECK((target.begin().operator->() == address) == (Move || equal));
            for (int i = 0; i < 130; ++i) CHECK(target[i].value == i);
            source.emplace_front(9);
            CHECK(source.front().value == 9);
        }
        CHECK(GuardElement::live.empty() && AssignmentAudit::objects.empty());
        Audit::clean();
    }
}

void test_assignment_failures() {
    using CopyAlloc = AssignmentAllocator<GuardElement, true, false>;
    for (int failure = 0; failure < 5; ++failure) {
        {
            mystl::deque<GuardElement, CopyAlloc> source(130, CopyAlloc(2));
            mystl::deque<GuardElement, CopyAlloc> target(3, CopyAlloc(1));
            if (failure < 3) Audit::allocate_before_throw = failure;
            else GuardElement::copies_before_throw = failure == 3 ? 0 : 4;
            bool threw = false;
            try { target = source; } catch (...) { threw = true; }
            Audit::allocate_before_throw = GuardElement::copies_before_throw = -1;
            CHECK(threw && source.size() == 130 && target.empty());
            CHECK(GuardElement::live.size() == 130 && target.get_allocator().owner == 2);
            // Both propagated allocators must work after an allocation or
            // construction failure left the target empty.
            target.emplace_back(7);
            CHECK(target.front().value == 7);
        }
        CHECK(GuardElement::live.empty() && AssignmentAudit::objects.empty());
        Audit::clean();
    }
    using MoveAlloc = AssignmentAllocator<GuardElement, false, false>;
    for (bool construction_failure : {false, true}) {
        {
            mystl::deque<GuardElement, MoveAlloc> source(130, MoveAlloc(2));
            mystl::deque<GuardElement, MoveAlloc> target(3, MoveAlloc(1));
            const auto allocations = Audit::allocations.size();
            if (construction_failure) GuardElement::copies_before_throw = 0;
            else GuardElement::assignments_before_throw = 0;
            bool threw = false;
            try { target = mystl::move(source); } catch (...) { threw = true; }
            GuardElement::copies_before_throw = GuardElement::assignments_before_throw = -1;
            CHECK(threw && source.size() == 130 && target.size() == 3);
            CHECK(GuardElement::live.size() == 133 && Audit::allocations.size() == allocations);
            target = mystl::move(source);
            CHECK(source.empty() && target.size() == 130 && target.get_allocator().owner == 1);
        }
        CHECK(GuardElement::live.empty() && AssignmentAudit::objects.empty());
        Audit::clean();
    }
}

void test_insert_guards() {
    constexpr int B = mystl::deque_block_size<GuardElement>;
    for (bool front : {false, true}) {
        for (bool allocation_failure : {false, true}) {
            {
                AssignDeque<GuardElement> d(B - 1);
                const auto allocations = Audit::allocations.size();
                for (int retry = 0; retry < 2; ++retry) {
                    if (allocation_failure) Audit::allocate_before_throw = 0;
                    else Audit::construct_before_throw = 0;
                    bool threw = false;
                    try {
                        if (front) d.emplace_front(9);
                        else d.emplace_back(9);
                    } catch (const std::bad_alloc&) { threw = true; }
                    Audit::allocate_before_throw = Audit::construct_before_throw = -1;
                    CHECK(threw && d.size() == B - 1 && GuardElement::live.size() == B - 1);
                    CHECK(Audit::allocations.size() == allocations);
                }
                if (front) d.emplace_front(9);
                else d.emplace_back(9);
                CHECK(d.size() == B);
            }
            CHECK(GuardElement::live.empty());
            Audit::clean();
        }
        // Failure before construction, during edge construction, and during
        // either shifted-element assignment or the final inserted-value assignment.
        for (int mode = 0; mode < 5; ++mode) {
            {
                AssignDeque<GuardElement> d(2 * B - 1);
                const auto allocations = Audit::allocations.size();
                if (mode == 0) Audit::allocate_before_throw = 0;
                if (mode == 1) Audit::construct_before_throw = 0;
                if (mode == 2) GuardElement::copies_before_throw = 0;
                if (mode >= 3) GuardElement::assignments_before_throw = mode == 3 ? 0 : 2;
                bool threw = false;
                try { d.emplace(d.begin() + (front ? 3 : d.size() - 3), 9); }
                catch (...) { threw = true; }
                Audit::allocate_before_throw = Audit::construct_before_throw = -1;
                GuardElement::copies_before_throw = GuardElement::assignments_before_throw = -1;
                CHECK(threw && d.size() == 2 * B - 1 && GuardElement::live.size() == d.size());
                CHECK(Audit::allocations.size() == allocations);
                auto it = d.emplace(d.begin() + (front ? 3 : d.size() - 3), 9);
                CHECK(it->value == 9 && d.size() == 2 * B);
            }
            CHECK(GuardElement::live.empty());
            Audit::clean();
        }
    }
    // Map growth succeeds, then block allocation fails: no live object may be
    // left outside the old range, even if the larger map remains installed.
    for (bool front : {false, true}) {
        for (int failure : {0, 1}) {
            {
                AssignDeque<GuardElement> d;
                GuardElement value(7);
                d.assign(8 * B - 1, value);
                const auto allocations = Audit::allocations.size();
                Audit::allocate_before_throw = failure;
                bool threw = false;
                try {
                    if (front) d.emplace_front(9);
                    else d.emplace_back(9);
                } catch (const std::bad_alloc&) { threw = true; }
                Audit::allocate_before_throw = -1;
                CHECK(threw && d.size() == 8 * B - 1 && GuardElement::live.size() == d.size() + 1);
                CHECK(Audit::allocations.size() == allocations);
            }
            CHECK(GuardElement::live.empty());
            Audit::clean();
        }
    }
}

struct InsertLarge {
    int value;
    char padding[512]{};
    explicit InsertLarge(int n = 0) : value(n) {}
    bool operator==(const InsertLarge& other) const { return value == other.value; }
};

template <class T>
void test_modifier_sequences() {
    mystl::deque<T> d;
    std::deque<T> expected;
    std::mt19937 random(20261006);
    for (int step = 0; step < 2500; ++step) {
        const int value = static_cast<int>(random() % 1000);
        switch (random() % 6) {
            case 0: d.emplace_front(value); expected.emplace_front(value); break;
            case 1: d.emplace_back(value); expected.emplace_back(value); break;
            case 2: if (!d.empty()) { d.pop_front(); expected.pop_front(); } break;
            case 3: if (!d.empty()) { d.pop_back(); expected.pop_back(); } break;
            default: {
                const auto index = random() % (d.size() + 1);
                auto it = d.emplace(d.begin() + index, value);
                expected.emplace(expected.begin() + index, value);
                CHECK(it == d.begin() + index && *it == T(value));
            }
        }
        CHECK(d.size() == expected.size());
        for (std::size_t i = 0; i < d.size(); ++i) CHECK(d[i] == expected[i]);
    }
    d.clear();
    d.emplace(d.begin(), 3);
    auto moved = mystl::move(d);
    auto it = d.emplace(d.begin(), 4);
    CHECK(it == d.begin() && *it == T(4) && moved.front() == T(3));
}

template <class T>
struct LimitedDequeAllocator : OwnerAllocator<T> {
    template <class U> struct rebind { using other = LimitedDequeAllocator<U>; };
    LimitedDequeAllocator() = default;
    template <class U> LimitedDequeAllocator(const LimitedDequeAllocator<U>& a)
        : OwnerAllocator<T>(a.owner) {}
    std::size_t max_size() const noexcept {
        if constexpr (std::is_pointer_v<T>) return OwnerAllocator<T>::max_size();
        return 128;
    }
};

void test_insert_boundaries() {
    {
        mystl::deque<int, LimitedDequeAllocator<int>> d(128, 1);
        for (int mode = 0; mode < 3; ++mode) {
            bool threw = false;
            try {
                if (mode == 0) d.emplace_front(2);
                else if (mode == 1) d.emplace_back(2);
                else d.emplace(d.begin() + 3, 2);
            } catch (const mystl::length_error&) { threw = true; }
            CHECK(threw && d.size() == 128);
        }
    }
    Audit::clean();
    for (bool front : {false, true}) {
        {
            Audit::map_limit = 8;
            mystl::deque<int, OwnerAllocator<int>> d;
            for (int i = 0; i < 5 * 128; ++i) {
                if (front) d.emplace_front(i);
                else d.emplace_back(i);
            }
            CHECK(d.size() == 640);
            for (int i = 0; i < 640; ++i) CHECK(d[i] == (front ? 639 - i : i));
        }
        Audit::clean();
    }
    // Both ends need map growth; an aliased argument must survive relocation.
    for (bool front : {false, true}) {
        mystl::deque<int> d;
        d.assign(1023, 1);
        d[100] = 42;
        const auto index = front ? 3 : d.size() - 3;
        auto it = d.emplace(d.begin() + index, d[100]);
        CHECK(d.size() == 1024 && it == d.begin() + index && *it == 42);
        CHECK(d[front ? 101 : 100] == 42);
    }
    {
        mystl::deque<std::unique_ptr<int>> d;
        for (int i = 0; i < 260; ++i) d.emplace_back(std::make_unique<int>(i));
        d.emplace(d.begin() + 2, std::make_unique<int>(800));
        d.emplace(d.end() - 2, std::make_unique<int>(900));
        CHECK(*d[2] == 800 && *d[3] == 2 && *d[d.size() - 3] == 900 && *d.back() == 259);
    }
}

int main() {
    mystl::allocator<int> alloc;
    mystl::allocator<int*> map_alloc(alloc);
    auto map = map_alloc.allocate(8);
    map_alloc.deallocate(map, 8);
    { mystl::deque<int> scaffold; }
    test_storage_boundaries();
    test_iterators_and_map_growth();
    test_moves();
    test_nonzero_start_offset();
    test_exceptions();
    test_allocator_extended_move();
    test_assign_guards();
    test_assign_sequences();
    test_assignment_propagation<true, true>();
    test_assignment_propagation<true, false>();
    test_assignment_propagation<false, false>();
    test_assignment_propagation<false, true>();
    test_assignment_failures();
    test_insert_guards();
    test_modifier_sequences<int>();
    test_modifier_sequences<InsertLarge>();
    test_insert_boundaries();
    std::puts("deque base regression tests passed");
}
