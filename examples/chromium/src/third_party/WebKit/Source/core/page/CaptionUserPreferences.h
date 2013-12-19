/*
 * Copyright (C) 2012, 2013  Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CaptionUserPreferences_h
#define CaptionUserPreferences_h

#include "core/html/track/TextTrack.h"
#include "core/platform/Language.h"
#include "core/platform/LocalizedStrings.h"
#include "core/platform/Timer.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class HTMLMediaElement;
class PageGroup;
class TextTrackList;

class CaptionUserPreferences {
public:
    static PassOwnPtr<CaptionUserPreferences> create(PageGroup* group) { return adoptPtr(new CaptionUserPreferences(group)); }
    virtual ~CaptionUserPreferences();

    virtual bool userHasCaptionPreferences() const { return m_testingMode && m_havePreferences; }
    virtual bool shouldShowCaptions() const;
    virtual void setShouldShowCaptions(bool);

    virtual int textTrackSelectionScore(TextTrack*, HTMLMediaElement*) const;
    virtual int textTrackLanguageSelectionScore(TextTrack*) const;

    virtual bool userPrefersCaptions() const;
    virtual void setUserPrefersCaptions(bool);

    virtual bool userPrefersSubtitles() const;
    virtual void setUserPrefersSubtitles(bool preference);
    
    virtual bool userPrefersTextDescriptions() const;
    virtual void setUserPrefersTextDescriptions(bool preference);

    virtual float captionFontSizeScale(bool& important) const { important = false; return 0.05f; }
    virtual String captionsStyleSheetOverride() const { return emptyString(); }

    virtual void setInterestedInCaptionPreferenceChanges() { }

    virtual void captionPreferencesChanged();

    virtual void setPreferredLanguage(String);
    virtual Vector<String> preferredLanguages() const;

    virtual String displayNameForTrack(TextTrack*) const;
    virtual Vector<RefPtr<TextTrack> > sortedTrackListForMenu(TextTrackList*);

    virtual bool testingMode() const { return m_testingMode; }
    virtual void setTestingMode(bool override) { m_testingMode = override; }

    PageGroup* pageGroup() const { return m_pageGroup; }

protected:
    CaptionUserPreferences(PageGroup*);

private:
    void timerFired(Timer<CaptionUserPreferences>*);
    void notify();

    PageGroup* m_pageGroup;
    Timer<CaptionUserPreferences> m_timer;
    String m_userPreferredLanguage;
    bool m_testingMode;
    bool m_havePreferences;
    bool m_shouldShowCaptions;
};
    
}

#endif
