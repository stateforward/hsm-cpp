#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <string>
#include <string_view>

#include <vector>
#include "path.hpp"
TEST_CASE("path::split")
{
    SUBCASE("Empty path")
    {
        auto components = hsm::path::split("");
        CHECK(components.empty());
    }

    SUBCASE("Single component")
    {
        auto components = hsm::path::split("component");
        REQUIRE(components.size() == 1);
        CHECK(components[0] == "component");
    }

    SUBCASE("Multiple components")
    {
        auto components = hsm::path::split("/path/to/file");
        REQUIRE(components.size() == 3);
        CHECK(components[0] == "path");
        CHECK(components[1] == "to");
        CHECK(components[2] == "file");
    }

    SUBCASE("Path with trailing separator")
    {
        auto components = hsm::path::split("/path/to/");
        REQUIRE(components.size() == 2);
        CHECK(components[0] == "path");
        CHECK(components[1] == "to");
    }

    SUBCASE("Path with multiple consecutive separators")
    {
        auto components = hsm::path::split("/path//to///file");
        REQUIRE(components.size() == 3);
        CHECK(components[0] == "path");
        CHECK(components[1] == "to");
        CHECK(components[2] == "file");
    }

    SUBCASE("Different container types")
    {
        // Test with string
        std::string str_path = "/path/to/file";
        auto components_str = hsm::path::split(str_path);
        REQUIRE(components_str.size() == 3);

        // Test with string_view
        std::string_view sv_path = "/path/to/file";
        auto components_sv = hsm::path::split(sv_path);
        REQUIRE(components_sv.size() == 3);

        // Test with C-string
        const char *c_path = "/path/to/file";
        auto components_c = hsm::path::split(c_path);
        REQUIRE(components_c.size() == 3);

        // Verify all results are the same
        for (size_t i = 0; i < 3; ++i)
        {
            CHECK(components_str[i] == components_sv[i]);
            CHECK(components_str[i] == components_c[i]);
        }
    }
}

TEST_CASE("path::is_ancestor")
{
    SUBCASE("Empty paths")
    {
        CHECK_FALSE(hsm::path::is_ancestor("", ""));
        CHECK_FALSE(hsm::path::is_ancestor("", "/path"));
        CHECK_FALSE(hsm::path::is_ancestor("/path", ""));
    }

    SUBCASE("Direct ancestors")
    {
        CHECK(hsm::path::is_ancestor("/path", "/path/to"));
        CHECK(hsm::path::is_ancestor("/path/", "/path/to"));
    }

    SUBCASE("Not ancestors")
    {
        CHECK_FALSE(hsm::path::is_ancestor("/path/to", "/path"));
        CHECK_FALSE(hsm::path::is_ancestor("/path", "/pathto"));
        CHECK_FALSE(hsm::path::is_ancestor("/path/to1", "/path/to2"));
    }

    SUBCASE("String view input")
    {
        std::string_view path1 = "/path";
        std::string_view path2 = "/path/to";
        CHECK(hsm::path::is_ancestor(path1, path2));
    }
}

TEST_CASE("path::lca")
{
    SUBCASE("Empty paths")
    {
        CHECK(hsm::path::lca("", "") == "");
        CHECK(hsm::path::lca("", "/path") == "/path");
        CHECK(hsm::path::lca("/path", "") == "/path");
    }

    SUBCASE("Same paths")
    {
        CHECK(hsm::path::lca("/path", "/path") == "/path");
    }

    SUBCASE("One is ancestor of other")
    {
        CHECK(hsm::path::lca("/path", "/path/to") == "/path");
        CHECK(hsm::path::lca("/path/to", "/path") == "/path");
    }

    SUBCASE("Common ancestor")
    {
        CHECK(hsm::path::lca("/path/to/file1", "/path/to/file2") == "/path/to");
        CHECK(hsm::path::lca("/path/to1/file", "/path/to2/file") == "/path");
    }

    SUBCASE("String view input")
    {
        std::string_view path1 = "/path/to/file1";
        std::string_view path2 = "/path/to/file2";
        CHECK(hsm::path::lca(path1, path2) == "/path/to");
    }
}

