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

// ECMAScript 402 API implementation is broken into separate files for
// each service. The build system combines them together into one
// Intl namespace.

/**
 * Returns a string that matches LDML representation of the options object.
 */
function toLDMLString(options) {
  var getOption = getGetOption(options, 'dateformat');

  var ldmlString = '';

  var option = getOption('weekday', 'string', ['narrow', 'short', 'long']);
  ldmlString += appendToLDMLString(
      option, {narrow: 'EEEEE', short: 'EEE', long: 'EEEE'});

  option = getOption('era', 'string', ['narrow', 'short', 'long']);
  ldmlString += appendToLDMLString(
      option, {narrow: 'GGGGG', short: 'GGG', long: 'GGGG'});

  option = getOption('year', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'yy', 'numeric': 'y'});

  option = getOption('month', 'string',
                     ['2-digit', 'numeric', 'narrow', 'short', 'long']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'MM', 'numeric': 'M',
          'narrow': 'MMMMM', 'short': 'MMM', 'long': 'MMMM'});

  option = getOption('day', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(
      option, {'2-digit': 'dd', 'numeric': 'd'});

  var hr12 = getOption('hour12', 'boolean');
  option = getOption('hour', 'string', ['2-digit', 'numeric']);
  if (hr12 === undefined) {
    ldmlString += appendToLDMLString(option, {'2-digit': 'jj', 'numeric': 'j'});
  } else if (hr12 === true) {
    ldmlString += appendToLDMLString(option, {'2-digit': 'hh', 'numeric': 'h'});
  } else {
    ldmlString += appendToLDMLString(option, {'2-digit': 'HH', 'numeric': 'H'});
  }

  option = getOption('minute', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'mm', 'numeric': 'm'});

  option = getOption('second', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'ss', 'numeric': 's'});

  option = getOption('timeZoneName', 'string', ['short', 'long']);
  ldmlString += appendToLDMLString(option, {short: 'v', long: 'vv'});

  return ldmlString;
}


/**
 * Returns either LDML equivalent of the current option or empty string.
 */
function appendToLDMLString(option, pairs) {
  if (option !== undefined) {
    return pairs[option];
  } else {
    return '';
  }
}


/**
 * Returns object that matches LDML representation of the date.
 */
function fromLDMLString(ldmlString) {
  // First remove '' quoted text, so we lose 'Uhr' strings.
  ldmlString = ldmlString.replace(QUOTED_STRING_RE, '');

  var options = {};
  var match = ldmlString.match(/E{3,5}/g);
  options = appendToDateTimeObject(
      options, 'weekday', match, {EEEEE: 'narrow', EEE: 'short', EEEE: 'long'});

  match = ldmlString.match(/G{3,5}/g);
  options = appendToDateTimeObject(
      options, 'era', match, {GGGGG: 'narrow', GGG: 'short', GGGG: 'long'});

  match = ldmlString.match(/y{1,2}/g);
  options = appendToDateTimeObject(
      options, 'year', match, {y: 'numeric', yy: '2-digit'});

  match = ldmlString.match(/M{1,5}/g);
  options = appendToDateTimeObject(options, 'month', match, {MM: '2-digit',
      M: 'numeric', MMMMM: 'narrow', MMM: 'short', MMMM: 'long'});

  // Sometimes we get L instead of M for month - standalone name.
  match = ldmlString.match(/L{1,5}/g);
  options = appendToDateTimeObject(options, 'month', match, {LL: '2-digit',
      L: 'numeric', LLLLL: 'narrow', LLL: 'short', LLLL: 'long'});

  match = ldmlString.match(/d{1,2}/g);
  options = appendToDateTimeObject(
      options, 'day', match, {d: 'numeric', dd: '2-digit'});

  match = ldmlString.match(/h{1,2}/g);
  if (match !== null) {
    options['hour12'] = true;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {h: 'numeric', hh: '2-digit'});

  match = ldmlString.match(/H{1,2}/g);
  if (match !== null) {
    options['hour12'] = false;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {H: 'numeric', HH: '2-digit'});

  match = ldmlString.match(/m{1,2}/g);
  options = appendToDateTimeObject(
      options, 'minute', match, {m: 'numeric', mm: '2-digit'});

  match = ldmlString.match(/s{1,2}/g);
  options = appendToDateTimeObject(
      options, 'second', match, {s: 'numeric', ss: '2-digit'});

  match = ldmlString.match(/v{1,2}/g);
  options = appendToDateTimeObject(
      options, 'timeZoneName', match, {v: 'short', vv: 'long'});

  return options;
}


