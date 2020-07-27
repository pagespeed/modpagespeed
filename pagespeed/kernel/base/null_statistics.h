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

#ifndef PAGESPEED_KERNEL_BASE_NULL_STATISTICS_H_
#define PAGESPEED_KERNEL_BASE_NULL_STATISTICS_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/statistics_template.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class NullStatisticsVariable {
 public:
  NullStatisticsVariable(StringPiece name, Statistics* statistics) {}
  ~NullStatisticsVariable() {}
  void Set(int64 value) {}
  int64 Get() const { return 0; }
  int64 AddHelper(int64 delta) const { return 0; }
  StringPiece GetName() const { return StringPiece(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatisticsVariable);
};

// Simple name/value pair statistics implementation.
class NullStatistics : public ScalarStatisticsTemplate<NullStatisticsVariable> {
 public:
  NullStatistics();
  ~NullStatistics() override;

  CountHistogram* NewHistogram(StringPiece name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatistics);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_NULL_STATISTICS_H_
