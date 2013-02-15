/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/rewrite_query.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char kHtmlUrl[] = "http://www.test.com/index.jsp";

class RewriteQueryTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    allow_related_options_ = false;
    image_url_ = Encode(kTestDomain, "ic", "0", "image.jpg", "jpg");
  }

  RewriteOptions* ParseAndScan(const StringPiece request_url,
                               const StringPiece& in_query,
                               const StringPiece& in_req_string) {
    return ParseAndScan(request_url, in_query, in_req_string, NULL, NULL);
  }

  // Parses query-params &/or HTTP headers.  The HTTP headers are specified
  // as a string, with semi-colons separating attr:value pairs.
  RewriteOptions* ParseAndScan(const StringPiece request_url,
                               const StringPiece& in_query,
                               const StringPiece& in_req_string,
                               GoogleString* out_query,
                               GoogleString* out_req_string) {
    GoogleString out_resp_string;
    RequestHeaders request_headers;
    StringPieceVector components;
    SplitStringPieceToVector(in_req_string, ";", &components, true);
    for (int i = 0, n = components.size(); i < n; ++i) {
      StringPieceVector attr_value;
      SplitStringPieceToVector(components[i], ":", &attr_value, false);
      CHECK_EQ(2, attr_value.size());
      request_headers.Add(attr_value[0], attr_value[1]);
    }
    return ParseAndScan(request_url, in_query, &request_headers,
                        NULL, out_query,
                        out_req_string, &out_resp_string);
  }

  RewriteOptions* ParseAndScan(const StringPiece request_url,
                               const StringPiece& in_query,
                               RequestHeaders* request_headers,
                               ResponseHeaders* response_headers,
                               GoogleString* out_query,
                               GoogleString* out_req_string,
                               GoogleString* out_resp_string) {
    options_.reset(new RewriteOptions);
    GoogleUrl url(StrCat(request_url, "?", in_query));
    if (RewriteQuery::Scan(allow_related_options_, factory(),
                           server_context(), &url, request_headers,
                           response_headers, &options_, &handler_)
        != RewriteQuery::kSuccess) {
      options_.reset(NULL);
    }
    if (out_query != NULL) {
      out_query->assign(url.Query().data(), url.Query().size());
    }
    if (out_req_string != NULL && request_headers != NULL) {
      out_req_string->assign(request_headers->ToString());
    }
    if (out_resp_string != NULL && response_headers != NULL) {
      out_resp_string->assign(response_headers->ToString());
    }

    return options_.get();
  }

  // Starts with image_, applies the specified image-options, and any
  // query-params and request-headers.
  RewriteOptions* ParseAndScanImageOptions(StringPiece image_options,
                                           StringPiece query_params,
                                           StringPiece request_headers) {
    allow_related_options_ = true;
    GoogleString query;
    GoogleString req_string;
    GoogleString image = AddOptionsToEncodedUrl(image_url_, image_options);
    RewriteOptions* options = ParseAndScan(image, query_params, request_headers,
                                           &query, &req_string);
    EXPECT_STREQ("", query);
    return options;
  }

  void CheckExtendCache(RewriteOptions* options, bool x) {
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheCss));
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheImages));
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheScripts));
  }

  // In a fashion patterned after the usage in mod_instaweb.cc, establish
  // a base configuration, and update it based on the passed-in query string.
  void Incremental(const StringPiece& query, RewriteOptions* options) {
    scoped_ptr<RewriteOptions> query_options;
    GoogleUrl gurl(StrCat("http://example.com/?ModPagespeedFilters=", query));
    EXPECT_EQ(RewriteQuery::kSuccess,
              RewriteQuery::Scan(allow_related_options_, factory(),
                                 server_context(), &gurl,
                                 NULL, NULL, &query_options,
                                 message_handler()));
    options->Merge(*query_options.get());
  }

  GoogleMessageHandler handler_;
  scoped_ptr<RewriteOptions> options_;
  bool allow_related_options_;
  GoogleString image_url_;
};

TEST_F(RewriteQueryTest, Empty) {
  EXPECT_TRUE(ParseAndScan(kHtmlUrl, "", "") == NULL);
}

TEST_F(RewriteQueryTest, OffQuery) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "ModPagespeed=off", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->enabled());
}

TEST_F(RewriteQueryTest, OffHeaders) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "", "ModPagespeed:off");
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->enabled());
}

