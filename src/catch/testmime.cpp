#include "catch.hpp"

#include <string>

#include "mimeparse.h"

extern bool rfc2047_decode(const std::string& in, std::string &out);

TEST_CASE("rcf2047_10", "[mime]") {
    std::string out;

    // Regular rfc2047-encoded header
    REQUIRE(rfc2047_decode("sujet =?iso-8859-1?Q?=E0?= accents", out) == true);
    REQUIRE(out == "sujet à accents");

    // Bad but treatable UTF-8 in header, no rfc stuff. Will try decoding from utf-8, ok if succeeds
    REQUIRE(rfc2047_decode("sujet à accents", out) == true);
    REQUIRE(out == "sujet à accents");

    // Bad but treatable input: UTF-8 in header, also rfc stuff.
    REQUIRE(rfc2047_decode("sujet à =?iso-8859-1?Q?=E0?= accents", out) == true);
    REQUIRE(out == "sujet à à accents");

    // cp1252 in header. Same
    REQUIRE(rfc2047_decode("sujet \340 accents", out) == true);
    REQUIRE(out == "sujet à accents");

    // Incomplete rfc2047: just transcode.
    REQUIRE(rfc2047_decode("sujet =?iso-8859-1?Q?=E0? accents", out) == false);
    REQUIRE(out == "sujet =?iso-8859-1?Q?=E0? accents");
}

TEST_CASE("rfc2231_10", "[mime]") {
    MimeHeaderValue out;
    SECTION("0") {
        REQUIRE(parseMimeHeaderValue("", out) == true);
        REQUIRE(out.value == "");
        REQUIRE(out.params == std::map<std::string,std::string>{});
    }
    SECTION("01") {
        REQUIRE(parseMimeHeaderValue(" ", out) == true);
        REQUIRE(out.value == "");
        REQUIRE(out.params == std::map<std::string,std::string>{});
    }
    SECTION("02") {
        REQUIRE(parseMimeHeaderValue(" (comment) ", out) == true);
        REQUIRE(out.value == "");
        REQUIRE(out.params == std::map<std::string,std::string>{});
    }
    SECTION("03") {
        REQUIRE(parseMimeHeaderValue("somevalue (comment) ", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{});
    }
    SECTION("03") {
        REQUIRE(parseMimeHeaderValue("(comment)somevalue(comment) ", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{});
    }
    SECTION("10") {
        REQUIRE(parseMimeHeaderValue("somevalue;par1=val1;par2=val2", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1","val1"},{"par2","val2"}});
    }
    SECTION("11") {
        REQUIRE(parseMimeHeaderValue("somevalue", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{});
    }
    SECTION("12") {
        REQUIRE(parseMimeHeaderValue("somevalue", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{});
    }
    SECTION("20") {
        REQUIRE(parseMimeHeaderValue("somevalue ;par1= val1; par2=val2", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1","val1"},{"par2","val2"}});
    }
    SECTION("30") {
        REQUIRE(parseMimeHeaderValue("somevalue;par1=\"val1\";par2=val2", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1","val1"},{"par2","val2"}});
    }
    SECTION("40") {
        std::string in = R"(somevalue;par1*0=v;
 par1*1="al1";
 par2=val2)";
        REQUIRE(parseMimeHeaderValue(in, out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1","val1"},{"par2","val2"}});
    }
    SECTION("50") {
        std::string in = R"(somevalue;par1*0*=ascii'fr'%76;
 par1*1="al1";
 par2=val2)";
        REQUIRE(parseMimeHeaderValue(in, out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1","val1"},{"par2","val2"}});
    }
    SECTION("60") {
        REQUIRE(parseMimeHeaderValue(";par1=val1;par2=val2", out) == true);
        REQUIRE(out.value == "");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1","val1"},{"par2","val2"}});
    }
    SECTION("70") {
        REQUIRE(parseMimeHeaderValue("somevalue;par1;par2=val2", out) == true);
        REQUIRE(out.value == "somevalue");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1",""},{"par2","val2"}});
    }
    SECTION("80") {
        REQUIRE(parseMimeHeaderValue(";par1=val1;par2", out) == true);
        REQUIRE(out.value == "");
        REQUIRE(out.params == std::map<std::string,std::string>{{"par1","val1"},{"par2",""}});
    }
}
