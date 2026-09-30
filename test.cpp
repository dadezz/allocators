// AI generated

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "output_alloc.hpp"
#include "aligned_alloc.hpp"
#include "arena_alloc.hpp"
#include "budget_alloc.hpp"

#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

struct dummy {
    int a;
    int* b;
    std::string c;
    bool d;
    std::size_t e;
};

struct alignas(64) OverAligned {
    char c;
};

#if defined(_MSC_VER) && defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
constexpr bool kContainerProxy = true;
#else
constexpr bool kContainerProxy = false;
#endif

// ============================================================ helper

template <typename P>
std::uintptr_t addr(const P* p) {
    return reinterpret_cast<std::uintptr_t>(p);
}

template <typename P>
bool in_arena(const P* p, std::size_t n, const ArenaBase& arena) {
    auto begin = addr(arena.buffer_ptr);
    return addr(p) >= begin && addr(p) + n * sizeof(P) <= begin + arena.capacity;
}

template <typename P>
bool disjoint(const P* a, std::size_t na, const P* b, std::size_t nb) {
    return addr(a) + na * sizeof(P) <= addr(b) || addr(b) + nb * sizeof(P) <= addr(a);
}

// An n such that n * sizeof(T) does not fit in size_t.
template <typename T>
std::size_t huge_n() {
    return sizeof(T) > 1 ? std::size_t(-1) / sizeof(T) + 1 : std::size_t(-1);
}

template <typename Container>
void fill_and_verify(Container& c, int n) {
    for (int i = 0; i < n; ++i) c.push_back(i);
    int i = 0;
    bool ok = true;
    for (auto x : c) ok = ok && (x == i++);
    CHECK(ok);
    CHECK(i == n);
}

template <typename Container>
bool contains_sequence(const Container& c) {
    int i = 0;
    for (auto x : c)
        if (x != i++) return false;
    return true;
}

struct CoutCapture {
    std::ostringstream ss;
    std::streambuf* old;
    CoutCapture() : old(std::cout.rdbuf(ss.rdbuf())) {}
    ~CoutCapture() { std::cout.rdbuf(old); }
    std::string str() const { return ss.str(); }
};

constexpr std::size_t kCap = 1 << 20;

struct OutputFamily {
    template <typename T> using alloc = OutputAllocator<T>;
    template <typename T> alloc<T> get() { return {}; }
};

struct AlignedFamily {
    template <typename T> using alloc = AlignedAllocator<T, 64>;
    template <typename T> alloc<T> get() { return {}; }
};

struct ArenaFamily {
    template <typename T> using alloc = ArenaAllocator<T, kCap>;
    alloc<char> base;
    template <typename T> alloc<T> get() { return alloc<T>(base); }
};

struct ArenaViewFamily {
    template <typename T> using alloc = ArenaViewAllocator<T>;
    std::unique_ptr<Arena<kCap>> arena = std::make_unique<Arena<kCap>>();
    template <typename T> alloc<T> get() { return alloc<T>(arena.get()); }
};

struct BudgetFamily {
    template <typename T> using alloc = BudgetAllocator<T, kCap>;
    alloc<char> base;
    template <typename T> alloc<T> get() { return alloc<T>(base); }
};

TYPE_TO_STRING(OutputFamily);
TYPE_TO_STRING(AlignedFamily);
TYPE_TO_STRING(ArenaFamily);
TYPE_TO_STRING(ArenaViewFamily);
TYPE_TO_STRING(BudgetFamily);
TYPE_TO_STRING(dummy);

#define ALL_FAMILIES OutputFamily, AlignedFamily, ArenaFamily, ArenaViewFamily, BudgetFamily

template <typename F, typename Fn>
void for_each_type(F& family, Fn fn) {
    fn(family.template get<char>());
    fn(family.template get<short>());
    fn(family.template get<int>());
    fn(family.template get<long long>());
    fn(family.template get<double>());
    fn(family.template get<dummy>());
}

// ============================================================ common

TEST_CASE_TEMPLATE("common: basic allocate/deallocate", F, ALL_FAMILIES) {
    F family;
    for_each_type(family, [](auto a) {
        using T = typename decltype(a)::value_type;
        INFO(typeid(T).name());
        T* p = a.allocate(10);
        CHECK(p != nullptr);
        a.deallocate(p, 10);
    });
}

