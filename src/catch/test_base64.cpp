#include "catch.hpp"

#include <string>

#include "base64.h"

TEST_CASE("base64 encode/decode roundtrip", "[base64]") {
    std::string input = "Hello, World!";
    std::string encoded, decoded;

    base64_encode(input, encoded);
    REQUIRE(!encoded.empty());

    base64_decode(encoded, decoded);
    REQUIRE(decoded == input);
}

TEST_CASE("base64 empty string", "[base64]") {
    std::string encoded, decoded;

    base64_encode("", encoded);
    base64_decode(encoded, decoded);
    REQUIRE(decoded == "");
}

TEST_CASE("base64 known values", "[base64]") {
    std::string encoded;

    base64_encode("f", encoded);
    REQUIRE(encoded == "Zg==");

    base64_encode("fo", encoded);
    REQUIRE(encoded == "Zm8=");

    base64_encode("foo", encoded);
    REQUIRE(encoded == "Zm9v");

    base64_encode("foobar", encoded);
    REQUIRE(encoded == "Zm9vYmFy");
}

TEST_CASE("base64 binary data roundtrip", "[base64]") {
    std::string input;
    for (int i = 0; i < 256; i++) {
        input += static_cast<char>(i);
    }
    std::string encoded, decoded;
    base64_encode(input, encoded);
    base64_decode(encoded, decoded);
    REQUIRE(decoded == input);
}

TEST_CASE("base64 inline versions", "[base64]") {
    std::string input = "test string";
    std::string encoded = base64_encode(input);
    std::string decoded = base64_decode(encoded);
    REQUIRE(decoded == input);
}
