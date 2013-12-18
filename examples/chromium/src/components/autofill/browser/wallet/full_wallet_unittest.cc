// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/browser/wallet/full_wallet.h"
#include "components/autofill/browser/wallet/required_action.h"
#include "components/autofill/browser/wallet/wallet_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFullWalletValidResponse[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"admin_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"address_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingExpirationMonth[] =
    "{"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"address_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingExpirationYear[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"address_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingIin[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"address_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingRest[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"address_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingBillingAddress[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"address_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletWithRequiredActions[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS\","
    "    \"update_EXPIRATION_date\","
    "    \"verify_CVV\","
    "    \" REQuIrE_PHONE_NumBER\t\n\r \""
    "  ]"
    "}";

const char kFullWalletWithInvalidRequiredActions[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  setup_wallet\","
    "    \"AcCePt_ToS  \","
    "    \"UPGRADE_MIN_ADDRESS\","
    "    \"INVALID_form_field\","
    "    \"   \\tGAIA_auth   \\n\\r\","
    "    \"PASSIVE_GAIA_AUTH\","
    "    \" 忍者の正体 \""
    "  ]"
    "}";

const char kFullWalletMalformedBillingAddress[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"address_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

}  // anonymous namespace

namespace autofill {
namespace wallet {

class FullWalletTest : public testing::Test {
 public:
  FullWalletTest() {}
 protected:
  void SetUpDictionary(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
    dict.reset(static_cast<DictionaryValue*>(value.release()));
  }
  scoped_ptr<DictionaryValue> dict;
};

TEST_F(FullWalletTest, CreateFullWalletMissingExpirationMonth) {
  SetUpDictionary(kFullWalletMissingExpirationMonth);
  EXPECT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingExpirationYear) {
  SetUpDictionary(kFullWalletMissingExpirationYear);
  EXPECT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingIin) {
  SetUpDictionary(kFullWalletMissingIin);
  EXPECT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingRest) {
  SetUpDictionary(kFullWalletMissingRest);
  EXPECT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingBillingAddress) {
  SetUpDictionary(kFullWalletMissingBillingAddress);
  EXPECT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMalformedBillingAddress) {
  SetUpDictionary(kFullWalletMalformedBillingAddress);
  EXPECT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletWithRequiredActions) {
  SetUpDictionary(kFullWalletWithRequiredActions);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS);
  required_actions.push_back(UPDATE_EXPIRATION_DATE);
  required_actions.push_back(VERIFY_CVV);
  required_actions.push_back(REQUIRE_PHONE_NUMBER);

  FullWallet full_wallet(-1,
                         -1,
                         std::string(),
                         std::string(),
                         scoped_ptr<Address>(),
                         scoped_ptr<Address>(),
                         required_actions);
  EXPECT_EQ(full_wallet, *FullWallet::CreateFullWallet(*dict));

  ASSERT_FALSE(required_actions.empty());
  required_actions.pop_back();
  FullWallet different_required_actions(-1,
                                        -1,
                                        std::string(),
                                        std::string(),
                                        scoped_ptr<Address>(),
                                        scoped_ptr<Address>(),
                                        required_actions);
  EXPECT_NE(full_wallet, different_required_actions);
}

TEST_F(FullWalletTest, CreateFullWalletWithInvalidRequiredActions) {
  SetUpDictionary(kFullWalletWithInvalidRequiredActions);
  EXPECT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWallet) {
  SetUpDictionary(kFullWalletValidResponse);
  std::vector<RequiredAction> required_actions;
  FullWallet full_wallet(12,
                         2012,
                         "iin",
                         "rest",
                         GetTestAddress(),
                         GetTestShippingAddress(),
                         required_actions);
  EXPECT_EQ(full_wallet, *FullWallet::CreateFullWallet(*dict));
}

TEST_F(FullWalletTest, EvenRestDecryptionTest) {
  std::vector<RequiredAction> required_actions;
  FullWallet full_wallet(12,
                         2012,
                         "528512",
                         "5ec4feecf9d6",
                         GetTestAddress(),
                         GetTestShippingAddress(),
                         required_actions);
  std::vector<uint8> one_time_pad;
  EXPECT_TRUE(base::HexStringToBytes("5F04A8704183", &one_time_pad));
  full_wallet.set_one_time_pad(one_time_pad);
  EXPECT_EQ(ASCIIToUTF16("5285121925598459"),
            full_wallet.GetInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(ASCIIToUTF16("989"),
            full_wallet.GetInfo(CREDIT_CARD_VERIFICATION_CODE));
}

TEST_F(FullWalletTest, OddRestDecryptionTest) {
  std::vector<RequiredAction> required_actions;
  FullWallet full_wallet(12,
                         2012,
                         "528512",
                         "1a068673eb0",
                         GetTestAddress(),
                         GetTestShippingAddress(),
                         required_actions);
  std::vector<uint8> one_time_pad;
  EXPECT_TRUE(base::HexStringToBytes("075DA779F98B", &one_time_pad));
  full_wallet.set_one_time_pad(one_time_pad);
  EXPECT_EQ(ASCIIToUTF16("5285127687171393"),
            full_wallet.GetInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(ASCIIToUTF16("339"),
            full_wallet.GetInfo(CREDIT_CARD_VERIFICATION_CODE));
}

}  // namespace wallet
}  // namespace autofill
