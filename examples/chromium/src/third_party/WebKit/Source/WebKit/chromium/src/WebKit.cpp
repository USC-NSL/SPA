/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKit.h"

#include "IDBFactoryBackendProxy.h"
#include "WebMediaPlayerClientImpl.h"
#include "WebWorkerClientImpl.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8RecursionScope.h"
#include "core/dom/CustomElementRegistry.h"
#include "core/dom/MutationObserver.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "RuntimeEnabledFeatures.h"
#include "core/page/Settings.h"
#include "core/platform/EventTracer.h"
#include "core/platform/LayoutTestSupport.h"
#include "core/platform/Logging.h"
#include "core/platform/graphics/chromium/ImageDecodingStore.h"
#include "core/platform/graphics/chromium/MediaPlayerPrivateChromium.h"
#include "core/platform/text/TextEncoding.h"
#include "core/workers/WorkerContextProxy.h"
#include "v8.h"
#include <public/Platform.h>
#include <public/WebPrerenderingSupport.h>
#include <public/WebThread.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/text/AtomicString.h>
#include <wtf/Threading.h>
#include <wtf/UnusedParam.h>

namespace WebKit {

namespace {

class EndOfTaskRunner : public WebThread::TaskObserver {
public:
    virtual void willProcessTask() { }
    virtual void didProcessTask()
    {
        WebCore::CustomElementRegistry::deliverAllLifecycleCallbacks();
        WebCore::MutationObserver::deliverAllMutations();
    }
};

} // namespace

static WebThread::TaskObserver* s_endOfTaskRunner = 0;

// Make sure we are not re-initialized in the same address space.
// Doing so may cause hard to reproduce crashes.
static bool s_webKitInitialized = false;

static Platform* s_webKitPlatformSupport = 0;

static bool generateEntropy(unsigned char* buffer, size_t length)
{
    if (s_webKitPlatformSupport) {
        s_webKitPlatformSupport->cryptographicallyRandomValues(buffer, length);
        return true;
    }
    return false;
}

#ifndef NDEBUG
static void assertV8RecursionScope()
{
    ASSERT(!isMainThread() || WebCore::V8RecursionScope::properlyUsed());
}
#endif

void initialize(Platform* webKitPlatformSupport)
{
    initializeWithoutV8(webKitPlatformSupport);

    v8::V8::SetEntropySource(&generateEntropy);
    v8::V8::Initialize();
    WebCore::V8PerIsolateData::ensureInitialized(v8::Isolate::GetCurrent());

    // currentThread will always be non-null in production, but can be null in Chromium unit tests.
    if (WebThread* currentThread = webKitPlatformSupport->currentThread()) {
#ifndef NDEBUG
        v8::V8::AddCallCompletedCallback(&assertV8RecursionScope);
#endif
        ASSERT(!s_endOfTaskRunner);
        s_endOfTaskRunner = new EndOfTaskRunner;
        currentThread->addTaskObserver(s_endOfTaskRunner);
    }
}

void initializeWithoutV8(Platform* webKitPlatformSupport)
{
    ASSERT(!s_webKitInitialized);
    s_webKitInitialized = true;

    ASSERT(webKitPlatformSupport);
    ASSERT(!s_webKitPlatformSupport);
    s_webKitPlatformSupport = webKitPlatformSupport;
    Platform::initialize(s_webKitPlatformSupport);

    WTF::initializeThreading();
    WTF::initializeMainThread();
    WebCore::init();
    WebCore::ImageDecodingStore::initializeOnce();

    // There are some code paths (for example, running WebKit in the browser
    // process and calling into LocalStorage before anything else) where the
    // UTF8 string encoding tables are used on a background thread before
    // they're set up.  This is a problem because their set up routines assert
    // they're running on the main WebKitThread.  It might be possible to make
    // the initialization thread-safe, but given that so many code paths use
    // this, initializing this lazily probably doesn't buy us much.
    WebCore::UTF8Encoding();

    WebCore::EventTracer::initialize();

    WebCore::setIDBFactoryBackendInterfaceCreateFunction(WebKit::IDBFactoryBackendProxy::create);

    WebCore::MediaPlayerPrivate::setMediaEngineRegisterSelfFunction(WebKit::WebMediaPlayerClientImpl::registerSelf);

    WebCore::WorkerContextProxy::setCreateDelegate(WebWorkerClientImpl::createWorkerContextProxy);
}


void shutdown()
{
    // WebKit might have been initialized without V8, so be careful not to invoke
    // V8 specific functions, if V8 was not properly initialized.
    if (s_endOfTaskRunner) {
#ifndef NDEBUG
        v8::V8::RemoveCallCompletedCallback(&assertV8RecursionScope);
#endif
        ASSERT(s_webKitPlatformSupport->currentThread());
        s_webKitPlatformSupport->currentThread()->removeTaskObserver(s_endOfTaskRunner);
        delete s_endOfTaskRunner;
        s_endOfTaskRunner = 0;
    }
    s_webKitPlatformSupport = 0;
    WebCore::ImageDecodingStore::shutdown();
    Platform::shutdown();
    WebPrerenderingSupport::shutdown();
}

Platform* webKitPlatformSupport()
{
    return s_webKitPlatformSupport;
}

void setLayoutTestMode(bool value)
{
    WebCore::setIsRunningLayoutTest(value);
}

bool layoutTestMode()
{
    return WebCore::isRunningLayoutTest();
}

void enableLogChannel(const char* name)
{
#if !LOG_DISABLED
    WTFLogChannel* channel = WebCore::getChannelFromName(name);
    if (channel)
        channel->state = WTFLogChannelOn;
#else
    UNUSED_PARAM(name);
#endif // !LOG_DISABLED
}

void resetPluginCache(bool reloadPages)
{
    WebCore::Page::refreshPlugins(reloadPages);
}

} // namespace WebKit