TEST_F(RewriteQueryTest, OffResponseHeader) {
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  GoogleString in_query, out_query, out_req_string, out_resp_string;

  response_headers.Add("ModPagespeed", "off");
  RewriteOptions* options = ParseAndScan(kHtmlUrl, in_query, &request_headers,
                                         &response_headers, &out_query,
                                         &out_req_string, &out_resp_string);
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->enabled());
}

TEST_F(RewriteQueryTest, OnWithDefaultFiltersQuery) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "ModPagespeed=on", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options, true);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCachePdfs));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, OnWithDefaultFiltersHeaders) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "", "ModPagespeed:on");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options, true);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCachePdfs));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFiltersQuery) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl,
                                         "ModPagespeedFilters=remove_quotes",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRemoveQuotes));
  CheckExtendCache(options, false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCachePdfs));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFiltersQueryCorePlusMinus) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "ModPagespeedFilters="
                                         "core,+div_structure,-inline_css,"
                                         "+extend_cache_css",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());

  CheckExtendCache(options, true);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCacheImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kDivStructure));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kInlineCss));
  // Unlike above, these are true because 'core' is in the filter list.
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFiltersRequestHeaders) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "",
                                         "ModPagespeedFilters:remove_quotes");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRemoveQuotes));
  CheckExtendCache(options, false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCachePdfs));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFiltersResponseHeaders) {
  // Check that response headers are properly parsed.
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  GoogleString in_query, out_query, out_req_string, out_resp_string;

  response_headers.Add("ModPagespeedFilters", "remove_quotes");
  RewriteOptions* options = ParseAndScan(kHtmlUrl, in_query, &request_headers,
                                         &response_headers, &out_query,
                                         &out_req_string, &out_resp_string);
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRemoveQuotes));
  CheckExtendCache(options, false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCachePdfs));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, QueryAndRequestAndResponse) {
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  GoogleString in_query, out_query, out_req_string, out_resp_string;

  in_query = ("ModPagespeedFilters=-div_structure,+extend_cache_css");

  request_headers.Add("ModPagespeedCssInlineMaxBytes", "10");
  request_headers.Add("ModPagespeedJsInlineMaxBytes", "7");
  request_headers.Add("ModPagespeedFilters",
                      "+div_structure,-inline_css,+remove_quotes");

  response_headers.Add("ModPagespeedFilters",
                       "+inline_css,-remove_quotes");
  response_headers.Add("ModPagespeedJsInlineMaxBytes", "13");
  response_headers.Add("ModPagespeedFilters", "");
  RewriteOptions* options = ParseAndScan(kHtmlUrl, in_query, &request_headers,
                                         &response_headers, &out_query,
                                         &out_req_string, &out_resp_string);

  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());

  EXPECT_EQ(10, options->css_inline_max_bytes());

  // Request and Response conflict, Response should win.
  EXPECT_EQ(13, options->js_inline_max_bytes());

  // Request/Response/Query conflicts, disabled should win over enabled
  EXPECT_FALSE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRemoveQuotes));

  EXPECT_FALSE(options->Enabled(RewriteOptions::kDivStructure));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteQueryTest, MultipleQuery) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl,
                                         "ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleHeaders) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "",
                                         "ModPagespeedFilters:inline_css;"
                                         "ModPagespeedCssInlineMaxBytes:10");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleQueryAndHeaders) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl,
                                         "ModPagespeedFilters=inline_css",
                                         "ModPagespeedCssInlineMaxBytes:10");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleIgnoreUnrelated) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl,
                                         "ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10"
                                         "&Unrelated1"
                                         "&Unrelated2="
                                         "&Unrelated3=value",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleBroken) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl,
                                         "ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10"
                                         "&ModPagespeedFilters=bogus_filter",
                                         "");
  EXPECT_TRUE(options == NULL);
}

TEST_F(RewriteQueryTest, MultipleInt64Params) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl,
                                         "ModPagespeedCssInlineMaxBytes=3"
                                         "&ModPagespeedImageInlineMaxBytes=5"
                                         "&ModPagespeedCssImageInlineMaxBytes=7"
                                         "&ModPagespeedJsInlineMaxBytes=11"
                                         "&ModPagespeedDomainShardCount=2",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_EQ(3, options->css_inline_max_bytes());
  EXPECT_EQ(5, options->ImageInlineMaxBytes());
  EXPECT_EQ(7, options->CssImageInlineMaxBytes());
  EXPECT_EQ(11, options->js_inline_max_bytes());
  EXPECT_EQ(2, options->domain_shard_count());
}

