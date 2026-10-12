#include "../src/impl/rb_tree.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #__VA_ARGS__); \
    std::abort(); } } while (false)

namespace {
struct AdlOnly {
    int value;
    explicit AdlOnly(int n) : value(n) {}
    AdlOnly(AdlOnly&&) = delete;
    AdlOnly& operator=(AdlOnly&&) = delete;
    friend void swap(AdlOnly& a, AdlOnly& b) noexcept {
        mystl::swap(a.value, b.value);
    }
};

struct Unswappable {
    Unswappable(Unswappable&&) = delete;
    Unswappable& operator=(Unswappable&&) = delete;
};

struct Compare {
    bool reverse = false;
    bool operator()(int a, int b) const { return reverse ? a > b : a < b; }
    friend void swap(Compare& a, Compare& b) noexcept {
        mystl::swap(a.reverse, b.reverse);
    }
};

struct Key {
    int offset = 0;
    int operator()(int v) const { return v + offset; }
    friend void swap(Key& a, Key& b) noexcept { mystl::swap(a.offset, b.offset); }
};

struct ThrowingSwapCompare : Compare {
    friend void swap(ThrowingSwapCompare&, ThrowingSwapCompare&) noexcept(false) {}
};
struct ThrowingSwapKey : Key {
    friend void swap(ThrowingSwapKey&, ThrowingSwapKey&) noexcept(false) {}
};

template<class T>
concept HasMemberSwap = requires(T& a, T& b) { a.swap(b); };

using Tree = mystl::rb_tree<int, int, Key, Compare>;
static_assert(HasMemberSwap<Tree>);
static_assert(noexcept(mystl::declval<Tree&>().swap(mystl::declval<Tree&>())));
static_assert(!HasMemberSwap<mystl::rb_tree<int, int, Key, ThrowingSwapCompare>>);
static_assert(!HasMemberSwap<mystl::rb_tree<int, int, ThrowingSwapKey, Compare>>);
static_assert(mystl::is_nothrow_swappable_v<int[2][3]>);
static_assert(mystl::is_nothrow_swappable_v<AdlOnly[2][2]>);
static_assert(!mystl::is_swappable_v<const int[2]>);
static_assert(!mystl::is_nothrow_swappable_v<const int[2]>);
static_assert(!mystl::is_swappable_v<Unswappable[2]>);
static_assert(!mystl::is_nothrow_swappable_v<Unswappable[2]>);
static_assert(!mystl::is_swappable_v<void>);
static_assert(!mystl::is_nothrow_swappable_v<void()>);
static_assert(mystl::is_swappable_v<ThrowingSwapCompare[2]>);
static_assert(!mystl::is_nothrow_swappable_v<ThrowingSwapCompare[2]>);

struct Counted {
    static inline int alive = 0;
    int value;
    explicit Counted(int n) : value(n) { ++alive; }
    Counted(const Counted&) = delete;
    Counted(Counted&&) = delete;
    ~Counted() { --alive; }
};
struct CountedKey {
    int operator()(const Counted& v) const { return v.value; }
};
template<class T>
struct TinyAllocator : mystl::allocator<T> {
    template<class U> struct rebind { using other = TinyAllocator<U>; };
    TinyAllocator() = default;
    template<class U> TinyAllocator(const TinyAllocator<U>&) {}
    std::size_t max_size() const noexcept { return 1; }
};
struct ThrowingCompare {
    static inline bool fail = false;
    bool operator()(int a, int b) const {
        if (fail) throw 42;
        return a < b;
    }
};

void test_hint_lifetime() {
    using TinyTree = mystl::rb_tree<int, Counted, CountedKey,
                                   mystl::less<int>, TinyAllocator<Counted>>;
    {
        TinyTree t;
        t.emplace_hint(t.end(), 2);
        const auto* original = &*t.begin();
        for (int key : {1, 3}) {
            bool threw = false;
            try {
                t.emplace_hint(key == 1 ? t.begin() : t.end(), key);
            } catch (const mystl::length_error&) {
                threw = true;
            }
            CHECK(threw && t.size() == 1 && Counted::alive == 1);
            CHECK(&*t.begin() == original);
        }
        CHECK(&*t.emplace_hint(t.begin(), 2) == original);
        CHECK(&*t.emplace_hint(t.end(), 2) == original);
        CHECK(Counted::alive == 1 && t.size() == 1);
    }
    CHECK(Counted::alive == 0);
    {
        mystl::rb_tree<int, Counted, CountedKey, ThrowingCompare> t;
        t.emplace_hint(t.end(), 2);
        ThrowingCompare::fail = true;
        bool threw = false;
        try { t.emplace_hint(t.begin(), 1); } catch (int) { threw = true; }
        ThrowingCompare::fail = false;
        CHECK(threw && t.size() == 1 && Counted::alive == 1);
        t.emplace_hint(t.begin(), 1);
        t.emplace_hint(t.end(), 3);
        t.emplace_hint(t.begin(), 4); // Bad hint: reuse the node in the full search.
        t.emplace_hint(t.begin(), 3); // Duplicate: destroy the unused node.
        CHECK(t.size() == 4 && Counted::alive == 4);
    }
    CHECK(Counted::alive == 0);
}

void test_swap() {
    AdlOnly a[2][2] = {{AdlOnly(1), AdlOnly(2)}, {AdlOnly(3), AdlOnly(4)}};
    AdlOnly b[2][2] = {{AdlOnly(5), AdlOnly(6)}, {AdlOnly(7), AdlOnly(8)}};
    mystl::swap(a, b);
    CHECK(a[0][0].value == 5 && a[1][1].value == 8);
    CHECK(b[0][0].value == 1 && b[1][1].value == 4);

    Tree x(Compare{false}, {}, Key{0});
    Tree y(Compare{true}, {}, Key{100});
    x.swap(x);
    CHECK(x.begin() == x.end());
    x.swap(y);
    CHECK(x.begin() == x.end() && y.begin() == y.end());
    x.swap(y);
    for (int k : {1, 2, 3}) x.emplace_hint(x.end(), k);
    for (int k : {4, 5, 6}) y.emplace_hint(y.begin(), k);
    auto saved = x.find(1);
    x.swap(y);
    CHECK(y.find(1) == saved && x.contains(104) && x.contains(106));
    CHECK(x.key_comp().reverse && !y.key_comp().reverse);
    ++saved;
    CHECK(*saved == 2);
    Tree empty;
    x.swap(empty);
    CHECK(x.empty() && x.begin() == x.end() && x.rbegin() == x.rend());
    CHECK(empty.contains(104));
    x.emplace_hint(x.end(), 9);
    CHECK(x.contains(9));
}

void test_hint_and_lookup() {
    for (bool reverse : {false, true}) {
        Tree t(Compare{reverse});
        std::set<int, Compare> expected(Compare{reverse});
        const auto& c = t;
        auto check = [&](int key) {
            auto match = [&](auto actual, auto ref) {
                CHECK((actual == t.end()) == (ref == expected.end()));
                if (ref != expected.end()) CHECK(*actual == *ref);
            };
            match(t.lower_bound(key), expected.lower_bound(key));
            match(c.lower_bound(key), expected.lower_bound(key));
            match(t.upper_bound(key), expected.upper_bound(key));
            match(c.upper_bound(key), expected.upper_bound(key));
            auto [lo, hi] = t.equal_range(key);
            auto [clo, chi] = c.equal_range(key);
            auto [rlo, rhi] = expected.equal_range(key);
            match(lo, rlo); match(hi, rhi); match(clo, rlo); match(chi, rhi);
            CHECK(c.count(key) == expected.count(key));
            CHECK(c.contains(key) == expected.contains(key));
            CHECK(t.size() == expected.size());
            auto it = t.begin();
            for (int value : expected) { CHECK(it != t.end() && *it == value); ++it; }
            CHECK(it == t.end());
        };
        check(0);
        std::mt19937 rng(1012);
        for (int i = 0; i < 3000; ++i) {
            int key = rng() % 200;
            if (rng() % 2) {
                auto hint = t.begin();
                for (auto n = rng() % (t.size() + 1); n; --n) ++hint;
                CHECK(*t.emplace_hint(hint, key) == key);
                expected.insert(key);
            } else {
                CHECK(t.erase(key) == expected.erase(key));
            }
            check(key); check(-1); check(201);
        }
    }
}
} // namespace

int main() {
    test_hint_lifetime();
    test_swap();
    test_hint_and_lookup();
}
