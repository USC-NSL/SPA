// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/features/feature.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_util.h"

// TODO(kbr): remove: http://crbug.com/222296
#if defined(OS_MACOSX)
#import "base/mac/mac_util.h"
#endif

using extensions::Extension;

namespace utils = extension_function_test_utils;

namespace {

class NotificationsApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  const extensions::Extension* LoadExtensionAndWait(
      const std::string& test_name) {
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    content::WindowedNotificationObserver page_created(
        chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
        content::NotificationService::AllSources());
    const extensions::Extension* extension = LoadExtension(extdir);
    if (extension) {
      page_created.Wait();
    }
    return extension;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestIdUsage) {
#if defined(OS_MACOSX)
  // TODO(kbr): re-enable: http://crbug.com/222296
  if (base::mac::IsOSMountainLionOrLater())
    return;
#endif

  // Create a new notification. A lingering output of this block is the
  // notifications ID, which we'll use in later parts of this test.
  std::string notification_id;
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());
  {
    scoped_refptr<extensions::NotificationsCreateFunction>
        notification_function(
            new extensions::NotificationsCreateFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function,
        "[\"\", "  // Empty string: ask API to generate ID
        "{"
        "\"type\": \"simple\","
        "\"iconUrl\": \"an/image/that/does/not/exist.png\","
        "\"title\": \"Attention!\","
        "\"message\": \"Check out Cirque du Soleil\""
        "}]",
        browser(), utils::NONE));

    ASSERT_EQ(base::Value::TYPE_STRING, result->GetType());
    ASSERT_TRUE(result->GetAsString(&notification_id));
    ASSERT_TRUE(notification_id.length() > 0);
  }

  // Update the existing notification.
  {
    scoped_refptr<extensions::NotificationsUpdateFunction>
        notification_function(
            new extensions::NotificationsUpdateFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function,
        "[\"" + notification_id + "\", "
        "{"
        "\"type\": \"simple\","
        "\"iconUrl\": \"an/image/that/does/not/exist.png\","
        "\"title\": \"Attention!\","
        "\"message\": \"Too late! The show ended yesterday\""
        "}]",
        browser(), utils::NONE));

    ASSERT_EQ(base::Value::TYPE_BOOLEAN, result->GetType());
    bool copy_bool_value = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_value));
    ASSERT_TRUE(copy_bool_value);

    // TODO(miket): add a testing method to query the message from the
    // displayed notification, and assert it matches the updated message.
    //
    // TODO(miket): add a method to count the number of outstanding
    // notifications, and confirm it remains at one at this point.
  }

  // Update a nonexistent notification.
  {
    scoped_refptr<extensions::NotificationsUpdateFunction>
        notification_function(
            new extensions::NotificationsUpdateFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function,
        "[\"xxxxxxxxxxxx\", "
        "{"
        "\"type\": \"simple\","
        "\"iconUrl\": \"an/image/that/does/not/exist.png\","
        "\"title\": \"!\","
        "\"message\": \"!\""
        "}]",
        browser(), utils::NONE));

    ASSERT_EQ(base::Value::TYPE_BOOLEAN, result->GetType());
    bool copy_bool_value = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_value));
    ASSERT_FALSE(copy_bool_value);
  }

  // Clear a nonexistent notification.
  {
    scoped_refptr<extensions::NotificationsClearFunction>
        notification_function(
            new extensions::NotificationsClearFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function,
        "[\"xxxxxxxxxxx\"]", browser(), utils::NONE));

    ASSERT_EQ(base::Value::TYPE_BOOLEAN, result->GetType());
    bool copy_bool_value = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_value));
    ASSERT_FALSE(copy_bool_value);
  }

  // Clear the notification we created.
  {
    scoped_refptr<extensions::NotificationsClearFunction>
        notification_function(
            new extensions::NotificationsClearFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function,
        "[\"" + notification_id + "\"]", browser(), utils::NONE));

    ASSERT_EQ(base::Value::TYPE_BOOLEAN, result->GetType());
    bool copy_bool_value = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_value));
    ASSERT_TRUE(copy_bool_value);
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestBaseFormatNotification) {
  scoped_refptr<extensions::NotificationsCreateFunction>
      notification_create_function(
          new extensions::NotificationsCreateFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  notification_create_function->set_extension(empty_extension.get());
  notification_create_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      notification_create_function,
      "[\"\", "
      "{"
      "\"type\": \"basic\","
      "\"iconUrl\": \"an/image/that/does/not/exist.png\","
      "\"title\": \"Attention!\","
      "\"message\": \"Check out Cirque du Soleil\","
      "\"priority\": 1,"
      "\"eventTime\": 1234567890.12345678,"
      "\"buttons\": ["
      "  {"
      "   \"title\": \"Up\","
      "   \"iconUrl\":\"http://www.google.com/logos/2012/\""
      "  },"
      "  {"
      "   \"title\": \"Down\""  // note: no iconUrl
      "  }"
      "],"
      "\"expandedMessage\": \"This is a longer expanded message.\","
      "\"imageUrl\": \"http://www.google.com/logos/2012/election12-hp.jpg\""
      "}]",
      browser(), utils::NONE));

  std::string notification_id;
  ASSERT_EQ(base::Value::TYPE_STRING, result->GetType());
  ASSERT_TRUE(result->GetAsString(&notification_id));
  ASSERT_TRUE(notification_id.length() > 0);
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestMultipleItemNotification) {
  scoped_refptr<extensions::NotificationsCreateFunction>
      notification_create_function(
          new extensions::NotificationsCreateFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  notification_create_function->set_extension(empty_extension.get());
  notification_create_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      notification_create_function,
      "[\"\", "
      "{"
      "\"type\": \"list\","
      "\"iconUrl\": \"an/image/that/does/not/exist.png\","
      "\"title\": \"Multiple Item Notification Title\","
      "\"message\": \"Multiple item notification message.\","
      "\"items\": ["
      "  {\"title\": \"Brett Boe\","
      " \"message\": \"This is an important message!\"},"
      "  {\"title\": \"Carla Coe\","
      " \"message\": \"Just took a look at the proposal\"},"
      "  {\"title\": \"Donna Doe\","
      " \"message\": \"I see that you went to the conference\"},"
      "  {\"title\": \"Frank Foe\","
      " \"message\": \"I ate Harry's sandwich!\"},"
      "  {\"title\": \"Grace Goe\","
      " \"message\": \"I saw Frank steal a sandwich :-)\"}"
      "],"
      "\"priority\": 1,"
      "\"eventTime\": 1361488019.9999999"
      "}]",
      browser(), utils::NONE));
  // TODO(dharcourt): [...], items = [{title: foo, message: bar}, ...], [...]

  std::string notification_id;
  ASSERT_EQ(base::Value::TYPE_STRING, result->GetType());
  ASSERT_TRUE(result->GetAsString(&notification_id));
  ASSERT_TRUE(notification_id.length() > 0);
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestEvents) {
#if defined(OS_MACOSX)
  // TODO(kbr): re-enable: http://crbug.com/222296
  if (base::mac::IsOSMountainLionOrLater())
    return;
#endif

  ASSERT_TRUE(RunExtensionTest("notifications/api/events")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestCSP) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/csp")) << message_;
}

#ifdef ENABLE_MESSAGE_CENTER
#if !defined(OS_WIN) || !defined(USE_ASH)

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestByUser) {
  if (!message_center::IsRichNotificationEnabled())
    return;

  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/by_user");
  ASSERT_TRUE(extension) << message_;

  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveNotification(
        extension->id() + "-FOO",
        false);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveNotification(
        extension->id() + "-BAR",
        true);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

#endif
#endif