TEST_F(RewriteQueryTest, OutputQueryandHeaders) {
  GoogleString output_query, output_headers;
  ParseAndScan(kHtmlUrl, "ModPagespeedCssInlineMaxBytes=3"
               "&ModPagespeedImageInlineMaxBytes=5"
               "&ModPagespeedCssImageInlineMaxBytes=7"
               "&ModPagespeedJsInlineMaxBytes=11"
               "&ModPagespeedDomainShardCount=100"
               "&ModPagespeedCssFlattenMaxBytes=13"
               "&abc=1"
               "&def",
               "ModPagespeedFilters:inline_css;"
               "xyz:6;"
               "ModPagespeedFilters:remove_quotes",
               &output_query, &output_headers);
  EXPECT_EQ(output_query, "abc=1&def");
  EXPECT_EQ(output_headers, "GET  HTTP/1.0\r\nxyz: 6\r\n\r\n");
  ParseAndScan(kHtmlUrl, "ModPagespeedCssInlineMaxBytes=3", "",
               &output_query, &output_headers);
  EXPECT_EQ(output_query, "");
}

TEST_F(RewriteQueryTest, OutputQueryandHeadersPostRequest) {
  GoogleString output_query, output_req_headers, output_resp_headers;
  RequestHeaders request_headers;
  request_headers.set_method(RequestHeaders::kPost);
  request_headers.Add("ModPagespeedFilters", "inline_css");
  request_headers.Add("xyz", "6");
  request_headers.set_message_body("pqr");

  ParseAndScan(kHtmlUrl, "ModPagespeedCssInlineMaxBytes=3"
               "&abc=1"
               "&def",
               &request_headers,
               NULL,
               &output_query, &output_req_headers, &output_resp_headers);
  EXPECT_EQ(output_query, "abc=1&def");
  EXPECT_EQ(output_req_headers, "POST  HTTP/1.0\r\nxyz: 6\r\n\r\n");
  EXPECT_EQ(request_headers.message_body(), "pqr");
}

// Tests the ability to add an additional filter on the command-line based
// on whatever set is already installed in the configuration.
TEST_F(RewriteQueryTest, IncrementalAdd) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("+debug", &options);
  EXPECT_TRUE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kDebug));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kAddBaseTag));
  EXPECT_TRUE(options.modified());
}

// Same exact test as above, except that we omit the "+".  This wipes out
// the explicitly enabled filter in the configuration and also the core
// level.
TEST_F(RewriteQueryTest, NonIncrementalAdd) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("debug", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kDebug));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

// In this version we specify nothing, and that should erase the filters.
TEST_F(RewriteQueryTest, IncrementalEmpty) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

TEST_F(RewriteQueryTest, IncrementalRemoveExplicit) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("-strip_scripts", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

TEST_F(RewriteQueryTest, IncrementalRemoveFromCore) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("-combine_css", &options);
  EXPECT_TRUE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

TEST_F(RewriteQueryTest, NoChangesShouldNotModify) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  Incremental("+combine_css", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kCombineCss));
  //
  // TODO(jmarantz): We would like at this point to have options show up
  // as unmodified.  However our implementation of query-params parsing
  // does not allow for this at this point, because it doesn't know
  // that it is working with the core filters.  Right now this is not
  // that important as the only usage of RewriteOptions::modified() is
  // in apache/mod_instaweb.cc which is just checking to see if there are
  // any directory-specific options set.
  //
  // EXPECT_FALSE(options.modified());
}

TEST_F(RewriteQueryTest, NoscriptQueryParamEmptyValue) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "ModPagespeed=noscript", "");
  RewriteOptions::FilterSet filter_set;
  options->GetEnabledFiltersRequiringScriptExecution(&filter_set);
  EXPECT_TRUE(filter_set.empty());
  EXPECT_FALSE(options->Enabled(RewriteOptions::kPrioritizeVisibleContent));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kHandleNoscriptRedirect));
}

