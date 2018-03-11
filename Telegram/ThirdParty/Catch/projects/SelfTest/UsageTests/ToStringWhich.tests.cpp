/*
 * Demonstrate which version of toString/StringMaker is being used
 * for various types
 */

// Replace fallback stringifier for this TU
// We should avoid ODR violations because these specific types aren't
// present in different TUs
#include <string>
template <typename T>
std::string fallbackStringifier(T const&) {
    return "{ !!! }";
}

#define CATCH_CONFIG_FALLBACK_STRINGIFIER fallbackStringifier
#include "catch.hpp"


struct has_operator { };
struct has_maker {};
struct has_maker_and_operator {};
struct has_neither {};

std::ostream& operator<<(std::ostream& os, const has_operator&) {
    os << "operator<<( has_operator )";
    return os;
}

std::ostream& operator<<(std::ostream& os, const has_maker_and_operator&) {
    os << "operator<<( has_maker_and_operator )";
    return os;
}

namespace Catch {
    template<>
    struct StringMaker<has_maker> {
        static std::string convert( const has_maker& ) {
            return "StringMaker<has_maker>";
        }
    };
    template<>
    struct StringMaker<has_maker_and_operator> {
        static std::string convert( const has_maker_and_operator& ) {
            return "StringMaker<has_maker_and_operator>";
        }
    };
}

// Call the operator
TEST_CASE( "stringify( has_operator )", "[toString]" ) {
    has_operator item;
    REQUIRE( ::Catch::Detail::stringify( item ) == "operator<<( has_operator )" );
}

// Call the stringmaker
TEST_CASE( "stringify( has_maker )", "[toString]" ) {
    has_maker item;
    REQUIRE( ::Catch::Detail::stringify( item ) == "StringMaker<has_maker>" );
}

// Call the stringmaker
TEST_CASE( "stringify( has_maker_and_operator )", "[toString]" ) {
    has_maker_and_operator item;
    REQUIRE( ::Catch::Detail::stringify( item ) == "StringMaker<has_maker_and_operator>" );
}

TEST_CASE("stringify( has_neither )", "[toString]") {
    has_neither item;
    REQUIRE( ::Catch::Detail::stringify(item) == "{ !!! }" );
}


// Vectors...

TEST_CASE( "stringify( vectors<has_operator> )", "[toString]" ) {
    std::vector<has_operator> v(1);
    REQUIRE( ::Catch::Detail::stringify( v ) == "{ operator<<( has_operator ) }" );
}

TEST_CASE( "stringify( vectors<has_maker> )", "[toString]" ) {
    std::vector<has_maker> v(1);
    REQUIRE( ::Catch::Detail::stringify( v ) == "{ StringMaker<has_maker> }" );
}

TEST_CASE( "stringify( vectors<has_maker_and_operator> )", "[toString]" ) {
    std::vector<has_maker_and_operator> v(1);
    REQUIRE( ::Catch::Detail::stringify( v ) == "{ StringMaker<has_maker_and_operator> }" );
}

// Range-based conversion should only be used if other possibilities fail
struct int_iterator {
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = int;
    using reference = int&;
    using pointer = int*;

    int_iterator() = default;
    int_iterator(int i) :val(i) {}

    value_type operator*() const { return val; }
    bool operator==(int_iterator rhs) const { return val == rhs.val; }
    bool operator!=(int_iterator rhs) const { return val != rhs.val; }
    int_iterator operator++() { ++val; return *this; }
    int_iterator operator++(int) {
        auto temp(*this);
        ++val;
        return temp;
    }
private:
    int val = 5;
};

struct streamable_range {
    int_iterator begin() const { return int_iterator{ 1 }; }
    int_iterator end() const { return {}; }
};

std::ostream& operator<<(std::ostream& os, const streamable_range&) {
    os << "op<<(streamable_range)";
    return os;
}

struct stringmaker_range {
    int_iterator begin() const { return int_iterator{ 1 }; }
    int_iterator end() const { return {}; }
};

namespace Catch {
template <>
struct StringMaker<stringmaker_range> {
    static std::string convert(stringmaker_range const&) {
        return "stringmaker(streamable_range)";
    }
};
}

struct just_range {
    int_iterator begin() const { return int_iterator{ 1 }; }
    int_iterator end() const { return {}; }
};

struct disabled_range {
    int_iterator begin() const { return int_iterator{ 1 }; }
    int_iterator end() const { return {}; }
};

namespace Catch {
template <>
struct is_range<disabled_range> {
    static const bool value = false;
};
}

TEST_CASE("stringify ranges", "[toString]") {
    REQUIRE(::Catch::Detail::stringify(streamable_range{}) == "op<<(streamable_range)");
    REQUIRE(::Catch::Detail::stringify(stringmaker_range{}) == "stringmaker(streamable_range)");
    REQUIRE(::Catch::Detail::stringify(just_range{}) == "{ 1, 2, 3, 4 }");
    REQUIRE(::Catch::Detail::stringify(disabled_range{}) == "{ !!! }");
}
