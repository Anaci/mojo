// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_test_base.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/files/directory.mojom.h"
#include "services/files/file.mojom.h"
#include "services/files/files.mojom.h"

namespace mojo {
namespace files {
namespace {

// TODO(vtl): Stuff copied from mojo/public/cpp/bindings/lib/template_util.h.
template <class T, T v>
struct IntegralConstant {
  static const T value = v;
};

template <class T, T v>
const T IntegralConstant<T, v>::value;

typedef IntegralConstant<bool, true> TrueType;
typedef IntegralConstant<bool, false> FalseType;

template <class T>
struct IsConst : FalseType {};
template <class T>
struct IsConst<const T> : TrueType {};

template <bool B, typename T = void>
struct EnableIf {};

template <typename T>
struct EnableIf<true, T> {
  typedef T type;
};

typedef char YesType;

struct NoType {
  YesType dummy[2];
};

template <typename T>
struct IsMoveOnlyType {
  template <typename U>
  static YesType Test(const typename U::MoveOnlyTypeForCPP03*);

  template <typename U>
  static NoType Test(...);

  static const bool value =
      sizeof(Test<T>(0)) == sizeof(YesType) && !IsConst<T>::value;
};

template <typename T>
typename EnableIf<!IsMoveOnlyType<T>::value, T>::type& Forward(T& t) {
  return t;
}

template <typename T>
typename EnableIf<IsMoveOnlyType<T>::value, T>::type Forward(T& t) {
  return t.Pass();
}
// TODO(vtl): (End of stuff copied from template_util.h.)

template <typename T1>
Callback<void(T1)> Capture(T1* t1) {
  return [t1](T1 got_t1) { *t1 = Forward(got_t1); };
}

template <typename T1, typename T2>
Callback<void(T1, T2)> Capture(T1* t1, T2* t2) {
  return [t1, t2](T1 got_t1, T2 got_t2) {
    *t1 = Forward(got_t1);
    *t2 = Forward(got_t2);
  };
}

class FilesAppTest : public test::ApplicationTestBase {
 public:
  FilesAppTest() {}
  ~FilesAppTest() override {}

  void SetUp() override {
    test::ApplicationTestBase::SetUp();
    application_impl()->ConnectToService("mojo:files", &files_);
  }

  FilesPtr& files() { return files_; }

 private:
  FilesPtr files_;

