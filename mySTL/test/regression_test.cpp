#include "../src/memory.h"
#include "../src/functional.h"
#include "../src/stdexcept.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <forward_list>
#include <string>
#include <stdexcept>
#include <type_traits>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #__VA_ARGS__); \
    std::abort(); } } while (false)

static_assert(mystl::strspn("abca!", "abc") == 4);
static_assert(mystl::strspn("abc", "") == 0);
static_assert(mystl::strcspn("abca!", "!") == 4);
static_assert(mystl::strcspn("", "abc") == 0);
static_assert(mystl::strpbrk("abc", "x") == nullptr);
static_assert(*mystl::strpbrk("abc", "cb") == 'b');
constexpr int permutation_source[] = {1, 2, 1};
constexpr int permutation_match[] = {1, 1, 2};
constexpr int permutation_mismatch[] = {1, 2, 2};
static_assert(mystl::is_permutation(permutation_source, permutation_source + 3, permutation_match));
static_assert(!mystl::is_permutation(permutation_source, permutation_source + 3, permutation_mismatch));
static_assert(mystl::is_permutation(permutation_source, permutation_source, permutation_match));

static_assert(mystl::is_same_v<mystl::remove_cv_ref_t<const int&>, int>);
static_assert(mystl::is_same_v<mystl::remove_pointer_t<const int*>, const int>);
static_assert(mystl::is_same_v<mystl::remove_pointer_t<int* const volatile>, int>);
static_assert(mystl::true_type{}() && !mystl::false_type{}());
static_assert(mystl::is_random_access_iterator_v<int*>);
static_assert(mystl::is_contiguous_iterator_v<const int*>);
static_assert(!mystl::is_iterator_v<int>);
static_assert(!mystl::is_iterator_v<void*>);
static_assert(!mystl::is_iterator_v<void(*)()>);
static_assert(mystl::is_same_v<mystl::iter_reference<const int*>, const int&>);
static_assert(mystl::is_same_v<mystl::allocator_traits<mystl::allocator<int>>::rebind_alloc<double>,
                              mystl::allocator<double>>);
static_assert(mystl::allocator_traits<mystl::allocator<int>>::propagate_on_container_move_assignment::value);
struct StatefulAllocator : mystl::allocator<int> {
    using is_always_equal = mystl::false_type;
    using propagate_on_container_move_assignment = mystl::false_type;
    using propagate_on_container_copy_assignment = mystl::true_type;
    using propagate_on_container_swap = mystl::true_type;
    int id = 0;
};
static_assert(!mystl::allocator_traits<StatefulAllocator>::is_always_equal::value);
static_assert(!mystl::allocator_traits<StatefulAllocator>::propagate_on_container_move_assignment::value);
static_assert(mystl::allocator_traits<StatefulAllocator>::propagate_on_container_copy_assignment::value);
static_assert(mystl::allocator_traits<StatefulAllocator>::propagate_on_container_swap::value);

struct ForwardIterator : mystl::iterator<mystl::forward_iterator_tag, int> {
    int* p{};
    int& operator*() const { return *p; }
    ForwardIterator& operator++() { ++p; return *this; }
    bool operator==(const ForwardIterator& other) const { return p == other.p; }
};
static_assert(mystl::is_forward_iterator_v<ForwardIterator>);
static_assert(!mystl::is_random_access_iterator_v<ForwardIterator>);

struct Counted {
    static inline int alive = 0;
    static inline int copies_before_throw = -1;
    int value;
    explicit Counted(int n = 0) : value(n) { ++alive; }
    Counted(const Counted& other) : value(other.value) {
        if (copies_before_throw == 0) throw std::runtime_error("copy");
        if (copies_before_throw > 0) --copies_before_throw;
        ++alive;
    }
    Counted(Counted&& other) noexcept : value(other.value) { other.value = -1; ++alive; }
    ~Counted() { --alive; }
};

struct HookAllocator : mystl::allocator<int> {
    int constructed = 0;
    int destroyed = 0;
    int remaining = -1;
    template <typename... Args>
    void construct(int* p, Args&&... args) {
        if (remaining == 0) throw std::runtime_error("allocator construct");
        if (remaining > 0) --remaining;
        mystl::construct(p, mystl::forward<Args>(args)...);
        ++constructed;
    }
    void destroy(int*) { ++destroyed; }
};

