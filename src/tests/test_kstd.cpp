#include <gtest/gtest.h>
#include <unpd/kstd/concepts.hpp>
#include <unpd/kstd/span.hpp>
#include <unpd/kstd/expected.hpp>
#include <unpd/kstd/unique_ptr.hpp>

using namespace unpd::kstd;

TEST(KstdTest, ConceptsVerification) {
    static_assert(same_as<int, int>);
    static_assert(!same_as<int, double>);
    static_assert(integral<uint64_t>);
    static_assert(integral<int32_t>);
    static_assert(!integral<float>);
    static_assert(pointer<void*>);
    static_assert(pointer<const char*>);
    static_assert(!pointer<int>);
    static_assert(trivially_copyable<uint64_t>);
    SUCCEED();
}

TEST(KstdTest, SpanSubspanAndIterators) {
    int data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    span<int> s(data, 10);

    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.size_bytes(), 10 * sizeof(int));
    EXPECT_FALSE(s.empty());

    int count = 0;
    for (int x : s) {
        EXPECT_EQ(x, count++);
    }

    auto sub = s.subspan(3, 4);
    EXPECT_EQ(sub.size(), 4);
    EXPECT_EQ(sub[0], 3);
    EXPECT_EQ(sub[3], 6);

    auto outOfBoundsSub = s.subspan(100);
    EXPECT_TRUE(outOfBoundsSub.empty());
}

TEST(KstdTest, ExpectedSuccessAndErrorMonad) {
    expected<int, int> ok(100);
    EXPECT_TRUE(ok.has_value());
    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_EQ(ok.value(), 100);
    EXPECT_EQ(ok.value_or(0), 100);

    expected<int, int> err = expected<int, int>::error(404);
    EXPECT_FALSE(err.has_value());
    EXPECT_FALSE(static_cast<bool>(err));
    EXPECT_EQ(err.error(), 404);
    EXPECT_EQ(err.value_or(999), 999);
}

TEST(KstdTest, ExpectedVoidSpecialization) {
    expected<void, int> ok = expected<void, int>::success();
    EXPECT_TRUE(ok.has_value());
    EXPECT_TRUE(static_cast<bool>(ok));

    expected<void, int> err = expected<void, int>::error(500);
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(err.error(), 500);
}

TEST(KstdTest, UniquePtrRaiiManagement) {
    struct TestObject {
        int Value = 0;
        explicit TestObject(int v) : Value(v) {}
    };

    auto* raw = new TestObject(42);
    struct CustomDeleter {
        void operator()(TestObject* p) const noexcept {
            delete p;
        }
    };

    unique_ptr<TestObject, CustomDeleter> ptr(raw);
    EXPECT_TRUE(static_cast<bool>(ptr));
    EXPECT_EQ(ptr->Value, 42);
    EXPECT_EQ((*ptr).Value, 42);

    TestObject* released = ptr.release();
    EXPECT_EQ(released, raw);
    EXPECT_FALSE(static_cast<bool>(ptr));
    delete released;
}
