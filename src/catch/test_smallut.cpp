#include "catch.hpp"

#include <string>
#include <vector>

#include "smallut.h"

TEST_CASE("stringicmp", "[smallut]") {
    REQUIRE(stringicmp("hello", "HELLO") == 0);
    REQUIRE(stringicmp("abc", "abd") < 0);
    REQUIRE(stringicmp("abd", "abc") > 0);
    REQUIRE(stringicmp("", "") == 0);
    REQUIRE(stringicmp("a", "") > 0);
    REQUIRE(stringicmp("", "a") < 0);
}

TEST_CASE("stringtolower", "[smallut]") {
    REQUIRE(stringtolower("Hello World 123!") == "hello world 123!");
    REQUIRE(stringtolower("") == "");
    REQUIRE(stringtolower("ALLCAPS") == "allcaps");
}

TEST_CASE("stringtoupper", "[smallut]") {
    REQUIRE(stringtoupper("Hello World 123!") == "HELLO WORLD 123!");
}

TEST_CASE("trimstring", "[smallut]") {
    std::string s = "  hello  ";
    trimstring(s);
    REQUIRE(s == "hello");

    s = "hello";
    trimstring(s);
    REQUIRE(s == "hello");

    s = "   ";
    trimstring(s);
    REQUIRE(s == "");

    s = "";
    trimstring(s);
    REQUIRE(s == "");

    // trimstring only trims " \t" by default
    s = "\t hello \t";
    trimstring(s);
    REQUIRE(s == "hello");
}

TEST_CASE("rtrimstring", "[smallut]") {
    std::string s = "  hello  ";
    rtrimstring(s);
    REQUIRE(s == "  hello");
}

TEST_CASE("ltrimstring", "[smallut]") {
    std::string s = "  hello  ";
    ltrimstring(s);
    REQUIRE(s == "hello  ");
}

TEST_CASE("beginswith", "[smallut]") {
    REQUIRE(beginswith("hello world", "hello") == true);
    REQUIRE(beginswith("hello", "hello world") == false);
    REQUIRE(beginswith("hello", "") == true);
    REQUIRE(beginswith("", "hello") == false);
    REQUIRE(beginswith("", "") == true);
}

TEST_CASE("stringToTokens", "[smallut]") {
    std::vector<std::string> tokens;
    std::string s = "one,two,three";
    stringToTokens(s, tokens, ",");
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0] == "one");
    REQUIRE(tokens[1] == "two");
    REQUIRE(tokens[2] == "three");

    tokens.clear();
    s = "  one  two  three  ";
    stringToTokens(s, tokens);
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0] == "one");
    REQUIRE(tokens[1] == "two");
    REQUIRE(tokens[2] == "three");

    tokens.clear();
    s = "";
    stringToTokens(s, tokens, ",");
    REQUIRE(tokens.empty());
}

TEST_CASE("escapeHtml", "[smallut]") {
    std::string s = "<div class=\"test\">&foo</div>";
    std::string result = escapeHtml(s);
    REQUIRE(result == "&lt;div class=&quot;test&quot;&gt;&amp;foo&lt;/div&gt;");

    REQUIRE(escapeHtml("") == "");
    REQUIRE(escapeHtml("plain text") == "plain text");
}

TEST_CASE("neutchars", "[smallut]") {
    // neutchars returns a new string with chars replaced by ' '
    std::string result = neutchars("hello-world_test", "-_");
    REQUIRE(result == "hello world test");
}

TEST_CASE("displayableBytes", "[smallut]") {
    // displayableBytes includes unit suffix like " B ", " KB ", etc.
    REQUIRE(displayableBytes(0).find("B") != std::string::npos);
    REQUIRE(displayableBytes(500).find("B") != std::string::npos);
    std::string result = displayableBytes(1024LL * 1024 * 1024);
    REQUIRE(result.find("GB") != std::string::npos);
}

TEST_CASE("pc_decode", "[smallut]") {
    REQUIRE(pc_decode("hello%20world") == "hello world");
}

TEST_CASE("stringToBool", "[smallut]") {
    // stringToBool: true if starts with y/Y/t/T or non-zero digit
    REQUIRE(stringToBool("true") == true);
    REQUIRE(stringToBool("1") == true);
    REQUIRE(stringToBool("yes") == true);
    REQUIRE(stringToBool("True") == true);
    REQUIRE(stringToBool("Yes") == true);
    // "on"/"off" don't start with y/Y/t/T
    REQUIRE(stringToBool("on") == false);
    REQUIRE(stringToBool("false") == false);
    REQUIRE(stringToBool("0") == false);
    REQUIRE(stringToBool("no") == false);
    REQUIRE(stringToBool("") == false);
}
