/*
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/fetcher_test.h"

#include <utility>                      // for pair, make_pair
#include <vector>
#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class MessageHandler;

const char FetcherTest::kStartDate[] = "Sun, 16 Dec 1979 02:27:45 GMT";
const char FetcherTest::kHtmlContent[] = "<html><body>Nuts!</body></html>";
const char FetcherTest::kErrorMessage[] = "Invalid URL";
const char FetcherTest::kGoodUrl[] = "http://pi.com";
const char FetcherTest::kNotCachedUrl[] = "http://not_cacheable.com";
const char FetcherTest::kBadUrl[] = "http://this_url_will_fail.com";
const char FetcherTest::kHeaderName[] = "header-name";
const char FetcherTest::kHeaderValue[] = "header value";

SimpleStats* FetcherTest::statistics_ = NULL;

FetcherTest::FetcherTest()
    : mock_async_fetcher_(&mock_fetcher_),
      thread_system_(ThreadSystem::CreateThreadSystem()) {
}

void FetcherTest::ValidateMockFetcherResponse(
    bool success, bool check_error_message,
    const GoogleString& content,
    const ResponseHeaders& response_headers) {
  if (success) {
    EXPECT_EQ(GoogleString(kHtmlContent), content);
    ConstStringStarVector values;
    EXPECT_TRUE(response_headers.Lookup(kHeaderName, &values));
    EXPECT_EQ(1, values.size());
    EXPECT_EQ(GoogleString(kHeaderValue), *(values[0]));
  } else if (check_error_message) {
    EXPECT_EQ(GoogleString(kErrorMessage), content);
  }
}

int FetcherTest::CountFetchesSync(const StringPiece& url, bool expect_success,
                                  bool check_error_message) {
  CHECK(sync_fetcher() != NULL);
  return CountFetchesSync(url, sync_fetcher(),
                          expect_success, check_error_message);
}

int FetcherTest::CountFetchesSync(
    const StringPiece& url, UrlFetcher* fetcher,
    bool expect_success, bool check_error_message) {
  int starting_fetches = mock_fetcher_.num_fetches();
  GoogleString content;
  StringWriter content_writer(&content);
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  bool success = fetcher->StreamingFetchUrl(
      url.as_string(), request_headers, &response_headers, &content_writer,
      &message_handler_, RequestContextPtr(NULL));
  EXPECT_EQ(expect_success, success);
  ValidateMockFetcherResponse(success, check_error_message, content,
                              response_headers);
  return mock_fetcher_.num_fetches() - starting_fetches;
}

int FetcherTest::CountFetchesAsync(const StringPiece& url, bool expect_success,
                                   bool* callback_called) {
  CHECK(async_fetcher() != NULL);
  *callback_called = false;
  int starting_fetches = mock_fetcher_.num_fetches();
  CheckCallback* fetch = new CheckCallback(
      RequestContext::NewTestRequestContext(thread_system_.get()),
      expect_success, callback_called);
  async_fetcher()->Fetch(url.as_string(), &message_handler_, fetch);
  return mock_fetcher_.num_fetches() - starting_fetches;
}

void FetcherTest::ValidateOutput(const GoogleString& content,
                                 const ResponseHeaders& response_headers) {
  // The detailed header parsing code is tested in
  // simple_meta_data_test.cc.  But let's check the rseponse code
  // and the last header here, and make sure we got the content.
  EXPECT_EQ(200, response_headers.status_code());
  EXPECT_EQ(13, response_headers.NumAttributes());
  EXPECT_EQ(GoogleString("X-Google-GFE-Response-Body-Transformations"),
            GoogleString(response_headers.Name(12)));
  EXPECT_EQ(GoogleString("gunzipped"),
            GoogleString(response_headers.Value(12)));

  // Verifies that after the headers, we see the content.  Note that this
  // currently assumes 'wget' style output.  Wget takes care of any unzipping.
  static const char start_of_doc[] = "<!doctype html>";
  EXPECT_EQ(0, strncmp(start_of_doc, content.c_str(),
                       STATIC_STRLEN(start_of_doc)));
}


// MockFetcher
bool FetcherTest::MockFetcher::StreamingFetchUrl(
    const GoogleString& url,
    const RequestHeaders& request_headers,
    ResponseHeaders* response_headers,
    Writer* writer,
    MessageHandler* message_handler,
    const RequestContextPtr& unused_request_context) {
  bool ret = false;
  if (url == kGoodUrl) {
    ret = Populate("max-age=300", response_headers, writer,
                   message_handler);
  } else if (url == kNotCachedUrl) {
    ret = Populate("no-cache", response_headers, writer,
                   message_handler);
  } else {
    // Note: Non-zero status code must be set.
    response_headers->set_status_code(HttpStatus::kNotFound);
    writer->Write(kErrorMessage, message_handler);
  }
  ++num_fetches_;
  return ret;
}

bool FetcherTest::MockFetcher::Populate(const char* cache_control,
                                        ResponseHeaders* response_headers,
                                        Writer* writer,
                                        MessageHandler* message_handler) {
  response_headers->SetStatusAndReason(HttpStatus::kOK);
  response_headers->set_major_version(1);
  response_headers->set_minor_version(1);
  response_headers->Add(HttpAttributes::kCacheControl, cache_control);
  response_headers->Add(HttpAttributes::kDate, kStartDate);
  response_headers->Add(kHeaderName, kHeaderValue);
  response_headers->ComputeCaching();
  writer->Write(kHtmlContent, message_handler);
  return true;
}


// MockAsyncFetcher
void FetcherTest::MockAsyncFetcher::Fetch(const GoogleString& url,
                                          MessageHandler* handler,
                                          AsyncFetch* fetch) {
  bool status = url_fetcher_->StreamingFetchUrl(
      url, *fetch->request_headers(), fetch->response_headers(), fetch,
      handler, fetch->request_context());
  deferred_callbacks_.push_back(std::make_pair(status, fetch));
}

void FetcherTest::MockAsyncFetcher::CallCallbacks() {
  for (int i = 0, n = deferred_callbacks_.size(); i < n; ++i) {
    bool status = deferred_callbacks_[i].first;
    AsyncFetch* fetch = deferred_callbacks_[i].second;
    fetch->Done(status);
  }
  deferred_callbacks_.clear();
}

void FetcherTest::SetUpTestCase() {
  statistics_ = new SimpleStats;
  HTTPCache::InitStats(statistics_);
}

void FetcherTest::TearDownTestCase() {
  delete statistics_;
  statistics_ = NULL;
}

}  // namespace net_instaweb