function appendToDateTimeObject(options, option, match, pairs) {
  if (match === null) {
    if (!options.hasOwnProperty(option)) {
      defineWEProperty(options, option, undefined);
    }
    return options;
  }

  var property = match[0];
  defineWEProperty(options, option, pairs[property]);

  return options;
}


/**
 * Returns options with at least default values in it.
 */
function toDateTimeOptions(options, required, defaults) {
  if (options === undefined) {
    options = null;
  } else {
    options = toObject(options);
  }

  options = Object.apply(this, [options]);

  var needsDefault = true;
  if ((required === 'date' || required === 'any') &&
      (options.weekday !== undefined || options.year !== undefined ||
       options.month !== undefined || options.day !== undefined)) {
    needsDefault = false;
  }

  if ((required === 'time' || required === 'any') &&
      (options.hour !== undefined || options.minute !== undefined ||
       options.second !== undefined)) {
    needsDefault = false;
  }

  if (needsDefault && (defaults === 'date' || defaults === 'all')) {
    Object.defineProperty(options, 'year', {value: 'numeric',
                                            writable: true,
                                            enumerable: true,
                                            configurable: true});
    Object.defineProperty(options, 'month', {value: 'numeric',
                                             writable: true,
                                             enumerable: true,
                                             configurable: true});
    Object.defineProperty(options, 'day', {value: 'numeric',
                                           writable: true,
                                           enumerable: true,
                                           configurable: true});
  }

  if (needsDefault && (defaults === 'time' || defaults === 'all')) {
    Object.defineProperty(options, 'hour', {value: 'numeric',
                                            writable: true,
                                            enumerable: true,
                                            configurable: true});
    Object.defineProperty(options, 'minute', {value: 'numeric',
                                              writable: true,
                                              enumerable: true,
                                              configurable: true});
    Object.defineProperty(options, 'second', {value: 'numeric',
                                              writable: true,
                                              enumerable: true,
                                              configurable: true});
  }

  return options;
}


/**
 * Initializes the given object so it's a valid DateTimeFormat instance.
 * Useful for subclassing.
 */
function initializeDateTimeFormat(dateFormat, locales, options) {
  native function NativeJSCreateDateTimeFormat();

  if (dateFormat.hasOwnProperty('__initializedIntlObject')) {
    throw new TypeError('Trying to re-initialize DateTimeFormat object.');
  }

  if (options === undefined) {
    options = {};
  }

  var locale = resolveLocale('dateformat', locales, options);

  options = toDateTimeOptions(options, 'any', 'date');

  var getOption = getGetOption(options, 'dateformat');

  // We implement only best fit algorithm, but still need to check
  // if the formatMatcher values are in range.
  var matcher = getOption('formatMatcher', 'string',
                          ['basic', 'best fit'], 'best fit');

  // Build LDML string for the skeleton that we pass to the formatter.
  var ldmlString = toLDMLString(options);

  // Filter out supported extension keys so we know what to put in resolved
  // section later on.
  // We need to pass calendar and number system to the method.
  var tz = canonicalizeTimeZoneID(options.timeZone);

  // ICU prefers options to be passed using -u- extension key/values, so
  // we need to build that.
  var internalOptions = {};
  var extensionMap = parseExtension(locale.extension);
  var extension = setOptions(options, extensionMap, DATETIME_FORMAT_KEY_MAP,
                             getOption, internalOptions);

  var requestedLocale = locale.locale + extension;
  var resolved = Object.defineProperties({}, {
    calendar: {writable: true},
    day: {writable: true},
    era: {writable: true},
    hour12: {writable: true},
    hour: {writable: true},
    locale: {writable: true},
    minute: {writable: true},
    month: {writable: true},
    numberingSystem: {writable: true},
    pattern: {writable: true},
    requestedLocale: {value: requestedLocale, writable: true},
    second: {writable: true},
    timeZone: {writable: true},
    timeZoneName: {writable: true},
    tz: {value: tz, writable: true},
    weekday: {writable: true},
    year: {writable: true}
  });

  var formatter = NativeJSCreateDateTimeFormat(
    requestedLocale, {skeleton: ldmlString, timeZone: tz}, resolved);

  if (tz !== undefined && tz !== resolved.timeZone) {
    throw new RangeError('Unsupported time zone specified ' + tz);
  }

  Object.defineProperty(dateFormat, 'formatter', {value: formatter});
  Object.defineProperty(dateFormat, 'resolved', {value: resolved});
  Object.defineProperty(dateFormat, '__initializedIntlObject',
                        {value: 'dateformat'});

  return dateFormat;
}