TEST_CASE("path::normalize")
{
    SUBCASE("Empty path")
    {
        CHECK(hsm::path::normalize("") == ".");
    }

    SUBCASE("Simple paths")
    {
        CHECK(hsm::path::normalize("/path/to/file") == "/path/to/file");
        CHECK(hsm::path::normalize("path/to/file") == "path/to/file");
    }

    SUBCASE("Paths with redundant separators")
    {
        CHECK(hsm::path::normalize("/path//to///file") == "/path/to/file");
    }

    SUBCASE("Paths with . and ..")
    {
        CHECK(hsm::path::normalize("/path/./to/file") == "/path/to/file");
        CHECK(hsm::path::normalize("/path/to/../file") == "/path/file");
        CHECK(hsm::path::normalize("/path/to/../../file") == "/file");
    }

    SUBCASE("Paths with .. at root")
    {
        CHECK(hsm::path::normalize("/..") == "/");
        CHECK(hsm::path::normalize("/../path") == "/path");
    }

    SUBCASE("Relative paths with ..")
    {
        CHECK(hsm::path::normalize("path/..") == ".");
        CHECK(hsm::path::normalize("path/../") == ".");
        CHECK(hsm::path::normalize("path/../other") == "other");
    }

    SUBCASE("String view input")
    {
        std::string_view path = "/path//to/../file";
        CHECK(hsm::path::normalize(path) == "/path/file");
    }
}

TEST_CASE("path::join")
{
    SUBCASE("Empty paths")
    {
        CHECK(hsm::path::join() == "");
        CHECK(hsm::path::join("") == "");
        CHECK(hsm::path::join("", "") == ".");
    }

    SUBCASE("Simple paths")
    {
        CHECK(hsm::path::join("path") == "path");
        CHECK(hsm::path::join("path", "to") == "path/to");
        CHECK(hsm::path::join("path", "to", "file") == "path/to/file");
    }

    SUBCASE("Paths with separators")
    {
        CHECK(hsm::path::join("path/", "to") == "path/to");
        CHECK(hsm::path::join("path", "/to") == "path/to");
        CHECK(hsm::path::join("path/", "/to/") == "path/to");
    }

    SUBCASE("Absolute paths")
    {
        CHECK(hsm::path::join("/path", "to") == "/path/to");
    }

    SUBCASE("Iterator join")
    {
        std::vector<std::string> paths = {"path", "to", "file"};
        CHECK(hsm::path::join(paths.begin(), paths.end()) == "path/to/file");
    }

    SUBCASE("String view input")
    {
        std::string_view path1 = "path";
        std::string_view path2 = "to";
        CHECK(hsm::path::join(path1, path2) == "path/to");
    }

    SUBCASE("Mixed string and string_view types")
    {
        std::string str_path = "path";
        std::string_view sv_path = "to";
        const char *c_path = "file";

        // Test all combinations
        CHECK(hsm::path::join(str_path, sv_path) == "path/to");
        CHECK(hsm::path::join(str_path, c_path) == "path/file");
        CHECK(hsm::path::join(sv_path, str_path) == "to/path");
        CHECK(hsm::path::join(sv_path, c_path) == "to/file");
        CHECK(hsm::path::join(c_path, str_path) == "file/path");
        CHECK(hsm::path::join(c_path, sv_path) == "file/to");

        // Three arguments with mixed types
        CHECK(hsm::path::join(str_path, sv_path, c_path) == "path/to/file");
        CHECK(hsm::path::join(sv_path, c_path, str_path) == "to/file/path");
        CHECK(hsm::path::join(c_path, str_path, sv_path) == "file/path/to");
    }
}

