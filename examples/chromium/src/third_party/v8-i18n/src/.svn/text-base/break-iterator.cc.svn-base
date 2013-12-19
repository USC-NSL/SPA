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

#include "src/break-iterator.h"

#include <string.h>

#include "src/utils.h"
#include "unicode/brkiter.h"
#include "unicode/locid.h"
#include "unicode/rbbi.h"

namespace v8_i18n {

static v8::Handle<v8::Value> ThrowUnexpectedObjectError();
static icu::UnicodeString* ResetAdoptedText(v8::Handle<v8::Object>,
					    v8::Handle<v8::Value>);
static icu::BreakIterator* InitializeBreakIterator(v8::Handle<v8::String>,
						   v8::Handle<v8::Object>,
						   v8::Handle<v8::Object>);
static icu::BreakIterator* CreateICUBreakIterator(const icu::Locale&,
						  v8::Handle<v8::Object>);
static void SetResolvedSettings(const icu::Locale&,
                                icu::BreakIterator*,
                                v8::Handle<v8::Object>);

icu::BreakIterator* BreakIterator::UnpackBreakIterator(
    v8::Handle<v8::Object> obj) {
  v8::HandleScope handle_scope;

  // v8::ObjectTemplate doesn't have HasInstance method so we can't check
  // if obj is an instance of BreakIterator class. We'll check for a property
  // that has to be in the object. The same applies to other services, like
  // Collator and DateTimeFormat.
  if (obj->HasOwnProperty(v8::String::New("breakIterator"))) {
    return static_cast<icu::BreakIterator*>(
        obj->GetAlignedPointerFromInternalField(0));
  }

  return NULL;
}

void BreakIterator::DeleteBreakIterator(v8::Isolate* isolate,
                                        v8::Persistent<v8::Value> object,
                                        void* param) {
  v8::Persistent<v8::Object> persistent_object =
      v8::Persistent<v8::Object>::Cast(object);

  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a break iterator.
  delete UnpackBreakIterator(persistent_object);

  delete static_cast<icu::UnicodeString*>(
      persistent_object->GetAlignedPointerFromInternalField(1));

  // Then dispose of the persistent handle to JS object.
  persistent_object.Dispose(isolate);
}

// Throws a JavaScript exception.
static v8::Handle<v8::Value> ThrowUnexpectedObjectError() {
  // Returns undefined, and schedules an exception to be thrown.
  return v8::ThrowException(v8::Exception::Error(
      v8::String::New("BreakIterator method called on an object "
                      "that is not a BreakIterator.")));
}

// Deletes the old value and sets the adopted text in corresponding
// JavaScript object.
icu::UnicodeString* ResetAdoptedText(
    v8::Handle<v8::Object> obj, v8::Handle<v8::Value> value) {
  // Get the previous value from the internal field.
  icu::UnicodeString* text = static_cast<icu::UnicodeString*>(
      obj->GetAlignedPointerFromInternalField(1));
  delete text;

  // Assign new value to the internal pointer.
  v8::String::Value text_value(value);
  text = new icu::UnicodeString(
      reinterpret_cast<const UChar*>(*text_value), text_value.length());
  obj->SetAlignedPointerInInternalField(1, text);

  // Return new unicode string pointer.
  return text;
}

v8::Handle<v8::Value> BreakIterator::JSInternalBreakIteratorAdoptText(
    const v8::Arguments& args) {
  if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsString()) {
    return v8::ThrowException(v8::Exception::Error(
        v8::String::New(
            "Internal error. Iterator and text have to be specified.")));
  }

  icu::BreakIterator* break_iterator = UnpackBreakIterator(args[0]->ToObject());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  break_iterator->setText(*ResetAdoptedText(args[0]->ToObject(), args[1]));

  return v8::Undefined();
}

v8::Handle<v8::Value> BreakIterator::JSInternalBreakIteratorFirst(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args[0]->ToObject());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  return v8::Int32::New(break_iterator->first());
}

v8::Handle<v8::Value> BreakIterator::JSInternalBreakIteratorNext(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args[0]->ToObject());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  return v8::Int32::New(break_iterator->next());
}

v8::Handle<v8::Value> BreakIterator::JSInternalBreakIteratorCurrent(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args[0]->ToObject());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  return v8::Int32::New(break_iterator->current());
}