TEST_CASE_TEMPLATE("common: n = 0", F, ALL_FAMILIES) {
    F family;
    for_each_type(family, [](auto a) {
        using T = typename decltype(a)::value_type;
        INFO(typeid(T).name());
        T* p = nullptr;
        CHECK_NOTHROW(p = a.allocate(0));
        CHECK_NOTHROW(a.deallocate(p, 0));
    });
}

TEST_CASE_TEMPLATE("common: n * sizeof(T) overflow", F, ALL_FAMILIES) {
    F family;
    for_each_type(family, [](auto a) {
        using T = typename decltype(a)::value_type;
        INFO(typeid(T).name());
        CHECK_THROWS_AS(a.allocate(huge_n<T>()), std::bad_alloc);
    });
}

TEST_CASE_TEMPLATE("common: natural alignment", F, ALL_FAMILIES) {
    F family;
    for_each_type(family, [](auto a) {
        using T = typename decltype(a)::value_type;
        INFO(typeid(T).name());
        for (std::size_t n : {1, 3, 7}) {
            CAPTURE(n);
            T* p = a.allocate(n);
            CHECK(addr(p) % alignof(T) == 0);
            a.deallocate(p, n);
        }
    });
}

// Types whose alignas exceeds what ::operator new guarantees (16 bytes).
// Repeated 32 times because malloc can return an aligned address by chance.
TEST_CASE_TEMPLATE("common: over-aligned types", F, ALL_FAMILIES) {
    F family;
    auto a = family.template get<OverAligned>();
    bool all_aligned = true;
    OverAligned* ptrs[32];
    for (auto& p : ptrs) {
        p = a.allocate(1);
        all_aligned = all_aligned && addr(p) % alignof(OverAligned) == 0;
    }
    CHECK(all_aligned);
    for (auto p : ptrs) a.deallocate(p, 1);
}

TEST_CASE_TEMPLATE("common: live allocations do not overlap", F, ALL_FAMILIES) {
    F family;
    for_each_type(family, [](auto a) {
        using T = typename decltype(a)::value_type;
        INFO(typeid(T).name());
        T* p1 = a.allocate(8);
        T* p2 = a.allocate(8);
        CHECK(disjoint(p1, 8, p2, 8));
        a.deallocate(p2, 8);
        a.deallocate(p1, 8);
    });
}

TEST_CASE_TEMPLATE("common: data integrity", F, ALL_FAMILIES) {
    F family;
    for_each_type(family, [](auto a) {
        using T = typename decltype(a)::value_type;
        INFO(typeid(T).name());
        constexpr std::size_t n = 16;
        const std::size_t bytes = n * sizeof(T);

        T* p1 = a.allocate(n);
        std::memset(static_cast<void*>(p1), 0xAB, bytes);
        T* p2 = a.allocate(n);
        T* p3 = a.allocate(n);
        std::memset(static_cast<void*>(p2), 0xCD, bytes);
        std::memset(static_cast<void*>(p3), 0xEF, bytes);

        auto* raw = reinterpret_cast<unsigned char*>(p1);
        bool intact = true;
        for (std::size_t i = 0; i < bytes; ++i) intact = intact && raw[i] == 0xAB;
        CHECK(intact);

        a.deallocate(p3, n);
        a.deallocate(p2, n);
        a.deallocate(p1, n);
    });
}

TEST_CASE_TEMPLATE("common: containers", F, ALL_FAMILIES) {
    F family;
    using IntAlloc = typename F::template alloc<int>;

    SUBCASE("vector") {
        std::vector<int, IntAlloc> v(family.template get<int>());
        fill_and_verify(v, 1000);
    }
    SUBCASE("list (rebind to node type)") {
        std::list<int, IntAlloc> l(family.template get<int>());
        fill_and_verify(l, 1000);
    }
    SUBCASE("map (rebind to tree node type)") {
        using PairAlloc = typename F::template alloc<std::pair<const int, int>>;
        std::map<int, int, std::less<int>, PairAlloc> m(family.template get<std::pair<const int, int>>());
        for (int i = 0; i < 1000; ++i) m[i] = i * 2;
        bool ok = m.size() == 1000;
        for (int i = 0; i < 1000; ++i) ok = ok && m.at(i) == i * 2;
        CHECK(ok);
    }
}