void test_memory() {
    const int source[] = {3, 5, 8};
    int* dest = mystl::allocator<int>::allocate(3);
    mystl::allocator<int> alloc;
    // All eight copy/move entry points must instantiate and return the end.
    CHECK(mystl::uninitialized_copy(source, source + 3, dest) == dest + 3);
    CHECK(mystl::uninitialized_copy_n(source, 3, dest) == dest + 3);
    CHECK(mystl::uninitialized_copy_a(source, source + 3, dest, alloc) == dest + 3);
    CHECK(mystl::uninitialized_copy_n_a(source, 3, dest, alloc) == dest + 3);
    CHECK(mystl::uninitialized_move(source, source + 3, dest) == dest + 3);
    CHECK(mystl::uninitialized_move_n(source, 3, dest) == dest + 3);
    CHECK(mystl::uninitialized_move_a(source, source + 3, dest, alloc) == dest + 3);
    CHECK(mystl::uninitialized_move_n_a(source, 3, dest, alloc) == dest + 3);
    CHECK(dest[0] == 3 && dest[1] == 5 && dest[2] == 8);
    CHECK(mystl::uninitialized_copy(static_cast<int*>(nullptr), static_cast<int*>(nullptr),
                                  static_cast<int*>(nullptr)) == nullptr);
    CHECK(mystl::uninitialized_move_n(static_cast<int*>(nullptr), 0, static_cast<int*>(nullptr)) == nullptr);
    CHECK(mystl::uninitialized_value_construct_n(dest, 3) == dest + 3);
    CHECK(dest[0] == 0 && dest[1] == 0 && dest[2] == 0);
    CHECK(mystl::uninitialized_fill_n(dest, 3, 6) == dest + 3);
    CHECK(dest[0] == 6 && dest[2] == 6);

    int values[] = {9, 10, 11};
    ForwardIterator first{{}, values}, last{{}, values + 3}, output{{}, dest};
    CHECK(mystl::distance(first, last) == 3);
    CHECK(mystl::uninitialized_copy(first, last, output).p == dest + 3);
    CHECK(dest[0] == 9 && dest[2] == 11);

    HookAllocator hooks;
    CHECK(mystl::uninitialized_copy_a(source, source + 3, dest, hooks) == dest + 3);
    CHECK(hooks.constructed == 3);
    mystl::destroy_a(dest, dest + 3, hooks);
    CHECK(hooks.destroyed == 3);
    hooks.remaining = 1;
    bool threw = false;
    try { mystl::uninitialized_copy_n_a(source, 3, dest, hooks); }
    catch (const std::runtime_error&) { threw = true; }
    CHECK(threw && hooks.constructed == 4 && hooks.destroyed == 4);
    hooks.remaining = -1;
    mystl::uninitialized_default_construct_n_a(dest, 3, hooks);
    CHECK(hooks.constructed == 7 && dest[0] == 0);
    mystl::destroy_a(dest, dest + 3, hooks);
    mystl::uninitialized_fill_a(dest, dest + 3, 12, hooks);
    CHECK(hooks.constructed == 10 && dest[2] == 12);
    mystl::destroy_a(dest, dest + 3, hooks);
    CHECK(hooks.destroyed == 10);
    mystl::allocator<int>::deallocate(dest, 3);

    auto* bytes = mystl::allocator<char>::allocate(3);
    CHECK(mystl::uninitialized_fill_n(bytes, 3, 'x') == bytes + 3);
    CHECK(bytes[0] == 'x' && bytes[2] == 'x');
    mystl::allocator<char>::deallocate(bytes, 3);
    struct ReadOnly { const int value; };
    auto* readonly = mystl::allocator<ReadOnly>::allocate(2);
    mystl::uninitialized_fill(readonly, readonly + 2, ReadOnly{7});
    CHECK(readonly[0].value == 7 && readonly[1].value == 7);
    mystl::allocator<ReadOnly>::deallocate(readonly, 2);

    {
        Counted src[] = {Counted(1), Counted(2), Counted(3)};
        auto* p = mystl::allocator<Counted>::allocate(3);
        Counted::copies_before_throw = 1;
        threw = false;
        try { mystl::uninitialized_copy(src, src + 3, p); }
        catch (const std::runtime_error&) { threw = true; }
        CHECK(threw && Counted::alive == 3);
        Counted::copies_before_throw = -1;
        mystl::uninitialized_move(src, src + 3, p);
        CHECK(Counted::alive == 6 && p[2].value == 3);
        mystl::destroy(p, p + 3);
        mystl::allocator<Counted>::deallocate(p, 3);
    }
    CHECK(Counted::alive == 0);
    {
        std::string src[] = {"one", "two"};
        auto* p = mystl::allocator<std::string>::allocate(2);
        mystl::uninitialized_copy(src, src + 2, p);
        CHECK(p[0] == "one" && p[1] == "two");
        mystl::destroy(p, p + 2);
        mystl::allocator<std::string>::deallocate(p, 2);
    }
    struct alignas(128) Aligned { int value; };
    auto* aligned = mystl::allocator<Aligned>::allocate(2);
    CHECK(reinterpret_cast<std::uintptr_t>(aligned) % alignof(Aligned) == 0);
    mystl::allocator<Aligned>::deallocate(aligned, 2);
}

