/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#ifndef PAGESPEED_KERNEL_HTML_ELIDE_ATTRIBUTES_FILTER_H_
#define PAGESPEED_KERNEL_HTML_ELIDE_ATTRIBUTES_FILTER_H_

#include <map>
#include <set>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_name.h"

namespace net_instaweb {
class HtmlElement;
class HtmlParse;

// Remove attributes and attribute values that can be safely elided.
class ElideAttributesFilter : public EmptyHtmlFilter {
 public:
  explicit ElideAttributesFilter(HtmlParse* html_parse);
  virtual ~ElideAttributesFilter();

  void StartElement(HtmlElement* element) override;
  virtual const char* Name() const { return "ElideAttributes"; }

 private:
  struct AttrValue {
    const char* attr_value;
    bool requires_version_5;  // Default value only exists in (X)HTML 5.
  };

  typedef std::set<HtmlName::Keyword> KeywordSet;
  typedef std::map<HtmlName::Keyword, KeywordSet> KeywordSetMap;
  typedef std::map<HtmlName::Keyword, AttrValue> ValueMap;
  typedef std::map<HtmlName::Keyword, ValueMap> ValueMapMap;

  HtmlParse* html_parse_;
  KeywordSetMap one_value_attrs_map_;  // tag/attrs with only one possible value
  ValueMapMap default_value_map_;      // tag/attrs with default values

  DISALLOW_COPY_AND_ASSIGN(ElideAttributesFilter);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_ELIDE_ATTRIBUTES_FILTER_H_