TEST_CASE_TEMPLATE("common: copy, conversion and ==", F, ALL_FAMILIES) {
    F family;
    auto a = family.template get<int>();

    SUBCASE("reflexivity") {
        CHECK(a == a);
        auto copy = a;
        CHECK(copy == a);
        CHECK_FALSE(copy != a);
    }
    SUBCASE("conversion int -> double -> int") {
        typename F::template alloc<double> b(a);
        CHECK(b == a);
        CHECK(a == b);
        typename F::template alloc<int> c(b);
        CHECK(c == a);
    }
}

TEST_CASE_TEMPLATE("common: allocator_traits requirements (compile-time)", F, ALL_FAMILIES) {
    using A = typename F::template alloc<int>;
    using Traits = std::allocator_traits<A>;

    static_assert(std::is_same_v<typename Traits::value_type, int>);
    static_assert(std::is_same_v<typename Traits::pointer, int*>);
    static_assert(std::is_same_v<typename Traits::template rebind_alloc<double>,
                                 typename F::template alloc<double>>);
    static_assert(std::is_nothrow_copy_constructible_v<A>);
    static_assert(std::is_nothrow_move_constructible_v<A>);
    CHECK(true);  // doctest needs at least one runtime assertion to count the test
}

// ============================================================ OutputAllocator

TEST_CASE("output: stateless, all instances compare equal") {
    OutputAllocator<int> a;
    OutputAllocator<int> b;
    OutputAllocator<double> c;
    CHECK(a == b);
    CHECK(a == c);
    CHECK(c == a);
}

TEST_CASE("output: is_always_equal") {
    static_assert(std::allocator_traits<OutputAllocator<int>>::is_always_equal::value);
    CHECK(true);
}

TEST_CASE("output: allocate log reports elements and bytes") {
    OutputAllocator<int> a;
    std::string log;
    {
        CoutCapture capture;
        int* p = a.allocate(3);
        a.deallocate(p, 3);
        log = capture.str();
    }
    CAPTURE(log);
    CHECK(log.find("for 3 elements") != std::string::npos);
    CHECK(log.find(std::to_string(3 * sizeof(int)) + " bytes") != std::string::npos);
}

TEST_CASE("output: swap and move assignment between containers") {
    std::vector<int, OutputAllocator<int>> a, b;
    fill_and_verify(b, 100);

    a.swap(b);
    CHECK(a.size() == 100);
    CHECK(b.empty());
    CHECK(contains_sequence(a));

    b = std::move(a);
    CHECK(b.size() == 100);
    CHECK(contains_sequence(b));
}

// ============================================================ AlignedAllocator

template <typename T, std::size_t A>
void check_aligned_allocations() {
    AlignedAllocator<T, A> a;
    for (std::size_t n : {1, 3, 17, 1000}) {
        CAPTURE(A);
        CAPTURE(n);
        T* p = a.allocate(n);
        CHECK(addr(p) % A == 0);
        a.deallocate(p, n);
    }
}

TEST_CASE("aligned: requested alignment across sizes") {
    check_aligned_allocations<char, 16>();
    check_aligned_allocations<char, 32>();
    check_aligned_allocations<char, 64>();
    check_aligned_allocations<char, 128>();
    check_aligned_allocations<char, 4096>();
    check_aligned_allocations<double, 64>();
    check_aligned_allocations<dummy, 64>();
}

TEST_CASE("aligned: vector::data() aligned after every reallocation") {
    std::vector<double, AlignedAllocator<double, 64>> v;
    int reallocations = 0;
    bool always_aligned = true;
    for (int i = 0; i < 1000; ++i) {
        auto old_capacity = v.capacity();
        v.push_back(i);
        if (v.capacity() != old_capacity) {
            ++reallocations;
            always_aligned = always_aligned && addr(v.data()) % 64 == 0;
        }
    }
    CHECK(reallocations > 1);
    CHECK(always_aligned);
}