v8::Handle<v8::Value> BreakIterator::JSInternalBreakIteratorBreakType(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args[0]->ToObject());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  // TODO(cira): Remove cast once ICU fixes base BreakIterator class.
  icu::RuleBasedBreakIterator* rule_based_iterator =
      static_cast<icu::RuleBasedBreakIterator*>(break_iterator);
  int32_t status = rule_based_iterator->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return v8::String::New("none");
  } else if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return v8::String::New("number");
  } else if (status >= UBRK_WORD_LETTER && status < UBRK_WORD_LETTER_LIMIT) {
    return v8::String::New("letter");
  } else if (status >= UBRK_WORD_KANA && status < UBRK_WORD_KANA_LIMIT) {
    return v8::String::New("kana");
  } else if (status >= UBRK_WORD_IDEO && status < UBRK_WORD_IDEO_LIMIT) {
    return v8::String::New("ideo");
  } else {
    return v8::String::New("unknown");
  }
}

v8::Handle<v8::Value> BreakIterator::JSCreateBreakIterator(
    const v8::Arguments& args) {
  if (args.Length() != 3 ||
      !args[0]->IsString() ||
      !args[1]->IsObject() ||
      !args[2]->IsObject()) {
    return v8::ThrowException(v8::Exception::Error(
        v8::String::New("Internal error, wrong parameters.")));
  }

  v8::Isolate* isolate = args.GetIsolate();
  v8::Persistent<v8::ObjectTemplate> break_iterator_template =
      Utils::GetTemplate2(isolate);

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object = break_iterator_template->NewInstance();
  // But the handle shouldn't be empty.
  // That can happen if there was a stack overflow when creating the object.
  if (local_object.IsEmpty()) {
    return local_object;
  }

  v8::Persistent<v8::Object> wrapper =
      v8::Persistent<v8::Object>::New(isolate, local_object);

  // Set break iterator as internal field of the resulting JS object.
  icu::BreakIterator* break_iterator = InitializeBreakIterator(
      args[0]->ToString(), args[1]->ToObject(), args[2]->ToObject());

  if (!break_iterator) {
    return v8::ThrowException(v8::Exception::Error(v8::String::New(
        "Internal error. Couldn't create ICU break iterator.")));
  } else {
    wrapper->SetAlignedPointerInInternalField(0, break_iterator);
    // Make sure that the pointer to adopted text is NULL.
    wrapper->SetAlignedPointerInInternalField(1, NULL);

    v8::TryCatch try_catch;
    wrapper->Set(v8::String::New("breakIterator"), v8::String::New("valid"));
    if (try_catch.HasCaught()) {
      return v8::ThrowException(v8::Exception::Error(
          v8::String::New("Internal error, couldn't set property.")));
    }
  }

  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak(isolate, NULL, DeleteBreakIterator);

  return wrapper;
}

static icu::BreakIterator* InitializeBreakIterator(
    v8::Handle<v8::String> locale,
    v8::Handle<v8::Object> options,
    v8::Handle<v8::Object> resolved) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::AsciiValue bcp47_locale(locale);
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return NULL;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::BreakIterator* break_iterator =
    CreateICUBreakIterator(icu_locale, options);
  if (!break_iterator) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    break_iterator = CreateICUBreakIterator(no_extension_locale, options);

    // Set resolved settings (locale).
    SetResolvedSettings(no_extension_locale, break_iterator, resolved);
  } else {
    SetResolvedSettings(icu_locale, break_iterator, resolved);
  }

  return break_iterator;
}

static icu::BreakIterator* CreateICUBreakIterator(
    const icu::Locale& icu_locale, v8::Handle<v8::Object> options) {
  UErrorCode status = U_ZERO_ERROR;
  icu::BreakIterator* break_iterator = NULL;
  icu::UnicodeString type;
  if (!Utils::ExtractStringSetting(options, "type", &type)) {
    // Type had to be in the options. This would be an internal error.
    return NULL;
  }

  if (type == UNICODE_STRING_SIMPLE("character")) {
    break_iterator =
      icu::BreakIterator::createCharacterInstance(icu_locale, status);
  } else if (type == UNICODE_STRING_SIMPLE("sentence")) {
    break_iterator =
      icu::BreakIterator::createSentenceInstance(icu_locale, status);
  } else if (type == UNICODE_STRING_SIMPLE("line")) {
    break_iterator =
      icu::BreakIterator::createLineInstance(icu_locale, status);
  } else {
    // Defualt is word iterator.
    break_iterator =
      icu::BreakIterator::createWordInstance(icu_locale, status);
  }

  if (U_FAILURE(status)) {
    delete break_iterator;
    return NULL;
  }

  return break_iterator;
}

static void SetResolvedSettings(const icu::Locale& icu_locale,
                                icu::BreakIterator* date_format,
                                v8::Handle<v8::Object> resolved) {
  UErrorCode status = U_ZERO_ERROR;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    resolved->Set(v8::String::New("locale"), v8::String::New(result));
  } else {
    // This would never happen, since we got the locale from ICU.
    resolved->Set(v8::String::New("locale"), v8::String::New("und"));
  }
}

}  // namespace v8_i18n
