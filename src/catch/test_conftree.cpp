#include "catch.hpp"

#include <string>
#include <vector>

#include "conftree.h"

TEST_CASE("ConfSimple basic set/get", "[conftree]") {
    ConfSimple conf;
    conf.set("key1", "value1");

    std::string val;
    REQUIRE(conf.get("key1", val) == 1);
    REQUIRE(val == "value1");
}

TEST_CASE("ConfSimple missing key", "[conftree]") {
    ConfSimple conf;
    std::string val;
    REQUIRE(conf.get("nonexistent", val) == 0);
}

TEST_CASE("ConfSimple overwrite", "[conftree]") {
    ConfSimple conf;
    conf.set("key1", "value1");
    conf.set("key1", "value2");

    std::string val;
    REQUIRE(conf.get("key1", val) == 1);
    REQUIRE(val == "value2");
}

TEST_CASE("ConfSimple erase", "[conftree]") {
    ConfSimple conf;
    conf.set("key1", "value1");

    conf.erase("key1", "");

    std::string val;
    REQUIRE(conf.get("key1", val) == 0);
}

TEST_CASE("ConfSimple subsections", "[conftree]") {
    ConfSimple conf;
    conf.set("key1", "global_value");
    conf.set("key1", "section_value", "section1");

    std::string val;
    REQUIRE(conf.get("key1", val) == 1);
    REQUIRE(val == "global_value");

    REQUIRE(conf.get("key1", val, "section1") == 1);
    REQUIRE(val == "section_value");
}

TEST_CASE("ConfSimple getNames", "[conftree]") {
    ConfSimple conf;
    conf.set("key1", "val1");
    conf.set("key2", "val2");
    conf.set("key3", "val3");

    std::vector<std::string> names = conf.getNames("");
    REQUIRE(names.size() == 3);
}

TEST_CASE("ConfSimple from string", "[conftree]") {
    std::string data = "key1 = value1\nkey2 = value2\n[section1]\nkey3 = value3\n";
    ConfSimple conf(data, 1); // 1 = from string, not file

    std::string val;
    REQUIRE(conf.get("key1", val) == 1);
    REQUIRE(val == "value1");

    REQUIRE(conf.get("key2", val) == 1);
    REQUIRE(val == "value2");

    REQUIRE(conf.get("key3", val, "section1") == 1);
    REQUIRE(val == "value3");
}

TEST_CASE("ConfSimple integer values", "[conftree]") {
    ConfSimple conf;
    conf.set("intval", "42");

    REQUIRE(conf.getInt("intval", 0) == 42);
    REQUIRE(conf.getBool("intval", false) == true); // "42" -> non-zero digit -> true
}