  DISALLOW_COPY_AND_ASSIGN(FilesAppTest);
};

TEST_F(FilesAppTest, CreateWriteCloseRenameOpenRead) {
  // Get a temporary root directory.
  DirectoryPtr directory;
  Error error = ERROR_INTERNAL;
  files()->OpenFileSystem(FILE_SYSTEM_TEMPORARY, GetProxy(&directory),
                          Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  {
    // Create my_file.
    FilePtr file;
    error = ERROR_INTERNAL;
    directory->OpenFile("my_file", GetProxy(&file),
                        kOpenFlagWrite | kOpenFlagCreate, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingMethodCall());
    EXPECT_EQ(ERROR_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = ERROR_INTERNAL;
    uint32_t num_bytes_written = 0;
    file->Write(Array<uint8_t>::From(bytes_to_write), 0, WHENCE_FROM_CURRENT,
                Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingMethodCall());
    EXPECT_EQ(ERROR_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = ERROR_INTERNAL;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingMethodCall());
    EXPECT_EQ(ERROR_OK, error);
  }

  // Rename it.
  error = ERROR_INTERNAL;
  directory->Rename("my_file", "your_file", Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  {
    // Open my_file again.
    FilePtr file;
    error = ERROR_INTERNAL;
    directory->OpenFile("your_file", GetProxy(&file), kOpenFlagRead,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingMethodCall());
    EXPECT_EQ(ERROR_OK, error);

    // Read from it.
    Array<uint8_t> bytes_read;
    error = ERROR_INTERNAL;
    file->Read(3, 1, WHENCE_FROM_START, Capture(&error, &bytes_read));
    ASSERT_TRUE(file.WaitForIncomingMethodCall());
    EXPECT_EQ(ERROR_OK, error);
    ASSERT_EQ(3u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('e'), bytes_read[0]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[1]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[2]);
  }

  // TODO(vtl): Test various open options.
  // TODO(vtl): Test read/write offset options.
}

// Note: Ignore nanoseconds, since it may not always be supported. We expect at
// least second-resolution support though.
TEST_F(FilesAppTest, StatTouch) {
  // Get a temporary root directory.
  DirectoryPtr directory;
  Error error = ERROR_INTERNAL;
  files()->OpenFileSystem(FILE_SYSTEM_TEMPORARY, GetProxy(&directory),
                          Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Create my_file.
  FilePtr file;
  error = ERROR_INTERNAL;
  directory->OpenFile("my_file", GetProxy(&file),
                      kOpenFlagWrite | kOpenFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Stat it.
  error = ERROR_INTERNAL;
  FileInformationPtr file_info;
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(0, file_info->size);
  ASSERT_FALSE(file_info->atime.is_null());
  EXPECT_GT(file_info->atime->seconds, 0);  // Expect that it's not 1970-01-01.
  ASSERT_FALSE(file_info->mtime.is_null());
  EXPECT_GT(file_info->mtime->seconds, 0);
  int64_t first_mtime = file_info->mtime->seconds;

  // Touch only the atime.
  error = ERROR_INTERNAL;
  TimespecOrNowPtr t(TimespecOrNow::New());
  t->now = false;
  t->timespec = Timespec::New();
  const int64_t kPartyTime1 = 1234567890;  // Party like it's 2009-02-13.
  t->timespec->seconds = kPartyTime1;
  file->Touch(t.Pass(), nullptr, Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Stat again.
  error = ERROR_INTERNAL;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  ASSERT_FALSE(file_info->atime.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime->seconds);
  ASSERT_FALSE(file_info->mtime.is_null());
  EXPECT_EQ(first_mtime, file_info->mtime->seconds);

  // Touch only the mtime.
  t = TimespecOrNow::New();
  t->now = false;
  t->timespec = Timespec::New();
  const int64_t kPartyTime2 = 1425059525;  // No time like the present.
  t->timespec->seconds = kPartyTime2;
  file->Touch(nullptr, t.Pass(), Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Stat again.
  error = ERROR_INTERNAL;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  ASSERT_FALSE(file_info->atime.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime->seconds);
  ASSERT_FALSE(file_info->mtime.is_null());
  EXPECT_EQ(kPartyTime2, file_info->mtime->seconds);

  // TODO(vtl): Also test non-zero file size.
  // TODO(vtl): Also test Touch() "now" options.
  // TODO(vtl): Also test touching both atime and mtime.

  // Close it.
  error = ERROR_INTERNAL;
  file->Close(Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
}

TEST_F(FilesAppTest, TellSeek) {
  // Get a temporary root directory.
  DirectoryPtr directory;
  Error error = ERROR_INTERNAL;
  files()->OpenFileSystem(FILE_SYSTEM_TEMPORARY, GetProxy(&directory),
                          Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Create my_file.
  FilePtr file;
  error = ERROR_INTERNAL;
  directory->OpenFile("my_file", GetProxy(&file),
                      kOpenFlagWrite | kOpenFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write(1000, '!');
  error = ERROR_INTERNAL;
  uint32_t num_bytes_written = 0;
  file->Write(Array<uint8_t>::From(bytes_to_write), 0, WHENCE_FROM_CURRENT,
              Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int size = static_cast<int>(num_bytes_written);

  // Tell.
  error = ERROR_INTERNAL;
  int64_t position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  // Should be at the end.
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(size, position);

  // Seek back 100.
  error = ERROR_INTERNAL;
  position = -1;
  file->Seek(-100, WHENCE_FROM_CURRENT, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(size - 100, position);

  // Tell.
  error = ERROR_INTERNAL;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(size - 100, position);

  // Seek to 123 from start.
  error = ERROR_INTERNAL;
  position = -1;
  file->Seek(123, WHENCE_FROM_START, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(123, position);

  // Tell.
  error = ERROR_INTERNAL;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(123, position);

  // Seek to 123 back from end.
  error = ERROR_INTERNAL;
  position = -1;
  file->Seek(-123, WHENCE_FROM_END, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(size - 123, position);

  // Tell.
  error = ERROR_INTERNAL;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(size - 123, position);

  // TODO(vtl): Check that seeking actually affects reading/writing.
  // TODO(vtl): Check that seeking can extend the file?

  // Close it.
  error = ERROR_INTERNAL;
  file->Close(Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
}

TEST_F(FilesAppTest, Dup) {
  // Get a temporary root directory.
  DirectoryPtr directory;
  Error error = ERROR_INTERNAL;
  files()->OpenFileSystem(FILE_SYSTEM_TEMPORARY, GetProxy(&directory),
                          Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Create my_file.
  FilePtr file1;
  error = ERROR_INTERNAL;
  directory->OpenFile("my_file", GetProxy(&file1),
                      kOpenFlagRead | kOpenFlagWrite | kOpenFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write;
  bytes_to_write.push_back(static_cast<uint8_t>('h'));
  bytes_to_write.push_back(static_cast<uint8_t>('e'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('o'));
  error = ERROR_INTERNAL;
  uint32_t num_bytes_written = 0;
  file1->Write(Array<uint8_t>::From(bytes_to_write), 0, WHENCE_FROM_CURRENT,
               Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file1.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int end_hello_pos = static_cast<int>(num_bytes_written);

  // Dup it.
  FilePtr file2;
  error = ERROR_INTERNAL;
  file1->Dup(GetProxy(&file2), Capture(&error));
  ASSERT_TRUE(file1.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // |file2| should have the same position.
  error = ERROR_INTERNAL;
  int64_t position = -1;
  file2->Tell(Capture(&error, &position));
  ASSERT_TRUE(file2.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(end_hello_pos, position);

  // Write using |file2|.
  std::vector<uint8_t> more_bytes_to_write;
  more_bytes_to_write.push_back(static_cast<uint8_t>('w'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('o'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('r'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('l'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('d'));
  error = ERROR_INTERNAL;
  num_bytes_written = 0;
  file2->Write(Array<uint8_t>::From(more_bytes_to_write), 0,
               WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file2.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(more_bytes_to_write.size(), num_bytes_written);
  const int end_world_pos = end_hello_pos + static_cast<int>(num_bytes_written);

  // |file1| should have the same position.
  error = ERROR_INTERNAL;
  position = -1;
  file1->Tell(Capture(&error, &position));
  ASSERT_TRUE(file1.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(end_world_pos, position);

  // Close |file1|.
  error = ERROR_INTERNAL;
  file1->Close(Capture(&error));
  ASSERT_TRUE(file1.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Read everything using |file2|.
  Array<uint8_t> bytes_read;
  error = ERROR_INTERNAL;
  file2->Read(1000, 0, WHENCE_FROM_START, Capture(&error, &bytes_read));
  ASSERT_TRUE(file2.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  ASSERT_EQ(static_cast<size_t>(end_world_pos), bytes_read.size());
  // Just check the first and last bytes.
  EXPECT_EQ(static_cast<uint8_t>('h'), bytes_read[0]);
  EXPECT_EQ(static_cast<uint8_t>('d'), bytes_read[end_world_pos - 1]);

  // Close |file2|.
  error = ERROR_INTERNAL;
  file2->Close(Capture(&error));
  ASSERT_TRUE(file2.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // TODO(vtl): Test that |file2| has the same open options as |file1|.
}

}  // namespace
}  // namespace files
}  // namespace mojo