TEST_F(RewriteQueryTest, NoscriptHeader) {
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "", "ModPagespeed:noscript");
  RewriteOptions::FilterSet filter_set;
  options->GetEnabledFiltersRequiringScriptExecution(&filter_set);
  EXPECT_TRUE(filter_set.empty());
  EXPECT_FALSE(options->Enabled(RewriteOptions::kPrioritizeVisibleContent));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kHandleNoscriptRedirect));
}

TEST_F(RewriteQueryTest, PreserveUrlsForPagespeedResources) {
  const char kQuery[] = "ModPagespeedJpegRecompressionQuality=85";
  GoogleString query, req;
  RewriteOptions* options = ParseAndScan(image_url_, kQuery, "", &query, &req);
  EXPECT_TRUE(options != NULL);
  EXPECT_STREQ("", query);
}

TEST_F(RewriteQueryTest, GenerateEmptyResourceOption) {
  EXPECT_EQ("", RewriteQuery::GenerateResourceOption("ic", rewrite_driver()));
}

TEST_F(RewriteQueryTest, GenerateResourceOptionRecompressImages) {
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);  // relevant
  options()->EnableFilter(RewriteOptions::kCombineCss);  // not relevant
  options()->set_image_jpeg_recompress_quality(70);
  EXPECT_EQ("rj+iq=70",
            RewriteQuery::GenerateResourceOption("ic", rewrite_driver()));
  EXPECT_EQ("",
            RewriteQuery::GenerateResourceOption("jm", rewrite_driver()));

  // TODO(jmarantz): add support for CSS/JS options & test.
  // TODO(jmarantz): test all relevant filter/option combinations.
}

TEST_F(RewriteQueryTest, DontAllowArbitraryOptionsForNonPagespeedResources) {
  allow_related_options_ = true;
  // The kHtmlUrl is a .jsp, which is not .pagespeed.
  RewriteOptions* options = ParseAndScan(kHtmlUrl, "PsolOpt=rj,iq:70", "");
  EXPECT_TRUE(options == NULL);
}

TEST_F(RewriteQueryTest, DontAllowArbitraryOptionsWhenDisabled) {
  GoogleString image = AddOptionsToEncodedUrl(image_url_, "rj+iq=70");
  RewriteOptions* options = ParseAndScan(image, "", "");
  EXPECT_TRUE(options == NULL);
}

TEST_F(RewriteQueryTest, CanQueryRecompressImages) {
  RewriteOptions* options = ParseAndScanImageOptions("rj+iq=70", "", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRecompressJpeg));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_EQ(70, options->image_jpeg_recompress_quality());
}

TEST_F(RewriteQueryTest, CanOverrideRecompressImagesWithQuery) {
  RewriteOptions* options = ParseAndScanImageOptions(
      "rj+iq=70", "ModPagespeedJpegRecompressionQuality=71", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRecompressJpeg));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_EQ(71, options->image_jpeg_recompress_quality());
}

TEST_F(RewriteQueryTest, CanOverrideRecompressImagesWithReqHeaders) {
  RewriteOptions* options = ParseAndScanImageOptions(
      "rj+iq=70", "", "ModPagespeedJpegRecompressionQuality:72");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRecompressJpeg));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_EQ(72, options->image_jpeg_recompress_quality());
}

TEST_F(RewriteQueryTest, CanOverrideRecompressImagesWithBoth) {
  RewriteOptions* options = ParseAndScanImageOptions(
      "rj+iq=70",
      "ModPagespeedJpegRecompressionQuality=71",
      "ModPagespeedJpegRecompressionQuality:72");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRecompressJpeg));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_EQ(72, options->image_jpeg_recompress_quality()) << "req-headers win.";
}

TEST_F(RewriteQueryTest, OnlyAllowWhitelistedResources) {
  allow_related_options_ = true;

  // whitelisted by "ic"
  GoogleString image = AddOptionsToEncodedUrl(image_url_, "rj");
  EXPECT_TRUE(ParseAndScan(image, "", "") != NULL);
  image = AddOptionsToEncodedUrl(image_url_, "iq=70");
  EXPECT_TRUE(ParseAndScan(image, "", "") != NULL);

  // not whitelisted by "ic"
  image = AddOptionsToEncodedUrl(image_url_, "cc");
  EXPECT_TRUE(ParseAndScan(image_url_, "", "") == NULL);
  image = AddOptionsToEncodedUrl(image_url_, "rdm=10");
  EXPECT_TRUE(ParseAndScan(image_url_, "", "") == NULL);
}

}  // namespace net_instaweb