// list/map nodes are not accessible, but the element sits at a fixed offset
// inside the node: if every node is 64-aligned, all elements share the same
// address modulo 64.
TEST_CASE("aligned: list and map nodes are aligned") {
    SUBCASE("list") {
        std::list<int, AlignedAllocator<int, 64>> l;
        for (int i = 0; i < 100; ++i) l.push_back(i);
        auto residue = addr(&l.front()) % 64;
        bool same = true;
        for (auto& x : l) same = same && addr(&x) % 64 == residue;
        CHECK(same);
    }
    SUBCASE("map") {
        std::map<int, int, std::less<int>, AlignedAllocator<std::pair<const int, int>, 64>> m;
        for (int i = 0; i < 100; ++i) m[i] = i;
        auto residue = addr(&*m.begin()) % 64;
        bool same = true;
        for (auto& kv : m) same = same && addr(&kv) % 64 == residue;
        CHECK(same);
    }
}

TEST_CASE("aligned: == depends only on alignment") {
    CHECK(AlignedAllocator<int, 64>{} == AlignedAllocator<double, 64>{});
    CHECK(AlignedAllocator<int, 64>{} == AlignedAllocator<int, 64>{});
    CHECK_FALSE(AlignedAllocator<int, 32>{} == AlignedAllocator<int, 64>{});
}

TEST_CASE("aligned: rebind preserves alignment") {
    static_assert(std::is_same_v<AlignedAllocator<int, 64>::rebind<double>::other,
                                 AlignedAllocator<double, 64>>);
    static_assert(std::is_same_v<std::allocator_traits<AlignedAllocator<int, 128>>::rebind_alloc<char>,
                                 AlignedAllocator<char, 128>>);
    CHECK(true);
}

// An invalid alignAt (not a power of 2, or < alignof(T)) must not compile.
// This cannot be checked at runtime: once the static_assert is in place,
// verify it by hand by instantiating AlignedAllocator<int, 3>.

// ============================================================ ArenaAllocator

TEST_CASE("arena: sequential allocations") {
    ArenaAllocator<int, 1024> a;
    int* p1 = a.allocate(4);
    int* p2 = a.allocate(4);
    CHECK(addr(p2) >= addr(p1 + 4));
}

TEST_CASE("arena: every allocation lies inside the buffer") {
    ArenaAllocator<int, 1024> a;
    bool all_inside = true;
    int count = 0;
    try {
        for (std::size_t n = 1;; n = n % 7 + 1) {
            int* p = a.allocate(n);
            all_inside = all_inside && in_arena(p, n, *a.arena);
            ++count;
        }
    } catch (const std::bad_alloc&) {
    }
    CHECK(count > 0);
    CHECK(all_inside);
}

TEST_CASE("arena: exact fill") {
    ArenaAllocator<char, 64> a;
    CHECK_NOTHROW(a.allocate(64));
    CHECK(a.arena->offset == 64);
    CHECK_THROWS_AS(a.allocate(1), std::bad_alloc);

    ArenaAllocator<char, 64> b;
    CHECK_THROWS_AS(b.allocate(65), std::bad_alloc);
}

TEST_CASE("arena: exhaustion") {
    ArenaAllocator<int, 16> a;
    CHECK_THROWS_AS(a.allocate(5), std::bad_alloc);
    CHECK_NOTHROW(a.allocate(2));
    CHECK_THROWS_AS(a.allocate(3), std::bad_alloc);
}

TEST_CASE("arena: padding does not overrun the buffer") {
    // offset = 9, the double needs padding up to 16 > capacity 13
    ArenaAllocator<char, 13> a;
    (void)a.allocate(9);
    ArenaAllocator<double, 13> b(a);
    CHECK_THROWS_AS(b.allocate(1), std::bad_alloc);
}

TEST_CASE("arena: correct padding between mixed types") {
    ArenaAllocator<char, 256> c(ArenaAllocator<char, 256>{});
    ArenaAllocator<double, 256> d(c);
    ArenaAllocator<int, 256> i(c);

    char* p1 = c.allocate(1);
    double* p2 = d.allocate(1);
    char* p3 = c.allocate(3);
    int* p4 = i.allocate(1);

    CHECK(addr(p2) % alignof(double) == 0);
    CHECK(addr(p4) % alignof(int) == 0);
    CHECK(addr(p2) >= addr(p1 + 1));
    CHECK(addr(p3) >= addr(p2 + 1));
    CHECK(addr(p4) >= addr(p3 + 3));
}

