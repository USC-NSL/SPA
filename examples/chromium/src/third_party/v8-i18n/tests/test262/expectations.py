#!/usr/bin/python
#
# Copyright 2012 the v8-i18n authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Bugs
expect_fail = {
  # ISO and CLDR data mismatch. http://unicode.org/cldr/trac/ticket/5302
  'data/test/suite/intl402/ch11/11.1/11.1.1_20_c.js': 'FAIL',
  # ICU bug http://bugs.icu-project.org/trac/ticket/9547
  'data/test/suite/intl402/ch11/11.3/11.3.2_TRP.js': 'FAIL',
  # ICU bug http://bugs.icu-project.org/trac/ticket/9265
  'data/test/suite/intl402/ch09/9.2/9.2.5_11_g_ii_2.js': 'FAIL',
  # ICU bug http://bugs.icu-project.org/trac/ticket/9562
  'data/test/suite/intl402/ch06/6.2/6.2.3.js': 'FAIL',
  # Not implemented yet (overrides for toLocaleDate...)
  'data/test/suite/intl402/ch13/13.2/13.2.1_1.js': 'FAIL',
  'data/test/suite/intl402/ch13/13.2/13.2.1_4.js': 'FAIL',
  # V8 issue - can't detect if a function is called as a constructor.
  'data/test/suite/intl402/ch10/10.2/10.2.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch10/10.3/10.3.2_1_a_L15.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.2/11.2.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.3/11.3.2_1_a_L15.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.2/12.2.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.3/12.3.2_1_a_L15.js': 'FAIL',
  # Can't delete prototype property from non-constructor function.
  'data/test/suite/intl402/ch10/10.3/10.3.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch10/10.3/10.3.3_L15.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.3/11.3.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.3/11.3.3_L15.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.3/12.3.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.3/12.3.3_L15.js': 'FAIL',
  # V8 preformance issue with defineProperty and arguments[]. See r143.
  'data/test/suite/intl402/ch08/8.0/8.0_L15.js': 'FAIL',
  'data/test/suite/intl402/ch10/10.1/10.1_L15.js': 'FAIL',
  'data/test/suite/intl402/ch10/10.2/10.2.1.js': 'FAIL',
  'data/test/suite/intl402/ch10/10.2/10.2.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch10/10.3/10.3_L15.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.1/11.1_L15.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.2/11.2.1.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.2/11.2.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch11/11.3/11.3_L15.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.1/12.1_L15.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.2/12.2.1.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.2/12.2.2_L15.js': 'FAIL',
  'data/test/suite/intl402/ch12/12.3/12.3_L15.js': 'FAIL',
}

# Deliberate incompatibilities
incompatible = {
}
