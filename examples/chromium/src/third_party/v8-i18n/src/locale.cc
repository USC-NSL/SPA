// Copyright 2012 the v8-i18n authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/locale.h"

#include <string.h>

#include "unicode/brkiter.h"
#include "unicode/coll.h"
#include "unicode/datefmt.h"
#include "unicode/numfmt.h"
#include "unicode/uloc.h"
#include "unicode/uversion.h"

namespace v8_i18n {

v8::Handle<v8::Value> JSCanonicalizeLanguageTag(const v8::Arguments& args) {
  // Expect locale id which is a string.
  if (args.Length() != 1 || !args[0]->IsString()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Locale identifier, as a string, is required.")));
  }

  UErrorCode error = U_ZERO_ERROR;

  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;

  // Return value which denotes invalid language tag.
  const char* const kInvalidTag = "invalid-tag";

  v8::String::AsciiValue locale_id(args[0]->ToString());
  if (*locale_id == NULL) {
    return v8::String::New(kInvalidTag);
  }

  uloc_forLanguageTag(*locale_id, icu_result, ULOC_FULLNAME_CAPACITY,
                      &icu_length, &error);
  if (U_FAILURE(error) || icu_length == 0) {
    return v8::String::New(kInvalidTag);
  }

  char result[ULOC_FULLNAME_CAPACITY];

  // Force strict BCP47 rules.
  uloc_toLanguageTag(icu_result, result, ULOC_FULLNAME_CAPACITY, TRUE, &error);

  if (U_FAILURE(error)) {
    return v8::String::New(kInvalidTag);
  }

  return v8::String::New(result);
}

v8::Handle<v8::Value> JSAvailableLocalesOf(const v8::Arguments& args) {
  // Expect service name which is a string.
  if (args.Length() != 1 || !args[0]->IsString()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Service identifier, as a string, is required.")));
  }

  const icu::Locale* available_locales = NULL;

  int32_t count = 0;
  v8::String::AsciiValue service(args[0]->ToString());
  if (strcmp(*service, "collator") == 0) {
    available_locales = icu::Collator::getAvailableLocales(count);
  } else if (strcmp(*service, "numberformat") == 0) {
    available_locales = icu::NumberFormat::getAvailableLocales(count);
  } else if (strcmp(*service, "dateformat") == 0) {
    available_locales = icu::DateFormat::getAvailableLocales(count);
  } else if (strcmp(*service, "breakiterator") == 0) {
    available_locales = icu::BreakIterator::getAvailableLocales(count);
  }

  v8::TryCatch try_catch;
  UErrorCode error = U_ZERO_ERROR;
  char result[ULOC_FULLNAME_CAPACITY];
  v8::Handle<v8::Object> locales = v8::Object::New();

  for (int32_t i = 0; i < count; ++i) {
    const char* icu_name = available_locales[i].getName();

    error = U_ZERO_ERROR;
    // No need to force strict BCP47 rules.
    uloc_toLanguageTag(icu_name, result, ULOC_FULLNAME_CAPACITY, FALSE, &error);
    if (U_FAILURE(error)) {
      // This shouldn't happen, but lets not break the user.
      continue;
    }

    // Index is just a dummy value for the property value.
    locales->Set(v8::String::New(result), v8::Integer::New(i));
    if (try_catch.HasCaught()) {
      // Ignore error, but stop processing and return.
      break;
    }
  }

  return locales;
}

v8::Handle<v8::Value> JSGetDefaultICULocale(const v8::Arguments& args) {
  icu::Locale default_locale;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  UErrorCode status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      default_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    return v8::String::New(result);
  }

  return v8::String::New("und");
}

v8::Handle<v8::Value> JSGetLanguageTagVariants(const v8::Arguments& args) {
  v8::TryCatch try_catch;

  // Expect an array of strings.
  if (args.Length() != 1 || !args[0]->IsArray()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Internal error. Expected Array<String>.")));
  }

  v8::Local<v8::Array> input = v8::Local<v8::Array>::Cast(args[0]);
  v8::Handle<v8::Array> output = v8::Array::New(input->Length());
  for (unsigned int i = 0; i < input->Length(); ++i) {
    v8::Local<v8::Value> locale_id = input->Get(i);
    if (try_catch.HasCaught()) {
      break;
    }

    if (!locale_id->IsString()) {
      return v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Array element is missing "
                          "or it isn't a string.")));
    }

    v8::String::AsciiValue ascii_locale_id(locale_id);
    if (*ascii_locale_id == NULL) {
      return v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Non-ASCII locale identifier.")));
    }

    UErrorCode error = U_ZERO_ERROR;

    // Convert from BCP47 to ICU format.
    // de-DE-u-co-phonebk -> de_DE@collation=phonebook
    char icu_locale[ULOC_FULLNAME_CAPACITY];
    int icu_locale_length = 0;
    uloc_forLanguageTag(*ascii_locale_id, icu_locale, ULOC_FULLNAME_CAPACITY,
                        &icu_locale_length, &error);
    if (U_FAILURE(error) || icu_locale_length == 0) {
      return v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Failed to convert locale to ICU.")));
    }

    // Maximize the locale.
    // de_DE@collation=phonebook -> de_Latn_DE@collation=phonebook
    char icu_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_addLikelySubtags(
        icu_locale, icu_max_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Remove extensions from maximized locale.
    // de_Latn_DE@collation=phonebook -> de_Latn_DE
    char icu_base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(
        icu_max_locale, icu_base_max_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Get original name without extensions.
    // de_DE@collation=phonebook -> de_DE
    char icu_base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(
        icu_locale, icu_base_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Convert from ICU locale format to BCP47 format.
    // de_Latn_DE -> de-Latn-DE
    char base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(icu_base_max_locale, base_max_locale,
                       ULOC_FULLNAME_CAPACITY, FALSE, &error);

    // de_DE -> de-DE
    char base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(
        icu_base_locale, base_locale, ULOC_FULLNAME_CAPACITY, FALSE, &error);

    if (U_FAILURE(error)) {
      return v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Couldn't generate maximized "
                          "or base locale.")));
    }

    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("maximized"), v8::String::New(base_max_locale));
    result->Set(v8::String::New("base"), v8::String::New(base_locale));
    if (try_catch.HasCaught()) {
      break;
    }

    output->Set(i, result);
    if (try_catch.HasCaught()) {
      break;
    }
  }

  return output;
}

}  // namespace v8_i18n
