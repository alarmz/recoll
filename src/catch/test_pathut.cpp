#include "catch.hpp"

#include <string>

#include "pathut.h"

TEST_CASE("path_getsimple", "[pathut]") {
    REQUIRE(path_getsimple("/usr/bin/test") == "test");
    REQUIRE(path_getsimple("/usr/bin/") == "");
    REQUIRE(path_getsimple("test") == "test");
    REQUIRE(path_getsimple("") == "");
}

TEST_CASE("path_basename", "[pathut]") {
    // path_basename(path, suffix) strips the suffix if present
    REQUIRE(path_basename("test.txt", ".txt") == "test");
    REQUIRE(path_basename("test.txt", ".cpp") == "test.txt");
    REQUIRE(path_basename("test", "") == "test");
    REQUIRE(path_basename("archive.tar.gz", ".gz") == "archive.tar");
}

TEST_CASE("path_suffix", "[pathut]") {
    REQUIRE(path_suffix("test.txt") == "txt");
    REQUIRE(path_suffix("test") == "");
    REQUIRE(path_suffix("path/to/file.cpp") == "cpp");
    REQUIRE(path_suffix(".hidden") == "hidden");
}

TEST_CASE("path_cat", "[pathut]") {
    REQUIRE(path_cat("/usr", "bin") == "/usr/bin");
    REQUIRE(path_cat("/usr/", "bin") == "/usr/bin");
    REQUIRE(path_cat("", "bin") == "./bin");
    REQUIRE(path_cat("/usr", "") == "/usr");
}

TEST_CASE("path_getfather", "[pathut]") {
    REQUIRE(path_getfather("/usr/bin/test") == "/usr/bin/");
    REQUIRE(path_getfather("/usr/bin/") == "/usr/");
    REQUIRE(path_getfather("/") == "/");
}

TEST_CASE("path_isabsolute", "[pathut]") {
#ifdef _WIN32
    // path_isdriveabs requires forward slash: C:/
    REQUIRE(path_isabsolute("C:/Users/test") == true);
    // backslash form is NOT recognized by path_isdriveabs
    REQUIRE(path_isabsolute("C:\\Users\\test") == false);
    REQUIRE(path_isabsolute("relative/path") == false);
#else
    REQUIRE(path_isabsolute("/usr/bin") == true);
    REQUIRE(path_isabsolute("relative/path") == false);
#endif
    REQUIRE(path_isabsolute("") == false);
}

TEST_CASE("path_home", "[pathut]") {
    std::string home = path_home();
    REQUIRE(!home.empty());
}

TEST_CASE("path_tildexpand", "[pathut]") {
    std::string expanded = path_tildexpand("~");
    REQUIRE(expanded == path_home());
}
