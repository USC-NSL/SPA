/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UseCounter_h
#define UseCounter_h

#include "wtf/BitVector.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMWindow;
class Document;
class ScriptExecutionContext;

// UseCounter is used for counting the number of times features of
// Blink are used on real web pages and help us know commonly
// features are used and thus when it's safe to remove or change them.
//
// The Chromium Content layer controls what is done with this data.
// For instance, in Google Chrome, these counts are submitted
// anonymously through the Histogram recording system in Chrome
// for users who opt-in to "Usage Statistics" submission
// during their install of Google Chrome:
// http://www.google.com/chrome/intl/en/privacy.html

class UseCounter {
    WTF_MAKE_NONCOPYABLE(UseCounter);
public:
    UseCounter();
    ~UseCounter();

    enum Feature {
        PageDestruction,
        LegacyNotifications,
        MultipartMainResource,
        PrefixedIndexedDB,
        WorkerStart,
        SharedWorkerStart,
        LegacyWebAudio,
        WebAudioStart,
        PrefixedContentSecurityPolicy,
        UnprefixedIndexedDB,
        OpenWebDatabase,
        LegacyHTMLNotifications,
        LegacyTextNotifications,
        UnprefixedRequestAnimationFrame,
        PrefixedRequestAnimationFrame,
        ContentSecurityPolicy,
        ContentSecurityPolicyReportOnly,
        PrefixedContentSecurityPolicyReportOnly,
        PrefixedTransitionEndEvent,
        UnprefixedTransitionEndEvent,
        PrefixedAndUnprefixedTransitionEndEvent,
        AutoFocusAttribute,
        AutoSaveAttribute,
        DataListElement,
        FormAttribute,
        IncrementalAttribute,
        InputTypeColor,
        InputTypeDate,
        InputTypeDateTime,
        InputTypeDateTimeFallback,
        InputTypeDateTimeLocal,
        InputTypeEmail,
        InputTypeMonth,
        InputTypeNumber,
        InputTypeRange,
        InputTypeSearch,
        InputTypeTel,
        InputTypeTime,
        InputTypeURL,
        InputTypeWeek,
        InputTypeWeekFallback,
        ListAttribute,
        MaxAttribute,
        MinAttribute,
        PatternAttribute,
        PlaceholderAttribute,
        PrecisionAttribute,
        PrefixedDirectoryAttribute,
        PrefixedSpeechAttribute,
        RequiredAttribute,
        ResultsAttribute,
        StepAttribute,
        PageVisits,
        HTMLMarqueeElement,
        CSSOverflowMarquee,
        Reflection,
        CursorVisibility,
        StorageInfo,
        XFrameOptions,
        XFrameOptionsSameOrigin,
        XFrameOptionsSameOriginWithBadAncestorChain,
        DeprecatedFlexboxWebContent,
        DeprecatedFlexboxChrome,
        DeprecatedFlexboxChromeExtension,
        SVGTRefElement,
        UnprefixedPerformanceTimeline,
        PrefixedPerformanceTimeline,
        UnprefixedUserTiming,
        PrefixedUserTiming,
        WindowEvent,
        ContentSecurityPolicyWithBaseElement,
        PrefixedMediaAddKey,
        PrefixedMediaGenerateKeyRequest,
        WebAudioLooping,
        // Add new features above this line. Don't change assigned numbers of each items.
        NumberOfFeatures, // This enum value must be last.
    };

    // "count" sets the bit for this feature to 1. Repeated calls are ignored.
    static void count(Document*, Feature);
    static void count(DOMWindow*, Feature);

    // "countDeprecation" sets the bit for this feature to 1, and sends a deprecation
    // warning to the console. Repeated calls are ignored.
    //
    // Be considerate to developers' consoles: features should only send deprecation warnings
    // when we're actively interested in removing them from the platform.
    static void countDeprecation(DOMWindow*, Feature);
    static void countDeprecation(ScriptExecutionContext*, Feature);
    static void countDeprecation(Document*, Feature);
    String deprecationMessage(Feature);

    void didCommitLoad();

private:
    bool recordMeasurement(Feature feature)
    {
        ASSERT(feature != PageDestruction); // PageDestruction is reserved as a scaling factor.
        ASSERT(feature < NumberOfFeatures);
        if (!m_countBits) {
            m_countBits = adoptPtr(new BitVector(NumberOfFeatures));
            m_countBits->clearAll();
        }

        if (m_countBits->quickGet(feature))
            return false;

        m_countBits->quickSet(feature);
        return true;
    }

    void updateMeasurements();

    OwnPtr<BitVector> m_countBits;
};

} // namespace WebCore

#endif // UseCounter_h
