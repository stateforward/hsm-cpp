#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "kind.hpp"

#include "doctest/doctest.h"

constexpr auto grandma = hsm::make_kind();
constexpr auto grandpa = hsm::make_kind(1);
constexpr auto mother = hsm::make_kind(2, grandma, grandpa);
constexpr auto aunt = hsm::make_kind(3, grandma, grandpa);
constexpr auto me = hsm::make_kind(4, mother);
constexpr auto cousin = hsm::make_kind(5, aunt);

// constexpr auto cousin = sf::make_type(6, aunt);

TEST_CASE("type")
{
    static_assert(hsm::is_kind(mother, grandma));
    static_assert(hsm::is_kind(mother, grandpa));
    static_assert(hsm::is_kind(aunt, grandma));
    static_assert(hsm::is_kind(aunt, grandpa));
    static_assert(!hsm::is_kind(aunt, mother));
    static_assert(hsm::is_kind(me, mother));
    static_assert(!hsm::is_kind(me, aunt));
    static_assert(hsm::is_kind(me, grandma));
    static_assert(hsm::is_kind(me, grandpa));
    static_assert(!hsm::is_kind(me, cousin));
    static_assert(hsm::is_kind(cousin, aunt));
    static_assert(hsm::is_kind(cousin, grandma));
    static_assert(hsm::is_kind(cousin, grandpa));
    static_assert(hsm::is_kind(mother, grandpa));
    static_assert(hsm::is_kind(mother, mother));
    static_assert(!hsm::is_kind(grandpa, grandma));
    static_assert(!hsm::is_kind(grandpa, mother));
    static_assert(!hsm::is_kind(grandma, mother));
    static_assert(hsm::is_kind(aunt, grandma));
    static_assert(hsm::is_kind(aunt, grandpa));
    static_assert(!hsm::is_kind(aunt, mother));
    static_assert(hsm::is_kind(cousin, aunt));
    static_assert(!hsm::is_kind(cousin, mother));
    static_assert(hsm::is_kind(cousin, grandma));
    static_assert(hsm::base(me) != aunt);
    static_assert(hsm::base(me) == mother);
}
