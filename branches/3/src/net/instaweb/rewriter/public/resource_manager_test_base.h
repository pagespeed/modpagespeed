/**
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

// Author: abliss@google.com (Adam Bliss)

// Base class for tests which want a ResourceManager.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/fake_url_async_fetcher.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/mock_url_fetcher.h"
#include "net/instaweb/util/public/simple_stats.h"
#include <string>
#include "net/instaweb/util/public/wait_url_async_fetcher.h"

#define URL_PREFIX "http://www.example.com/"

namespace net_instaweb {

const int kCacheSize = 100 * 1000 * 1000;

class ResourceManagerTestBase : public HtmlParseTestBaseNoAlloc {
 protected:
  ResourceManagerTestBase()
      : mock_url_async_fetcher_(&mock_url_fetcher_),
        mock_timer_(0),
        // TODO(sligocki): Someone removed setting the file_prefix, but we
        // still appear to be using it. We should see what's going on with that.
        url_prefix_(URL_PREFIX),

        lru_cache_(new LRUCache(kCacheSize)),
        http_cache_(lru_cache_, &mock_timer_),
        // TODO(sligocki): Why can't I init it here ...
        // resource_manager_(new ResourceManager(
        //    file_prefix_, &file_system_,
        //    &filename_encoder_, &mock_url_async_fetcher_, &mock_hasher_,
        //    &http_cache_, &domain_lawyer_)),
        rewrite_driver_(&message_handler_, &file_system_,
                        &mock_url_async_fetcher_),

        other_lru_cache_(new LRUCache(kCacheSize)),
        other_http_cache_(other_lru_cache_, &mock_timer_),
        other_resource_manager_(
            file_prefix_, &other_file_system_,
            &filename_encoder_, &mock_url_async_fetcher_, &mock_hasher_,
            &other_http_cache_, &other_domain_lawyer_),
        other_rewrite_driver_(&message_handler_, &other_file_system_,
                              &mock_url_async_fetcher_) {
    // rewrite_driver_.SetResourceManager(resource_manager_);
    other_rewrite_driver_.SetResourceManager(&other_resource_manager_);
  }

  virtual void SetUp() {
    HtmlParseTestBaseNoAlloc::SetUp();
    file_prefix_ = GTestTempDir() + "/";
    // TODO(sligocki): Init this in constructor.
    resource_manager_ = new ResourceManager(
        file_prefix_, &file_system_,
        &filename_encoder_, &mock_url_async_fetcher_, &mock_hasher_,
        &http_cache_, &domain_lawyer_);
    rewrite_driver_.SetResourceManager(resource_manager_);
  }

  virtual void TearDown() {
    delete resource_manager_;
    HtmlParseTestBaseNoAlloc::TearDown();
  }

  // In this set of tests, we will provide explicit body tags, so
  // the test harness should not add them in for our convenience.
  // It can go ahead and add the <html> and </html>, however.
  virtual bool AddBody() const {
    return false;
  }


  // The async fetchers in these tests are really fake async fetchers, and
  // will call their callbacks directly.  Hence we don't really need
  // any functionality in the async callback.
  class DummyCallback : public UrlAsyncFetcher::Callback {
   public:
    explicit DummyCallback(bool expect_success)
        : done_(false),
          expect_success_(expect_success) {}
    virtual ~DummyCallback() {
      EXPECT_TRUE(done_);
    }
    virtual void Done(bool success) {
      EXPECT_FALSE(done_) << "Already Done; perhaps you reused without Reset()";
      done_ = true;
      EXPECT_EQ(expect_success_, success);
    }
    void Reset() {
      done_ = false;
    }

    bool done_;
    bool expect_success_;

   private:
    DISALLOW_COPY_AND_ASSIGN(DummyCallback);
  };

  void DeleteFileIfExists(const std::string& filename) {
    if (file_system_.Exists(filename.c_str(), &message_handler_).is_true()) {
      ASSERT_TRUE(file_system_.RemoveFile(filename.c_str(), &message_handler_));
    }
  }

  void AppendDefaultHeaders(const ContentType& content_type,
                            ResourceManager* resource_manager,
                            std::string* text) {
    SimpleMetaData header;
    resource_manager->SetDefaultHeaders(&content_type, &header);
    StringWriter writer(text);
    header.Write(&writer, &message_handler_);
  }

  void ServeResourceFromManyContexts(const std::string& resource_url,
                                     RewriteOptions::Filter filter,
                                     Hasher* hasher,
                                     const StringPiece& expected_content) {
    // TODO(sligocki): Serve the resource under several contexts. For example:
    //   1) With output-resource cached,
    //   2) With output-resource not cached, but in a file,
    //   3) With output-resource unavailable, but input-resource cached,
    //   4) With output-resource unavailable and input-resource not cached,
    //      but still fetchable,
    ServeResourceFromNewContext(resource_url, filter, hasher, expected_content);
    //   5) With nothing available (failure).
  }

  // Test that a resource can be served from an new server that has not already
  // constructed it.
  void ServeResourceFromNewContext(
      const std::string& resource_url,
      RewriteOptions::Filter filter,
      Hasher* hasher,
      const StringPiece& expected_content) {
    // New objects for the new server.
    MemFileSystem other_file_system;
    // other_lru_cache is owned by other_http_cache_.
    LRUCache* other_lru_cache(new LRUCache(kCacheSize));
    MockTimer other_mock_timer(0);
    HTTPCache other_http_cache(other_lru_cache, &other_mock_timer);
    DomainLawyer other_domain_lawyer;
    WaitUrlAsyncFetcher wait_url_async_fetcher(&mock_url_fetcher_);
    ResourceManager other_resource_manager(
        file_prefix_, &other_file_system,
        &filename_encoder_, &wait_url_async_fetcher, hasher,
        &other_http_cache, &other_domain_lawyer);

    SimpleStats stats;
    RewriteDriver::Initialize(&stats);
    other_resource_manager.set_statistics(&stats);

    RewriteDriver other_rewrite_driver(&message_handler_, &other_file_system,
                                       &wait_url_async_fetcher);
    other_rewrite_driver.SetResourceManager(&other_resource_manager);

    other_rewrite_driver.AddFilter(filter);

    Variable* cached_resource_fetches =
        stats.GetVariable(RewriteDriver::kResourceFetchesCached);
    Variable* succeeded_filter_resource_fetches =
        stats.GetVariable(RewriteDriver::kResourceFetchConstructSuccesses);
    Variable* failed_filter_resource_fetches =
        stats.GetVariable(RewriteDriver::kResourceFetchConstructFailures);

    SimpleMetaData request_headers;
    // TODO(sligocki): We should set default request headers.
    SimpleMetaData response_headers;
    std::string response_contents;
    StringWriter response_writer(&response_contents);
    DummyCallback callback(true);

    // Check that we don't already have it in cache.
    EXPECT_EQ(CacheInterface::kNotFound, other_http_cache.Query(resource_url));

    // Initiate fetch.
    EXPECT_EQ(true, other_rewrite_driver.FetchResource(
        resource_url, request_headers, &response_headers, &response_writer,
        &message_handler_, &callback));

    // Content should not be set until we call the callback.
    EXPECT_EQ(false, callback.done_);
    EXPECT_EQ("", response_contents);

    // After we call the callback, it should be correct.
    wait_url_async_fetcher.CallCallbacks();
    EXPECT_EQ(true, callback.done_);
    EXPECT_EQ(expected_content, response_contents);
    EXPECT_EQ(CacheInterface::kAvailable, other_http_cache.Query(resource_url));

    // Check that stats say we took the construct resource path.
    EXPECT_EQ(0, cached_resource_fetches->Get());
    EXPECT_EQ(1, succeeded_filter_resource_fetches->Get());
    EXPECT_EQ(0, failed_filter_resource_fetches->Get());
  }


  virtual HtmlParse* html_parse() { return rewrite_driver_.html_parse(); }

  // Initializes a resource for mock fetching.
  void InitMetaData(const StringPiece& resource_name,
                    const ContentType& content_type,
                    const StringPiece& content,
                    int64 ttl) {
    std::string name = StrCat("http://test.com/", resource_name);
    SimpleMetaData response_headers;
    resource_manager_->SetDefaultHeaders(&content_type, &response_headers);
    response_headers.RemoveAll(HttpAttributes::kCacheControl);
    response_headers.Add(
        HttpAttributes::kCacheControl,
        StringPrintf("public, max-age=%ld", static_cast<long>(ttl)).c_str());
    mock_url_fetcher_.SetResponse(name, response_headers, content);
  }

  // Callback that can be used for testing resource fetches.  As all the
  // async fetchers in unit-tests call their callbacks immediately, it
  // is safe to put this on the stack, rather than having it self-delete.
  class FetchCallback : public UrlAsyncFetcher::Callback {
   public:
    FetchCallback() : success_(false), done_(false) {}
    virtual void Done(bool success) {
      success_ = success;
      done_ = true;
    }
    bool success() const { return success_; }
   private:
    bool success_;
    bool done_;
  };

  // Helper function to test resource fetching, returning true if the fetch
  // succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
  // on the status and EXPECT_EQ on the content.
  bool ServeResource(const char* path, const char* id,
                     const StringPiece& name, const char* ext,
                     std::string* content) {
    SimpleMetaData request_headers, response_headers;
    content->clear();
    StringWriter writer(content);
    FetchCallback callback;
    ResourceNamer namer;
    namer.set_id(id);
    namer.set_name(name);
    namer.set_hash("0");
    namer.set_ext(ext);
    std::string url = StrCat(path, namer.Encode());
    bool fetched = rewrite_driver_.FetchResource(
        url, request_headers, &response_headers, &writer, &message_handler_,
        &callback);
    return fetched && callback.success();
  }

  // Testdata directory.
  static const char kTestData[];

  MockUrlFetcher mock_url_fetcher_;
  FakeUrlAsyncFetcher mock_url_async_fetcher_;
  FilenameEncoder filename_encoder_;

  MockHasher mock_hasher_;
  MD5Hasher md5_hasher_;

  MockTimer mock_timer_;
  std::string file_prefix_;
  std::string url_prefix_;

  // We have two independent RewriteDrivers representing two completely
  // separate servers for the same domain (say behind a load-balancer).
  //
  // Server A runs rewrite_driver_ and will be used to rewrite pages and
  // served the rewritten resources.
  MemFileSystem file_system_;
  LRUCache* lru_cache_;  // Owned by http_cache_
  HTTPCache http_cache_;
  DomainLawyer domain_lawyer_;
  ResourceManager* resource_manager_;  // TODO(sligocki): Make not a pointer.
  RewriteDriver rewrite_driver_;

  // Server B runs other_rewrite_driver_ and will get a request for
  // resources that server A has rewritten, but server B has not heard
  // of yet. Thus, server B will have to decode the instructions on how
  // to rewrite the resource just from the request.
  MemFileSystem other_file_system_;
  LRUCache* other_lru_cache_;  // Owned by other_http_cache_
  HTTPCache other_http_cache_;
  DomainLawyer other_domain_lawyer_;
  ResourceManager other_resource_manager_;
  RewriteDriver other_rewrite_driver_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_