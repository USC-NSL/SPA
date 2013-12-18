// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  function printTestPage() {
    chrome.send('PrintTestPage');
    chrome.send('DialogClose');
  }

  function setMessage(msg) {
    $('msg-content').innerHTML = msg;
  }

  function onPageShown() {
    $('close').focus();
  }

  return {
    printTestPage: printTestPage,
    setMessage: setMessage,
    onPageShown: onPageShown
  };
});