struct DeleteInt {
    using pointer = int*;
    int* calls;
    void operator()(int* p) const { ++*calls; delete p; }
};
void test_unique_ptr() {
    {
        mystl::unique_ptr<Counted> p(new Counted(42));
        mystl::unique_ptr<Counted> q(new Counted(7));
        q = mystl::move(p);
        CHECK(!p && q->value == 42 && Counted::alive == 1);
        auto* raw = q.release();
        CHECK(!q);
        delete raw;
        mystl::unique_ptr<Counted[]> a(new Counted[3]);
        mystl::unique_ptr<Counted[]> b;
        b = mystl::move(a);
        CHECK(!a && Counted::alive == 3 && b[0].value == 0);
    }
    CHECK(Counted::alive == 0);
    int calls1 = 0, calls2 = 0;
    {
        mystl::unique_ptr<int, DeleteInt> p(new int(1), DeleteInt{&calls1});
        mystl::unique_ptr<int, DeleteInt> q(new int(2), DeleteInt{&calls2});
        q = mystl::move(p);
        CHECK(calls2 == 1 && *q == 1);
        q.reset();
        CHECK(calls1 == 1);
    }
    CHECK(calls1 == 1 && calls2 == 1);
}

struct SmallCallable {
    static inline int alive = 0;
    static inline bool throw_copy = false;
    const SmallCallable* self;
    int value;
    explicit SmallCallable(int n) : self(this), value(n) { ++alive; }
    SmallCallable(const SmallCallable& other) : self(this), value(other.value) {
        if (throw_copy) throw std::runtime_error("callable copy");
        ++alive;
    }
    SmallCallable(SmallCallable&& other) noexcept : self(this), value(other.value) { ++alive; }
    ~SmallCallable() { CHECK(self == this); --alive; }
    int operator()() const { CHECK(self == this); return value; }
};
struct LargeCallable : SmallCallable {
    char padding[128]{};
    using SmallCallable::SmallCallable;
};
struct ThrowingConversion { operator int() const { throw std::runtime_error("conversion"); } };
struct ConversionCallable { ThrowingConversion operator()() const noexcept { return {}; } };
static_assert(mystl::is_invocable_r_v<int, ConversionCallable>);
static_assert(!mystl::is_nothrow_invocable_r_v<int, ConversionCallable>);
static_assert(!mystl::is_nothrow_invocable_v<int>);
struct RefCallable { int operator()() & { return 5; } };
static_assert(mystl::is_invocable_v<RefCallable&>);
static_assert(!mystl::is_invocable_v<RefCallable>);