TEST_CASE("arena: state unchanged after a failure") {
    ArenaAllocator<char, 13> a;
    (void)a.allocate(9);
    ArenaAllocator<double, 13> b(a);

    CHECK_THROWS_AS(b.allocate(1), std::bad_alloc);
    CHECK(a.arena->offset == 9);
    CHECK_NOTHROW(a.allocate(4));   // the remaining 4 bytes are still usable
    CHECK(a.arena->offset == 13);
}

TEST_CASE("arena: deallocate is a no-op") {
    ArenaAllocator<int, 1024> a;
    int* p = a.allocate(10);
    auto offset = a.arena->offset;
    a.deallocate(p, 10);
    CHECK(a.arena->offset == offset);
}

TEST_CASE("arena: reset starts over from the beginning") {
    ArenaAllocator<int, 1024> a;
    int* p = a.allocate(10);
    a.arena->reset();
    CHECK(a.arena->offset == 0);
    CHECK(a.allocate(10) == p);
}

TEST_CASE("arena: copies and rebinds share the arena") {
    ArenaAllocator<int, 1024> a;

    SUBCASE("copy") {
        auto copy = a;
        (void)copy.allocate(4);
        CHECK(a.arena->offset == 4 * sizeof(int));
        CHECK(a == copy);
    }
    SUBCASE("rebind") {
        ArenaAllocator<double, 1024> b(a);
        CHECK(b.arena == a.arena);
        CHECK(a == b);
    }
}

TEST_CASE("arena: independent instances are not equal") {
    ArenaAllocator<int, 1024> a, b;
    CHECK_FALSE(a == b);
    CHECK(a != b);
}

TEST_CASE("arena: the arena outlives the original while a copy exists") {
    std::optional<ArenaAllocator<int, 1024>> original(std::in_place);
    ArenaAllocator<int, 1024> copy(*original);
    int* p = original->allocate(4);
    p[0] = 42;

    original.reset();
    CHECK(copy.arena.use_count() == 1);
    CHECK(copy.arena->offset == 4 * sizeof(int));
    CHECK(p[0] == 42);
    CHECK_NOTHROW(copy.allocate(4));
}

TEST_CASE("arena: Arena is not copyable") {
    static_assert(!std::is_copy_constructible_v<Arena<16>>);
    static_assert(!std::is_copy_assignable_v<Arena<16>>);
    CHECK(true);
}

TEST_CASE("arena: vector grows until the arena is exhausted") {
    ArenaAllocator<int, 1024> a;
    std::vector<int, ArenaAllocator<int, 1024>> v(a);
    int i = 0;
    CHECK_THROWS_AS([&] { for (;;) v.push_back(i++); }(), std::bad_alloc);
    CHECK(v.size() > 0);
    CHECK(contains_sequence(v));   // push_back has the strong guarantee
}

TEST_CASE("arena: two containers on the same arena") {
    ArenaAllocator<int, 64 * 1024> a;
    std::vector<int, ArenaAllocator<int, 64 * 1024>> v1(a), v2(a);
    for (int i = 0; i < 500; ++i) {
        v1.push_back(i);
        v2.push_back(i);
    }
    CHECK(contains_sequence(v1));
    CHECK(contains_sequence(v2));
    CHECK(disjoint(v1.data(), v1.size(), v2.data(), v2.size()));
}

// ============================================================ ArenaViewAllocator

TEST_CASE("arena view: uses the external buffer") {
    alignas(std::max_align_t) std::byte buf[256];
    ArenaBase arena(buf, sizeof buf);
    ArenaViewAllocator<int> a(&arena);

    int* p = a.allocate(10);
    CHECK(in_arena(p, 10, arena));
    CHECK(addr(p) == addr(buf));
}

TEST_CASE("arena view: equality") {
    alignas(std::max_align_t) std::byte buf1[64], buf2[64];
    ArenaBase arena1(buf1, sizeof buf1), arena2(buf2, sizeof buf2);

    ArenaViewAllocator<int> a(&arena1), b(&arena1), c(&arena2);
    ArenaViewAllocator<double> d(a);
    CHECK(a == b);
    CHECK(a == d);
    CHECK_FALSE(a == c);
}

