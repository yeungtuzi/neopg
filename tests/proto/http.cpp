/* Tests for stream functions
   Copyright 2017 The NeoPG developers

   NeoPG is released under the Simplified BSD License (see license.txt)
*/

#include <neopg/proto/http.h>
#include "gtest/gtest.h"

using namespace NeoPG;

namespace NeoPG {

TEST(NeoPGTest, proto_uri_test) {
  {
    Proto::URI uri;
    std::string input;
    ASSERT_EQ(uri.str(), "");

    input = "http://www.example.org";
    uri.set_uri(input);
    ASSERT_EQ(uri.scheme, "http");
    ASSERT_EQ(uri.authority, "www.example.org");
    ASSERT_EQ(uri.path, "");
    ASSERT_EQ(uri.query, "");
    ASSERT_EQ(uri.fragment, "");
    ASSERT_EQ(uri.str(), input);

    input = "https://www.example.org:10080/index.html";
    uri.set_uri(input);
    ASSERT_EQ(uri.scheme, "https");
    ASSERT_EQ(uri.authority, "www.example.org:10080");
    ASSERT_EQ(uri.path, "/index.html");
    ASSERT_EQ(uri.query, "");
    ASSERT_EQ(uri.fragment, "");
    ASSERT_EQ(uri.str(), input);

    input = "http://www.example.org/?name=foo#chapter1";
    uri.set_uri(input);
    ASSERT_EQ(uri.scheme, "http");
    ASSERT_EQ(uri.authority, "www.example.org");
    ASSERT_EQ(uri.path, "/");
    ASSERT_EQ(uri.query, "name=foo");
    ASSERT_EQ(uri.fragment, "chapter1");
    ASSERT_EQ(uri.str(), input);
  }
}

TEST(NeoPGTest, proto_http_test) {
  { /* FIXME */
  }
}
}
