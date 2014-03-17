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
//
// Input resource created based on a local file.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FILE_INPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FILE_INPUT_RESOURCE_H_

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class MessageHandler;
class ResourceManager;
struct ContentType;

class FileInputResource : public Resource {
 public:
  FileInputResource(ResourceManager* resource_manager,
                    const ContentType* type,
                    const StringPiece& url,
                    const StringPiece& filename)
      : Resource(resource_manager, type),
        url_(url.data(), url.size()),
        filename_(filename.data(), filename.size()) {
  }

  virtual ~FileInputResource();

  virtual GoogleString url() const { return url_; }

 protected:
  virtual bool Load(MessageHandler* message_handler);

 private:
  GoogleString url_;
  GoogleString filename_;

  DISALLOW_COPY_AND_ASSIGN(FileInputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FILE_INPUT_RESOURCE_H_