TEST_CASE("arena view: multiple views share the offset") {
    alignas(std::max_align_t) std::byte buf[256];
    ArenaBase arena(buf, sizeof buf);
    ArenaViewAllocator<int> a(&arena);
    ArenaViewAllocator<double> b(&arena);

    int* p1 = a.allocate(3);
    double* p2 = b.allocate(2);
    CHECK(addr(p2) >= addr(p1 + 3));
    CHECK(arena.offset >= 3 * sizeof(int) + 2 * sizeof(double));
}

TEST_CASE("arena view: external reset") {
    alignas(std::max_align_t) std::byte buf[256];
    ArenaBase arena(buf, sizeof buf);
    ArenaViewAllocator<int> a(&arena);

    int* p = a.allocate(10);
    arena.reset();
    CHECK(a.allocate(10) == p);
}

TEST_CASE("arena view: exhaustion and padding with runtime capacity") {
    alignas(std::max_align_t) std::byte buf[64];
    ArenaBase arena(buf, 13);
    ArenaViewAllocator<char> c(&arena);
    ArenaViewAllocator<double> d(&arena);

    CHECK_THROWS_AS(c.allocate(14), std::bad_alloc);
    (void)c.allocate(9);
    CHECK_THROWS_AS(d.allocate(1), std::bad_alloc);
    CHECK(arena.offset == 9);
}

// ============================================================ BudgetAllocator

constexpr std::size_t kBudget = 1024;

TEST_CASE_TEMPLATE("budget: accounting", T, int, long, short, char, double, float, dummy) {
    BudgetAllocator<T, kBudget> a;
    const std::size_t max_n = kBudget / sizeof(T);

    SUBCASE("budget goes down and back up") {
        T* p = a.allocate(2);
        CHECK(*a.free_space == kBudget - 2 * sizeof(T));
        a.deallocate(p, 2);
        CHECK(*a.free_space == kBudget);
    }
    SUBCASE("exact budget") {
        T* p = a.allocate(max_n);
        CHECK(*a.free_space < sizeof(T));
        CHECK_THROWS_AS(a.allocate(1), std::bad_alloc);
        a.deallocate(p, max_n);
        CHECK(*a.free_space == kBudget);
    }
    SUBCASE("over budget throws and leaves the counter untouched") {
        CHECK_THROWS_AS(a.allocate(max_n + 1), std::bad_alloc);
        CHECK(*a.free_space == kBudget);
    }
    SUBCASE("n * sizeof(T) overflow") {
        CHECK_THROWS_AS(a.allocate(huge_n<T>()), std::bad_alloc);
        if constexpr (sizeof(T) > 1) {
            // the old bug: n * sizeof(T) wrapped around to a few bytes
            CHECK_THROWS_AS(a.allocate(huge_n<T>() + 1), std::bad_alloc);
        }
        CHECK(*a.free_space == kBudget);
    }
}

TEST_CASE("budget: copies and rebinds share the budget") {
    BudgetAllocator<int, kBudget> a;

    SUBCASE("copy") {
        auto copy = a;
        CHECK(a == copy);
        int* p = copy.allocate(4);
        CHECK(*a.free_space == kBudget - 4 * sizeof(int));
        copy.deallocate(p, 4);
        CHECK(*a.free_space == kBudget);
    }
    SUBCASE("rebind") {
        BudgetAllocator<double, kBudget> b(a);
        CHECK(a == b);
        double* p = b.allocate(2);
        CHECK(*a.free_space == kBudget - 2 * sizeof(double));
        b.deallocate(p, 2);
    }
}

TEST_CASE("budget: independent instances are not equal even with the same remaining budget") {
    BudgetAllocator<int, kBudget> a, b;
    CHECK(*a.free_space == *b.free_space);
    CHECK_FALSE(a == b);
    CHECK(a != b);
}

TEST_CASE("budget: no budget leak with containers") {
    BudgetAllocator<int, kCap> a;
    {
        std::vector<int, BudgetAllocator<int, kCap>> v(a);
        fill_and_verify(v, 1000);
        std::list<int, BudgetAllocator<int, kCap>> l(a);
        fill_and_verify(l, 1000);
        std::map<int, int, std::less<int>, BudgetAllocator<std::pair<const int, int>, kCap>> m(a);
        for (int i = 0; i < 100; ++i) m[i] = i;
        CHECK(*a.free_space < kCap);
    }
    CHECK(*a.free_space == kCap);
}