TEST_CASE("path::basename")
{
    SUBCASE("Empty path")
    {
        CHECK(hsm::path::basename("").empty());
    }

    SUBCASE("Path without separators")
    {
        CHECK(hsm::path::basename("file").empty());
    }

    // SUBCASE("Root path")
    // {
    //     CHECK(hsm::path::basename("/") == "");
    // }

    SUBCASE("Simple paths")
    {
        CHECK(hsm::path::basename("/path") == "/");
        CHECK(hsm::path::basename("/path/to") == "/path");
        CHECK(hsm::path::basename("/path/to/file") == "/path/to");
        CHECK(hsm::path::basename("path/to/file") == "path/to");
    }

    SUBCASE("Paths with trailing separators")
    {
        CHECK(hsm::path::basename("/path/to/") == "/path/to");
    }

    SUBCASE("String view input")
    {
        std::string_view path = "/path/to/file";
        CHECK(hsm::path::basename(path) == "/path/to");
    }
}

TEST_CASE("path::name")
{
    SUBCASE("Empty path")
    {
        CHECK(hsm::path::name("").empty());
    }

    SUBCASE("Path without separators")
    {
        CHECK(hsm::path::name("file") == "file");
    }

    SUBCASE("Simple paths")
    {
        CHECK(hsm::path::name("/path") == "path");
        CHECK(hsm::path::name("/path/to") == "to");
        CHECK(hsm::path::name("/path/to/file") == "file");
        CHECK(hsm::path::name("path/to/file") == "file");
    }

    SUBCASE("Paths with trailing separators")
    {
        CHECK(hsm::path::name("/path/to/").empty());
    }

    SUBCASE("String view input")
    {
        std::string_view path = "/path/to/file";
        CHECK(hsm::path::name(path) == "file");
    }
}

TEST_CASE("path::is_absolute")
{
    SUBCASE("Empty path")
    {
        CHECK_FALSE(hsm::path::is_absolute(""));
    }

    SUBCASE("Absolute paths")
    {
        CHECK(hsm::path::is_absolute("/"));
        CHECK(hsm::path::is_absolute("/path"));
        CHECK(hsm::path::is_absolute("/path/to/file"));
    }

    SUBCASE("Relative paths")
    {
        CHECK_FALSE(hsm::path::is_absolute("path"));
        CHECK_FALSE(hsm::path::is_absolute("path/to/file"));
        CHECK_FALSE(hsm::path::is_absolute("./path"));
        CHECK_FALSE(hsm::path::is_absolute("../path"));
    }

    SUBCASE("String view input")
    {
        std::string_view abs_path = "/path";
        std::string_view rel_path = "path";
        CHECK(hsm::path::is_absolute(abs_path));
        CHECK_FALSE(hsm::path::is_absolute(rel_path));
    }
}

TEST_CASE("string_view and string interoperability")
{
    SUBCASE("basename with different types")
    {
        std::string path_str = "/path/to/file";
        std::string_view path_sv = path_str;
        const char *path_cstr = "/path/to/file";

        CHECK(hsm::path::basename(path_str) == hsm::path::basename(path_sv));
        CHECK(hsm::path::basename(path_str) == hsm::path::basename(path_cstr));
    }

    SUBCASE("name with different types")
    {
        std::string path_str = "/path/to/file";
        std::string_view path_sv = path_str;
        const char *path_cstr = "/path/to/file";

        CHECK(hsm::path::name(path_str) == hsm::path::name(path_sv));
        CHECK(hsm::path::name(path_str) == hsm::path::name(path_cstr));
    }

    SUBCASE("is_absolute with different types")
    {
        std::string abs_str = "/path/to/file";
        std::string_view abs_sv = abs_str;
        const char *abs_cstr = "/path/to/file";

        CHECK(hsm::path::is_absolute(abs_str) == hsm::path::is_absolute(abs_sv));
        CHECK(hsm::path::is_absolute(abs_str) == hsm::path::is_absolute(abs_cstr));
    }

    SUBCASE("normalize with different types")
    {
        std::string path_str = "/path/to/../file";
        std::string_view path_sv = path_str;
        const char *path_cstr = "/path/to/../file";

        CHECK(hsm::path::normalize(path_str) == hsm::path::normalize(path_sv));
        CHECK(hsm::path::normalize(path_str) == hsm::path::normalize(path_cstr));
    }

    SUBCASE("string_view lifetime")
    {
        // This test demonstrates the importance of ensuring proper string_view lifetime
        std::string original = "/path/to/file";
        auto basename = hsm::path::basename(original);
        auto name = hsm::path::name(original);

        // Modifying original would invalidate the views if they weren't properly handled
        std::string copy_basename = std::string(basename);
        std::string copy_name = std::string(name);

        original = "something_completely_different";

        // Verify our copies are intact
        CHECK(copy_basename == "/path/to");
        CHECK(copy_name == "file");
    }
}

