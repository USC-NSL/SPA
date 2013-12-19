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

#ifndef V8_I18N_SRC_BREAK_ITERATOR_H
#define V8_I18N_SRC_BREAK_ITERATOR_H_

#include "unicode/uversion.h"
#include "v8/include/v8.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
class UnicodeString;
}

namespace v8_i18n {

class BreakIterator {
 public:
  static v8::Handle<v8::Value> JSCreateBreakIterator(const v8::Arguments& args);

  // Helper methods for various bindings.

  // Unpacks iterator object from corresponding JavaScript object.
  static icu::BreakIterator* UnpackBreakIterator(
      v8::Handle<v8::Object> obj);

  // Release memory we allocated for the BreakIterator once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteBreakIterator(v8::Isolate* isolate,
                                  v8::Persistent<v8::Value> object,
				  void* param);

  // Assigns new text to the iterator.
  static v8::Handle<v8::Value> JSInternalBreakIteratorAdoptText(
      const v8::Arguments& args);

  // Moves iterator to the beginning of the string and returns new position.
  static v8::Handle<v8::Value> JSInternalBreakIteratorFirst(
      const v8::Arguments& args);

  // Moves iterator to the next position and returns it.
  static v8::Handle<v8::Value> JSInternalBreakIteratorNext(
      const v8::Arguments& args);

  // Returns current iterator's current position.
  static v8::Handle<v8::Value> JSInternalBreakIteratorCurrent(
      const v8::Arguments& args);

  // Returns type of the item from current position.
  // This call is only valid for word break iterators. Others just return 0.
  static v8::Handle<v8::Value> JSInternalBreakIteratorBreakType(
      const v8::Arguments& args);

 private:
  BreakIterator() {}
};

}  // namespace v8_i18n

#endif  // V8_I18N_SRC_BREAK_ITERATOR
