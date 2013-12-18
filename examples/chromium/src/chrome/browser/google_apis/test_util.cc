// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/test_util.h"

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/pending_task.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_server/http_request.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace google_apis {
namespace test_util {

// This class is used to monitor if any task is posted to a message loop.
class TaskObserver : public MessageLoop::TaskObserver {
 public:
  TaskObserver() : posted_(false) {}
  virtual ~TaskObserver() {}

  // MessageLoop::TaskObserver overrides.
  virtual void WillProcessTask(const base::PendingTask& pending_task) OVERRIDE {
  }
  virtual void DidProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    posted_ = true;
  }

  // Returns true if any task was posted.
  bool posted() const { return posted_; }

 private:
  bool posted_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

bool RemovePrefix(const std::string& input,
                  const std::string& prefix,
                  std::string* output) {
  if (!StartsWithASCII(input, prefix, true /* case sensitive */))
    return false;

  *output = input.substr(prefix.size());
  return true;
}

base::FilePath GetTestFilePath(const std::string& relative_path) {
  base::FilePath path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &path))
    return base::FilePath();
  path = path.AppendASCII("chrome")
             .AppendASCII("test")
             .AppendASCII("data")
             .Append(base::FilePath::FromUTF8Unsafe(relative_path));
  return path;
}

GURL GetBaseUrlForTesting(int port) {
  return GURL(base::StringPrintf("http://127.0.0.1:%d/", port));
}

void RunBlockingPoolTask() {
  while (true) {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();

    TaskObserver task_observer;
    MessageLoop::current()->AddTaskObserver(&task_observer);
    MessageLoop::current()->RunUntilIdle();
    MessageLoop::current()->RemoveTaskObserver(&task_observer);
    if (!task_observer.posted())
      break;
  }
}

void RunAndQuit(const base::Closure& closure) {
  closure.Run();
  MessageLoop::current()->Quit();
}

scoped_ptr<base::Value> LoadJSONFile(const std::string& relative_path) {
  base::FilePath path = GetTestFilePath(relative_path);

  std::string error;
  JSONFileValueSerializer serializer(path);
  scoped_ptr<base::Value> value(serializer.Deserialize(NULL, &error));
  LOG_IF(WARNING, !value.get()) << "Failed to parse " << path.value()
                                << ": " << error;
  return value.Pass();
}

// Returns a HttpResponse created from the given file path.
scoped_ptr<test_server::HttpResponse> CreateHttpResponseFromFile(
    const base::FilePath& file_path) {
  std::string content;
  if (!file_util::ReadFileToString(file_path, &content))
    return scoped_ptr<test_server::HttpResponse>();

  std::string content_type = "text/plain";
  if (EndsWith(file_path.AsUTF8Unsafe(), ".json", true /* case sensitive */)) {
    content_type = "application/json";
  } else if (EndsWith(file_path.AsUTF8Unsafe(), ".xml", true)) {
    content_type = "application/atom+xml";
  }

  scoped_ptr<test_server::HttpResponse> http_response(
      new test_server::HttpResponse);
  http_response->set_code(test_server::SUCCESS);
  http_response->set_content(content);
  http_response->set_content_type(content_type);
  return http_response.Pass();
}

void DoNothingForReAuthenticateCallback(
    AuthenticatedOperationInterface* /* operation */) {
  NOTREACHED();
}

scoped_ptr<test_server::HttpResponse> HandleDownloadRequest(
    const GURL& base_url,
    test_server::HttpRequest* out_request,
    const test_server::HttpRequest& request) {
  *out_request = request;

  GURL absolute_url = base_url.Resolve(request.relative_url);
  std::string remaining_path;
  if (!RemovePrefix(absolute_url.path(), "/files/", &remaining_path))
    return scoped_ptr<test_server::HttpResponse>();
  return CreateHttpResponseFromFile(GetTestFilePath(remaining_path));
}

bool VerifyJsonData(const base::FilePath& expected_json_file_path,
                    const base::Value* json_data) {
  if (!json_data) {
    LOG(ERROR) << "json_data is NULL";
    return false;
  }

  std::string expected_content;
  if (!file_util::ReadFileToString(
          expected_json_file_path, &expected_content)) {
    LOG(ERROR) << "Failed to read file: " << expected_json_file_path.value();
    return false;
  }

  scoped_ptr<base::Value> expected_json_data(
      base::JSONReader::Read(expected_content));
  if (!base::Value::Equals(expected_json_data.get(), json_data)) {
    LOG(ERROR)
        << "The value of json_data is different from the file's content.";
    return false;
  }

  return true;
}

bool ParseContentRangeHeader(const std::string& value,
                             int64* start_position,
                             int64* end_position,
                             int64* length) {
  DCHECK(start_position);
  DCHECK(end_position);
  DCHECK(length);

  std::string remaining;
  if (!RemovePrefix(value, "bytes ", &remaining))
    return false;

  std::vector<std::string> parts;
  base::SplitString(remaining, '/', &parts);
  if (parts.size() != 2U)
    return false;

  const std::string range = parts[0];
  if (!base::StringToInt64(parts[1], length))
    return false;

  parts.clear();
  base::SplitString(range, '-', &parts);
  if (parts.size() != 2U)
    return false;

  return (base::StringToInt64(parts[0], start_position) &&
          base::StringToInt64(parts[1], end_position));
}

void AppendProgressCallbackResult(std::vector<ProgressInfo>* progress_values,
                                  int64 progress,
                                  int64 total) {
  progress_values->push_back(ProgressInfo(progress, total));
}

TestGetContentCallback::TestGetContentCallback()
    : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          base::Bind(&TestGetContentCallback::OnGetContent,
                     base::Unretained(this)))) {
}

TestGetContentCallback::~TestGetContentCallback() {
}

std::string TestGetContentCallback::GetConcatenatedData() const {
  std::string result;
  for (size_t i = 0; i < data_.size(); ++i) {
    result += *data_[i];
  }
  return result;
}

void TestGetContentCallback::OnGetContent(google_apis::GDataErrorCode error,
                                          scoped_ptr<std::string> data) {
  data_.push_back(data.release());
}

}  // namespace test_util
}  // namespace google_apis