TEST_CASE("path::match")
{
    SUBCASE("Empty patterns and paths")
    {
        CHECK(hsm::path::match("", ""));
        CHECK_FALSE(hsm::path::match("", "path"));
        CHECK_FALSE(hsm::path::match("pattern", ""));
    }

    SUBCASE("Exact matches without wildcards")
    {
        CHECK(hsm::path::match("/path/to/file", "/path/to/file"));
        CHECK(hsm::path::match("path/to/file", "path/to/file"));
        CHECK_FALSE(hsm::path::match("/path/to/file", "/path/to/other"));
        CHECK_FALSE(hsm::path::match("/path/to/file", "/path/to/file/extra"));
    }

    SUBCASE("Wildcards at the end")
    {
        CHECK(hsm::path::match("/path/to/*", "/path/to/file"));
        CHECK(hsm::path::match("/path/to/*", "/path/to/other"));
        CHECK(hsm::path::match("/path/to/*", "/path/to/directory/file"));
        CHECK_FALSE(hsm::path::match("/path/to/*", "/path/tofile"));
    }

    SUBCASE("Wildcards at the beginning")
    {
        CHECK(hsm::path::match("*/file", "/path/to/file"));
        CHECK(hsm::path::match("*/file", "path/to/file"));
        CHECK_FALSE(hsm::path::match("*/file", "/path/to/other"));
    }

    SUBCASE("Wildcards in the middle")
    {
        CHECK(hsm::path::match("/path/*/file", "/path/to/file"));
        CHECK(hsm::path::match("/path/*/file", "/path/directory/file"));
        CHECK_FALSE(hsm::path::match("/path/*/file", "/different/to/file"));
    }

    SUBCASE("Multiple wildcards")
    {
        CHECK(hsm::path::match("/*/*/*", "/path/to/file"));
        CHECK(hsm::path::match("/path/*/*", "/path/to/file"));
        CHECK(hsm::path::match("/*/*/file", "/path/to/file"));
        CHECK(hsm::path::match("/*/*", "/path/to/file"));
    }

    SUBCASE("Wildcards matching empty segments")
    {
        CHECK(hsm::path::match("/path/*/file", "/path//file"));
        CHECK(hsm::path::match("/path/*/", "/path//"));
    }

    SUBCASE("Complex patterns")
    {
        CHECK(hsm::path::match("/path/*/component/*", "/path/to/component/file"));
        CHECK(hsm::path::match("*/*file", "/path/to/myfile"));
        CHECK(hsm::path::match("*.hpp", "module.hpp"));
        CHECK_FALSE(hsm::path::match("*.hpp", "module.cpp"));
    }

    SUBCASE("Question mark wildcard")
    {
        // '?' matches exactly one character
        CHECK(hsm::path::match("?", "a"));
        CHECK(hsm::path::match("a?c", "abc"));
        CHECK(hsm::path::match("a?c?e", "abcde"));
        CHECK_FALSE(hsm::path::match("a?c", "ac"));
        CHECK_FALSE(hsm::path::match("a?c", "abbc"));

        // Test with path separators
        CHECK(hsm::path::match("/path/?o/file", "/path/to/file"));
        CHECK_FALSE(hsm::path::match("/path/?o/file", "/path/too/file"));

        // Combining '?' and '*' wildcards
        CHECK(hsm::path::match("/*/?ile", "/path/file"));
        CHECK(hsm::path::match("*.?pp", "file.cpp"));
        CHECK(hsm::path::match("*.?pp", "file.hpp"));
        CHECK_FALSE(hsm::path::match("*.?pp", "file.txt"));
    }

    SUBCASE("String view input")
    {
        std::string_view pattern = "/path/*/file";
        std::string_view path = "/path/to/file";
        CHECK(hsm::path::match(pattern, path));
    }

    SUBCASE("Performance test - avoid recursion or excessive allocations")
    {
        // Create a deep path to match
        std::string deep_path;
        deep_path.reserve(1000);
        for (int i = 0; i < 100; i++)
        {
            deep_path += "/segment";
            deep_path += std::to_string(i);
        }

        // Pattern with wildcard at the end
        std::string pattern = "/segment0/segment1/*";

        // This should complete quickly if the implementation is efficient
        CHECK(hsm::path::match(pattern, deep_path));
    }

    SUBCASE("Multiple pattern matching with match_any")
    {
        // Basic usage
        CHECK(hsm::path::match_any("/path/to/file", "*.cpp", "*.hpp", "/path/to/file"));
        CHECK_FALSE(hsm::path::match_any("/path/to/file", "*.cpp", "*.hpp", "*.txt"));

        // Mix of patterns
        CHECK(hsm::path::match_any("/path/to/file.cpp", "/path/*/file.*", "*.cpp", "/other/*"));
        CHECK(hsm::path::match_any("file.txt", "*.cpp", "file.???", "*.txt"));

        // String literals and string_views
        std::string_view path = "/path/to/file";
        std::string pattern1 = "*.cpp";
        std::string_view pattern2 = "/path/*/file";
        CHECK(hsm::path::match_any(path, pattern1, pattern2, "*.hpp"));

        // Edge cases
        CHECK(hsm::path::match_any("/path/to/file", "/path/to/file"));
        // Empty vector should not match (instead of no patterns)
        std::vector<std::string> no_patterns;
        CHECK_FALSE(hsm::path::match_any("/path/to/file", no_patterns));

        // Complex combinations
        CHECK(hsm::path::match_any("/path/to/file",
                                   "/not/matching",
                                   "/*/?o/fi?e",
                                   "completely/different"));
    }

    SUBCASE("Vector-based pattern matching with match_any")
    {
        // Test with std::vector of string literals
        std::vector<const char *> vec_patterns1 = {"*.cpp", "*.hpp", "/path/to/file"};
        CHECK(hsm::path::match_any("/path/to/file", vec_patterns1));

        // Test with std::vector of strings
        std::vector<std::string> vec_patterns2 = {"*.cpp", "*.hpp", "*.txt"};
        CHECK_FALSE(hsm::path::match_any("/path/to/file", vec_patterns2));

        // Test with mixed string types
        std::vector<std::string> vec_patterns3 = {"/path/*/file.*", "*.cpp", "/other/*"};
        CHECK(hsm::path::match_any("/path/to/file.cpp", vec_patterns3));

        // Test with empty vector (should not match)
        std::vector<std::string> empty_patterns;
        CHECK_FALSE(hsm::path::match_any("/path/to/file", empty_patterns));

        // Test with vector of string_views
        std::string pattern_str1 = "*.cpp";
        std::string pattern_str2 = "*.hpp";
        std::string pattern_str3 = "/path/*/file";
        std::vector<std::string_view> vec_patterns4 = {
            pattern_str1, pattern_str2, pattern_str3};
        CHECK(hsm::path::match_any("/path/to/file", vec_patterns4));

        // Test with deferred events from State structure
        std::vector<std::string> deferred = {"event1", "event2", "*"};
        CHECK(hsm::path::match_any("event1", deferred));
        CHECK(hsm::path::match_any("event3", deferred)); // Matches wildcard "*"
    }
}