void test_function() {
    {
        mystl::function<int()> a(SmallCallable(1)), b(SmallCallable(2));
        a.swap(b);
        CHECK(a() == 2 && b() == 1);
        b = mystl::move(a);
        CHECK(!a && b() == 2 && SmallCallable::alive == 1);
        mystl::function<int()> large(LargeCallable(3));
        b.swap(large);
        CHECK(b() == 3 && large() == 2);
        auto copied = b;
        CHECK(copied() == 3);
        CHECK(b.target<LargeCallable>() != nullptr);
        CHECK(b.target<SmallCallable>() == nullptr);
        CHECK(b.target<int>() == nullptr);
        const auto& constant = copied;
        CHECK(constant.target<LargeCallable>()->value == 3);
        SmallCallable::throw_copy = true;
        bool threw = false;
        try { large = copied; } catch (const std::runtime_error&) { threw = true; }
        SmallCallable::throw_copy = false;
        CHECK(threw && large() == 2 && copied() == 3);
        auto* same = &b;
        b = mystl::move(*same);
        CHECK(b() == 3);
        mystl::function<int()> empty;
        b.swap(empty);
        CHECK(!b && empty() == 3);
    }
    CHECK(SmallCallable::alive == 0);
    mystl::function<void()> empty;
    bool threw = false;
    try { empty(); } catch (const mystl::bad_function_call&) { threw = true; }
    CHECK(threw);
    mystl::function<void()> throwing([] { throw std::runtime_error("target"); });
    threw = false;
    try { throwing(); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    mystl::function<int()> conversion{ConversionCallable{}};
    threw = false;
    try { (void)conversion(); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    int (*null_function)() = nullptr;
    mystl::function<int()> null_target(null_function);
    CHECK(!null_target);
    mystl::function<int()> ref_qualified{RefCallable{}};
    CHECK(ref_qualified() == 5);
    int n = 0;
    mystl::invoke_r<void>([&] { ++n; return 1; });
    CHECK(n == 1);
    struct Object { int value = 4; int add(int x) { return value + x; } };
    Object object;
    CHECK(mystl::invoke(&Object::add, object, 3) == 7);
    CHECK(mystl::invoke(&Object::value, &object) == 4);
    mystl::function<int(Object&, int)> member(&Object::add);
    CHECK(member(object, 2) == 6);
}

void test_cstring_and_exceptions() {
    const char high_bytes[] = {'\x80', '\xff', 'a', '\0'};
    CHECK(mystl::strspn(high_bytes, "\x80\xff") == 2);
    CHECK(mystl::strcspn(high_bytes, "\xff") == 1);
    CHECK(mystl::strpbrk(high_bytes, "\xff") == high_bytes + 1);
    char tokens[] = ",alpha,,beta;gamma;";
    CHECK(mystl::strcmp(mystl::strtok(tokens, ",;"), "alpha") == 0);
    CHECK(mystl::strcmp(mystl::strtok(nullptr, ",;"), "beta") == 0);
    CHECK(mystl::strcmp(mystl::strtok(nullptr, ",;"), "gamma") == 0);
    CHECK(mystl::strtok(nullptr, ",;") == nullptr);
    CHECK(mystl::strtok(nullptr, ",;") == nullptr);
    const char text[] = "abc";
    CHECK(mystl::memchr(text, 'b', 3) == text + 1);
    CHECK(mystl::memchr(text, 'x', 3) == nullptr);
    CHECK(mystl::memchr(text, 'a', 0) == nullptr);
    CHECK(mystl::strncmp("a", "b", 1) < 0);
    CHECK(mystl::strncmp("b", "a", 1) > 0);
    CHECK(mystl::strncmp("ab", "ac", 1) == 0);
    bool caught = false;
    try { throw mystl::out_of_range("bounds"); }
    catch (const mystl::exception& ex) { caught = mystl::strcmp(ex.what(), "bounds") == 0; }
    CHECK(caught);
    mystl::exception original;
    mystl::exception copy(original);
    CHECK(mystl::strcmp(copy.what(), "mystl::exception") == 0);
}

void test_permutation() {
    const std::forward_list<int> source{1, 2, 1, 3};
    const std::forward_list<int> matching{3, 1, 2, 1};
    const std::forward_list<int> different{3, 1, 2, 2};
    CHECK(mystl::is_permutation(source.begin(), source.end(), matching.begin()));
    CHECK(!mystl::is_permutation(source.begin(), source.end(), different.begin()));
    // Compare all three-element sequences over {0, 1, 2}, including duplicates.
    for (int a = 0; a < 27; ++a) {
        int first[] = {a / 9, a / 3 % 3, a % 3};
        for (int b = 0; b < 27; ++b) {
            int second[] = {b / 9, b / 3 % 3, b % 3};
            CHECK(mystl::is_permutation(first, first + 3, second) ==
                  std::is_permutation(first, first + 3, second));
        }
    }
}

int main() {
    test_memory();
    test_unique_ptr();
    test_function();
    test_cstring_and_exceptions();
    test_permutation();
    std::puts("core regression tests passed");
}
