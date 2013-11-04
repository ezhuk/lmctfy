// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lmctfy/cli/commands/spec.h"

#include <memory>
#include <vector>

#include "gflags/gflags.h"
#include "include/lmctfy.h"
#include "include/lmctfy.pb.h"
#include "include/lmctfy_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

DECLARE_bool(lmctfy_binary);

using ::std::unique_ptr;
using ::std::vector;
using ::testing::Ge;
using ::testing::Return;
using ::util::Status;

namespace containers {
namespace lmctfy {
namespace cli {
namespace {

static const char kContainerName[] = "/test";

class SpecTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    mock_lmctfy_.reset(new StrictMockContainerApi());
    mock_container_ = new StrictMockContainer(kContainerName);
  }

 protected:
  unique_ptr<MockContainerApi> mock_lmctfy_;
  MockContainer *mock_container_;
};

TEST_F(SpecTest, Success) {
  const vector<string> args = {"spec", kContainerName};
  ContainerSpec spec;
  spec.mutable_memory()->set_limit(10);

  EXPECT_CALL(*mock_lmctfy_, Get(kContainerName))
      .WillOnce(Return(mock_container_));
  EXPECT_CALL(*mock_container_, Spec())
      .WillRepeatedly(Return(spec));

  FLAGS_lmctfy_binary = false;
  EXPECT_TRUE(SpecContainer(args, mock_lmctfy_.get(), nullptr).ok());
}

TEST_F(SpecTest, SuccessBinary) {
  const vector<string> args = {"spec", kContainerName};
  ContainerSpec spec;
  spec.mutable_memory()->set_limit(10);

  EXPECT_CALL(*mock_lmctfy_, Get(kContainerName))
      .WillOnce(Return(mock_container_));
  EXPECT_CALL(*mock_container_, Spec())
      .WillRepeatedly(Return(spec));

  FLAGS_lmctfy_binary = true;
  EXPECT_TRUE(SpecContainer(args, mock_lmctfy_.get(), nullptr).ok());
}

TEST_F(SpecTest, SuccessSelf) {
  const vector<string> args = {"spec"};
  ContainerSpec spec;
  spec.mutable_memory()->set_limit(10);

  EXPECT_CALL(*mock_lmctfy_, Detect(Ge(0)))
      .WillRepeatedly(Return(string(kContainerName)));
  EXPECT_CALL(*mock_lmctfy_, Get(kContainerName))
      .WillOnce(Return(mock_container_));
  EXPECT_CALL(*mock_container_, Spec())
      .WillRepeatedly(Return(spec));

  FLAGS_lmctfy_binary = false;
  EXPECT_TRUE(SpecContainer(args, mock_lmctfy_.get(), nullptr).ok());
}

TEST_F(SpecTest, SummarySelfDetectFails) {
  const vector<string> args = {"spec"};

  EXPECT_CALL(*mock_lmctfy_, Detect(Ge(0)))
      .WillRepeatedly(Return(Status::CANCELLED));

  FLAGS_lmctfy_binary = false;
  EXPECT_EQ(Status::CANCELLED,
            SpecContainer(args, mock_lmctfy_.get(), nullptr));
}

TEST_F(SpecTest, GetFails) {
  const vector<string> args = {"spec", kContainerName};

  EXPECT_CALL(*mock_lmctfy_, Get(kContainerName))
      .WillOnce(Return(Status::CANCELLED));

  FLAGS_lmctfy_binary = false;
  EXPECT_EQ(Status::CANCELLED,
            SpecContainer(args, mock_lmctfy_.get(), nullptr));

  // Delete the container since it is never returned.
  delete mock_container_;
}

TEST_F(SpecTest, SpecFails) {
  const vector<string> args = {"spec", kContainerName};

  EXPECT_CALL(*mock_lmctfy_, Get(kContainerName))
      .WillOnce(Return(mock_container_));
  EXPECT_CALL(*mock_container_, Spec())
      .WillRepeatedly(Return(Status::CANCELLED));

  FLAGS_lmctfy_binary = false;
  EXPECT_EQ(Status::CANCELLED,
            SpecContainer(args, mock_lmctfy_.get(), nullptr));
}

}  // namespace
}  // namespace cli
}  // namespace lmctfy
}  // namespace containers