TEST_CASE("budget: different containers draw from the same budget") {
    BudgetAllocator<int, kCap> a;
    std::vector<int, BudgetAllocator<int, kCap>> v(a);
    fill_and_verify(v, 100);
    auto after_vector = *a.free_space;
    CHECK(after_vector < kCap);

    std::map<int, int, std::less<int>, BudgetAllocator<std::pair<const int, int>, kCap>> m(a);
    for (int i = 0; i < 100; ++i) m[i] = i;
    CHECK(*a.free_space < after_vector);
}

TEST_CASE("budget: vector exceeding the budget") {
    BudgetAllocator<int, 64> a;
    std::vector<int, BudgetAllocator<int, 64>> v(a);
    int i = 0;
    CHECK_THROWS_AS([&] { for (;;) v.push_back(i++); }(), std::bad_alloc);

    CHECK(v.size() > 0);
    CHECK(contains_sequence(v));
    CHECK(*a.free_space <= 64);
    if (!kContainerProxy)
        CHECK(*a.free_space == 64 - v.capacity() * sizeof(int));
}

TEST_CASE("budget: fill, empty, refill without drift") {
    BudgetAllocator<int, kCap> a;
    for (int round = 0; round < 3; ++round) {
        CAPTURE(round);
        {
            std::vector<int, BudgetAllocator<int, kCap>> v(a);
            fill_and_verify(v, 500);
        }
        CHECK(*a.free_space == kCap);
    }
}

// Documents CURRENT behavior, not necessarily the intended one.
// Converting between different budgets compiles and shares the counter: the
// budget parameter of the new allocator is ignored. If you decide to forbid it,
// replace with static_assert(!std::is_constructible_v<...>).
TEST_CASE("budget: conversion between different budgets (current behavior)") {
    BudgetAllocator<int, 64> a;
    BudgetAllocator<int, 128> b(a);
    CHECK(b.free_space == a.free_space);
    CHECK(*b.free_space == 64);
}

// ============================================================ container semantics
// Stateful allocators that compare unequal (two distinct arenas). None of the
// allocators define propagate_on_container_*, so the defaults (false) apply.

using IntArena = ArenaAllocator<int, 64 * 1024>;

TEST_CASE("container: no propagation declared") {
    using Traits = std::allocator_traits<IntArena>;
    static_assert(!Traits::propagate_on_container_copy_assignment::value);
    static_assert(!Traits::propagate_on_container_move_assignment::value);
    static_assert(!Traits::propagate_on_container_swap::value);
    static_assert(!Traits::is_always_equal::value);
    CHECK(true);
}

TEST_CASE("container: copy assignment with unequal allocators") {
    IntArena arena_a, arena_b;
    std::vector<int, IntArena> va(arena_a), vb(arena_b);
    fill_and_verify(vb, 100);

    va = vb;
    CHECK(va.get_allocator() == arena_a);           // allocator not propagated
    CHECK(in_arena(va.data(), va.size(), *arena_a.arena));
    CHECK(contains_sequence(va));
}

TEST_CASE("container: move assignment with unequal allocators") {
    IntArena arena_a, arena_b;
    std::vector<int, IntArena> va(arena_a), vb(arena_b);
    fill_and_verify(vb, 100);

    va = std::move(vb);                             // element-wise move
    CHECK(va.get_allocator() == arena_a);
    CHECK(in_arena(va.data(), va.size(), *arena_a.arena));
    CHECK(va.size() == 100);
    CHECK(contains_sequence(va));
}

// swap between containers with unequal, non-propagating allocators is
// undefined behavior: not tested, just avoid it.

// ============================================================ main

// Discards everything written to std::cout (allocator logs).
struct NullBuffer : std::streambuf {
    int overflow(int c) override { return traits_type::not_eof(c); }
};

int main(int argc, char** argv) {
    std::ostream report(std::cout.rdbuf());   // doctest output goes here
    NullBuffer null_buffer;
    std::streambuf* original = std::cout.rdbuf(&null_buffer);

    doctest::Context context(argc, argv);
    context.setCout(&report);
    int result = context.run();

    std::cout.rdbuf(original);
    return result;
}