/**
 * Constructs Intl.DateTimeFormat object given optional locales and options
 * parameters.
 *
 * @constructor
 */
Intl.DateTimeFormat = function() {
  var locales = arguments[0];
  var options = arguments[1];

  if (!this || this === Intl) {
    // Constructor is called as a function.
    return new Intl.DateTimeFormat(locales, options);
  }

  return initializeDateTimeFormat(toObject(this), locales, options);
};


/**
 * DateTimeFormat resolvedOptions method.
 */
Intl.DateTimeFormat.prototype.resolvedOptions = function() {
  if (!this || typeof this !== 'object' ||
      this.__initializedIntlObject !== 'dateformat') {
    throw new TypeError('resolvedOptions method called on a non-object or ' +
        'on a object that is not Intl.DateTimeFormat.');
  }

  var format = this;
  var fromPattern = fromLDMLString(format.resolved.pattern);
  var userCalendar = ICU_CALENDAR_MAP[format.resolved.calendar];
  if (userCalendar === undefined) {
    // Use ICU name if we don't have a match. It shouldn't happen, but
    // it would be too strict to throw for this.
    userCalendar = format.resolved.calendar;
  }

  var locale = getOptimalLanguageTag(format.resolved.requestedLocale,
                                     format.resolved.locale);

  var result = {
    locale: locale,
    numberingSystem: format.resolved.numberingSystem,
    calendar: userCalendar,
    timeZone: format.resolved.timeZone
  };

  addWECPropertyIfDefined(result, 'timeZoneName', fromPattern.timeZoneName);
  addWECPropertyIfDefined(result, 'era', fromPattern.era);
  addWECPropertyIfDefined(result, 'year', fromPattern.year);
  addWECPropertyIfDefined(result, 'month', fromPattern.month);
  addWECPropertyIfDefined(result, 'day', fromPattern.day);
  addWECPropertyIfDefined(result, 'weekday', fromPattern.weekday);
  addWECPropertyIfDefined(result, 'hour12', fromPattern.hour12);
  addWECPropertyIfDefined(result, 'hour', fromPattern.hour);
  addWECPropertyIfDefined(result, 'minute', fromPattern.minute);
  addWECPropertyIfDefined(result, 'second', fromPattern.second);

  return result;
};


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
Intl.DateTimeFormat.supportedLocalesOf = function(locales) {
  return supportedLocalesOf('dateformat', locales, arguments[1]);
};


/**
 * Returns a String value representing the result of calling ToNumber(date)
 * according to the effective locale and the formatting options of this
 * DateTimeFormat.
 */
function formatDate(formatter, dateValue) {
  native function NativeJSInternalDateFormat();

  var dateMs;
  if (dateValue === undefined) {
    dateMs = Date.now();
  } else {
    dateMs = Number(dateValue);
  }

  if (!isFinite(dateMs)) {
    throw new RangeError('Provided date is not in valid range.');
  }

  return NativeJSInternalDateFormat(formatter.formatter, new Date(dateMs));
}


/**
 * Returns a Date object representing the result of calling ToString(value)
 * according to the effective locale and the formatting options of this
 * DateTimeFormat.
 * Returns undefined if date string cannot be parsed.
 */
function parseDate(formatter, value) {
  native function NativeJSInternalDateParse();
  return NativeJSInternalDateParse(formatter.formatter, String(value));
}


// 0 because date is optional argument.
addBoundMethod(Intl.DateTimeFormat, 'format', formatDate, 0);
addBoundMethod(Intl.DateTimeFormat, 'v8Parse', parseDate, 1);


/**
 * Returns canonical Area/Location name, or throws an exception if the zone
 * name is invalid IANA name.
 */
function canonicalizeTimeZoneID(tzID) {
  // Skip undefined zones.
  if (tzID === undefined) {
    return tzID;
  }

  // Special case handling (UTC, GMT).
  var upperID = tzID.toUpperCase();
  if (upperID === 'UTC' || upperID === 'GMT' ||
      upperID === 'ETC/UTC' || upperID === 'ETC/GMT') {
    return 'UTC';
  }

  // We expect only _ and / beside ASCII letters.
  // All inputs should conform to Area/Location from now on.
  var match = TIMEZONE_NAME_CHECK_RE.exec(tzID);
  if (match === null) {
    throw new RangeError('Expected Area/Location for time zone, got ' + tzID);
  }

  var result = toTitleCaseWord(match[1]) + '/' + toTitleCaseWord(match[2]);
  var i = 3;
  while (match[i] !== undefined && i < match.length) {
    result = result + '_' + toTitleCaseWord(match[i]);
    i++;
  }

  return result;
}
