/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/rendering/RenderThemeChromiumSkia.h"

#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "UserAgentStyleSheets.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/TimeRanges.h"
#include "core/html/shadow/MediaControlElements.h"
#include "core/platform/LayoutTestSupport.h"
#include "core/platform/ScrollbarTheme.h"
#include "core/platform/graphics/Font.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/skia/PlatformContextSkia.h"
#include "core/platform/graphics/transforms/TransformationMatrix.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderMediaControlsChromium.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderProgress.h"
#include "core/rendering/RenderSlider.h"
#include "core/rendering/RenderThemeChromiumFontProvider.h"

#include <wtf/CurrentTime.h>

#include "SkGradientShader.h"
#include "SkShader.h"

namespace WebCore {

enum PaddingType {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

static const int styledMenuListInternalPadding[4] = { 1, 4, 1, 4 };

// These values all match Safari/Win.
static const float defaultControlFontPixelSize = 13;
static const float defaultCancelButtonSize = 9;
static const float minCancelButtonSize = 5;
static const float maxCancelButtonSize = 21;
static const float defaultSearchFieldResultsDecorationSize = 13;
static const float minSearchFieldResultsDecorationSize = 9;
static const float maxSearchFieldResultsDecorationSize = 30;
static const float defaultSearchFieldResultsButtonWidth = 18;

RenderThemeChromiumSkia::RenderThemeChromiumSkia()
{
}

RenderThemeChromiumSkia::~RenderThemeChromiumSkia()
{
}

// Use the Windows style sheets to match their metrics.
String RenderThemeChromiumSkia::extraDefaultStyleSheet()
{
    return String(themeWinUserAgentStyleSheet, sizeof(themeWinUserAgentStyleSheet)) +
           String(themeChromiumSkiaUserAgentStyleSheet, sizeof(themeChromiumSkiaUserAgentStyleSheet)) +
           String(themeChromiumUserAgentStyleSheet, sizeof(themeChromiumUserAgentStyleSheet));
}

String RenderThemeChromiumSkia::extraQuirksStyleSheet()
{
    return String(themeWinQuirksUserAgentStyleSheet, sizeof(themeWinQuirksUserAgentStyleSheet));
}

String RenderThemeChromiumSkia::extraMediaControlsStyleSheet()
{
    return String(mediaControlsChromiumUserAgentStyleSheet, sizeof(mediaControlsChromiumUserAgentStyleSheet));
}

bool RenderThemeChromiumSkia::supportsHover(const RenderStyle* style) const
{
    return true;
}

bool RenderThemeChromiumSkia::supportsFocusRing(const RenderStyle* style) const
{
    // This causes WebKit to draw the focus rings for us.
    return false;
}

bool RenderThemeChromiumSkia::supportsDataListUI(const AtomicString& type) const
{
    return RenderThemeChromiumCommon::supportsDataListUI(type);
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI) && ENABLE(CALENDAR_PICKER)
bool RenderThemeChromiumSkia::supportsCalendarPicker(const AtomicString& type) const
{
    return RenderThemeChromiumCommon::supportsCalendarPicker(type);
}
#endif

bool RenderThemeChromiumSkia::supportsClosedCaptioning() const
{
    return true;
}

Color RenderThemeChromiumSkia::platformActiveSelectionBackgroundColor() const
{
    return Color(0x1e, 0x90, 0xff);
}

Color RenderThemeChromiumSkia::platformInactiveSelectionBackgroundColor() const
{
    return Color(0xc8, 0xc8, 0xc8);
}

Color RenderThemeChromiumSkia::platformActiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeChromiumSkia::platformInactiveSelectionForegroundColor() const
{
    return Color(0x32, 0x32, 0x32);
}

Color RenderThemeChromiumSkia::platformFocusRingColor() const
{
    static Color focusRingColor(229, 151, 0, 255);
    return focusRingColor;
}

double RenderThemeChromiumSkia::caretBlinkInterval() const
{
    // Disable the blinking caret in layout test mode, as it introduces
    // a race condition for the pixel tests. http://b/1198440
    if (isRunningLayoutTest())
        return 0;

    return caretBlinkIntervalInternal();
}

void RenderThemeChromiumSkia::systemFont(int propId, FontDescription& fontDescription) const
{
    RenderThemeChromiumFontProvider::systemFont(propId, fontDescription);
}

int RenderThemeChromiumSkia::minimumMenuListSize(RenderStyle* style) const
{
    return 0;
}

// These are the default dimensions of radio buttons and checkboxes.
static const int widgetStandardWidth = 13;
static const int widgetStandardHeight = 13;

// Return a rectangle that has the same center point as |original|, but with a
// size capped at |width| by |height|.
IntRect center(const IntRect& original, int width, int height)
{
    width = std::min(original.width(), width);
    height = std::min(original.height(), height);
    int x = original.x() + (original.width() - width) / 2;
    int y = original.y() + (original.height() - height) / 2;

    return IntRect(x, y, width, height);
}

void RenderThemeChromiumSkia::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary
    // for now.  It matches Firefox.  At different DPI settings on Windows,
    // querying the theme gives you a larger size that accounts for the higher
    // DPI.  Until our entire engine honors a DPI setting other than 96, we
    // can't rely on the theme's metrics.
    const IntSize size(widgetStandardWidth, widgetStandardHeight);
    setSizeIfAuto(style, size);
}

void RenderThemeChromiumSkia::setRadioSize(RenderStyle* style) const
{
    // Use same sizing for radio box as checkbox.
    setCheckboxSize(style);
}

void RenderThemeChromiumSkia::adjustButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    if (style->appearance() == PushButtonPart) {
        // Ignore line-height.
        style->setLineHeight(RenderStyle::initialLineHeight());
    }
}

bool RenderThemeChromiumSkia::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeChromiumSkia::adjustSearchFieldStyle(StyleResolver*, RenderStyle* style, Element*) const
{
     // Ignore line-height.
     style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeChromiumSkia::paintSearchField(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeChromiumSkia::adjustSearchFieldCancelButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int cancelButtonSize = lroundf(std::min(std::max(minCancelButtonSize, defaultCancelButtonSize * fontScale), maxCancelButtonSize));
    style->setWidth(Length(cancelButtonSize, Fixed));
    style->setHeight(Length(cancelButtonSize, Fixed));
}

IntRect RenderThemeChromiumSkia::convertToPaintingRect(RenderObject* inputRenderer, const RenderObject* partRenderer, LayoutRect partRect, const IntRect& localOffset) const
{
    // Compute an offset between the part renderer and the input renderer.
    LayoutSize offsetFromInputRenderer = -partRenderer->offsetFromAncestorContainer(inputRenderer);
    // Move the rect into partRenderer's coords.
    partRect.move(offsetFromInputRenderer);
    // Account for the local drawing offset.
    partRect.move(localOffset.x(), localOffset.y());

    return pixelSnappedIntRect(partRect);
}

bool RenderThemeChromiumSkia::paintSearchFieldCancelButton(RenderObject* cancelButtonObject, const PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    Node* input = cancelButtonObject->node()->shadowHost();
    RenderObject* baseRenderer = input ? input->renderer() : cancelButtonObject;
    if (!baseRenderer->isBox())
        return false;
    RenderBox* inputRenderBox = toRenderBox(baseRenderer);
    LayoutRect inputContentBox = inputRenderBox->contentBoxRect();

    // Make sure the scaled button stays square and will fit in its parent's box.
    LayoutUnit cancelButtonSize = std::min(inputContentBox.width(), std::min<LayoutUnit>(inputContentBox.height(), r.height()));
    // Calculate cancel button's coordinates relative to the input element.
    // Center the button vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    LayoutRect cancelButtonRect(cancelButtonObject->offsetFromAncestorContainer(inputRenderBox).width(),
                                inputContentBox.y() + (inputContentBox.height() - cancelButtonSize + 1) / 2,
                                cancelButtonSize, cancelButtonSize);
    IntRect paintingRect = convertToPaintingRect(inputRenderBox, cancelButtonObject, cancelButtonRect, r);

    static Image* cancelImage = Image::loadPlatformResource("searchCancel").leakRef();
    static Image* cancelPressedImage = Image::loadPlatformResource("searchCancelPressed").leakRef();
    paintInfo.context->drawImage(isPressed(cancelButtonObject) ? cancelPressedImage : cancelImage,
                                 cancelButtonObject->style()->colorSpace(), paintingRect);
    return false;
}

void RenderThemeChromiumSkia::adjustSearchFieldDecorationStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    IntSize emptySize(1, 11);
    style->setWidth(Length(emptySize.width(), Fixed));
    style->setHeight(Length(emptySize.height(), Fixed));
}

void RenderThemeChromiumSkia::adjustSearchFieldResultsDecorationStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    // Scale the decoration size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int magnifierSize = lroundf(std::min(std::max(minSearchFieldResultsDecorationSize, defaultSearchFieldResultsDecorationSize * fontScale),
                                         maxSearchFieldResultsDecorationSize));
    style->setWidth(Length(magnifierSize, Fixed));
    style->setHeight(Length(magnifierSize, Fixed));
}

bool RenderThemeChromiumSkia::paintSearchFieldResultsDecoration(RenderObject* magnifierObject, const PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    Node* input = magnifierObject->node()->shadowHost();
    RenderObject* baseRenderer = input ? input->renderer() : magnifierObject;
    if (!baseRenderer->isBox())
        return false;
    RenderBox* inputRenderBox = toRenderBox(baseRenderer);
    LayoutRect inputContentBox = inputRenderBox->contentBoxRect();

    // Make sure the scaled decoration stays square and will fit in its parent's box.
    LayoutUnit magnifierSize = std::min(inputContentBox.width(), std::min<LayoutUnit>(inputContentBox.height(), r.height()));
    // Calculate decoration's coordinates relative to the input element.
    // Center the decoration vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    LayoutRect magnifierRect(magnifierObject->offsetFromAncestorContainer(inputRenderBox).width(),
                             inputContentBox.y() + (inputContentBox.height() - magnifierSize + 1) / 2,
                             magnifierSize, magnifierSize);
    IntRect paintingRect = convertToPaintingRect(inputRenderBox, magnifierObject, magnifierRect, r);

    static Image* magnifierImage = Image::loadPlatformResource("searchMagnifier").leakRef();
    paintInfo.context->drawImage(magnifierImage, magnifierObject->style()->colorSpace(), paintingRect);
    return false;
}

void RenderThemeChromiumSkia::adjustSearchFieldResultsButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int magnifierHeight = lroundf(std::min(std::max(minSearchFieldResultsDecorationSize, defaultSearchFieldResultsDecorationSize * fontScale),
                                           maxSearchFieldResultsDecorationSize));
    int magnifierWidth = lroundf(magnifierHeight * defaultSearchFieldResultsButtonWidth / defaultSearchFieldResultsDecorationSize);
    style->setWidth(Length(magnifierWidth, Fixed));
    style->setHeight(Length(magnifierHeight, Fixed));
}

bool RenderThemeChromiumSkia::paintSearchFieldResultsButton(RenderObject* magnifierObject, const PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    Node* input = magnifierObject->node()->shadowHost();
    RenderObject* baseRenderer = input ? input->renderer() : magnifierObject;
    if (!baseRenderer->isBox())
        return false;
    RenderBox* inputRenderBox = toRenderBox(baseRenderer);
    LayoutRect inputContentBox = inputRenderBox->contentBoxRect();

    // Make sure the scaled decoration will fit in its parent's box.
    LayoutUnit magnifierHeight = std::min<LayoutUnit>(inputContentBox.height(), r.height());
    LayoutUnit magnifierWidth = std::min<LayoutUnit>(inputContentBox.width(), magnifierHeight * defaultSearchFieldResultsButtonWidth / defaultSearchFieldResultsDecorationSize);
    LayoutRect magnifierRect(magnifierObject->offsetFromAncestorContainer(inputRenderBox).width(),
                             inputContentBox.y() + (inputContentBox.height() - magnifierHeight + 1) / 2,
                             magnifierWidth, magnifierHeight);
    IntRect paintingRect = convertToPaintingRect(inputRenderBox, magnifierObject, magnifierRect, r);

    static Image* magnifierImage = Image::loadPlatformResource("searchMagnifierResults").leakRef();
    paintInfo.context->drawImage(magnifierImage, magnifierObject->style()->colorSpace(), paintingRect);
    return false;
}

bool RenderThemeChromiumSkia::paintMediaSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaSlider, object, paintInfo, rect);
}

bool RenderThemeChromiumSkia::paintMediaVolumeSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaVolumeSlider, object, paintInfo, rect);
}

void RenderThemeChromiumSkia::adjustSliderThumbSize(RenderStyle* style, Element*) const
{
    RenderMediaControlsChromium::adjustMediaSliderThumbSize(style);
}

bool RenderThemeChromiumSkia::paintMediaSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaSliderThumb, object, paintInfo, rect);
}

bool RenderThemeChromiumSkia::paintMediaToggleClosedCaptionsButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaShowClosedCaptionsButton, o, paintInfo, r);
}

bool RenderThemeChromiumSkia::paintMediaVolumeSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaVolumeSliderThumb, object, paintInfo, rect);
}

bool RenderThemeChromiumSkia::paintMediaPlayButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaPlayButton, object, paintInfo, rect);
}

bool RenderThemeChromiumSkia::paintMediaMuteButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaMuteButton, object, paintInfo, rect);
}

String RenderThemeChromiumSkia::formatMediaControlsTime(float time) const
{
    return RenderMediaControlsChromium::formatMediaControlsTime(time);
}

String RenderThemeChromiumSkia::formatMediaControlsCurrentTime(float currentTime, float duration) const
{
    return RenderMediaControlsChromium::formatMediaControlsCurrentTime(currentTime, duration);
}

bool RenderThemeChromiumSkia::paintMediaFullscreenButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaEnterFullscreenButton, object, paintInfo, rect);
}

void RenderThemeChromiumSkia::adjustMenuListStyle(StyleResolver*, RenderStyle* style, WebCore::Element*) const
{
    // Height is locked to auto on all browsers.
    style->setLineHeight(RenderStyle::initialLineHeight());
}

void RenderThemeChromiumSkia::adjustMenuListButtonStyle(StyleResolver* styleResolver, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(styleResolver, style, e);
}

// Used to paint styled menulists (i.e. with a non-default border)
bool RenderThemeChromiumSkia::paintMenuListButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintMenuList(o, i, rect);
}

int RenderThemeChromiumSkia::popupInternalPaddingLeft(RenderStyle* style) const
{
    return menuListInternalPadding(style, LeftPadding);
}

int RenderThemeChromiumSkia::popupInternalPaddingRight(RenderStyle* style) const
{
    return menuListInternalPadding(style, RightPadding);
}

int RenderThemeChromiumSkia::popupInternalPaddingTop(RenderStyle* style) const
{
    return menuListInternalPadding(style, TopPadding);
}

int RenderThemeChromiumSkia::popupInternalPaddingBottom(RenderStyle* style) const
{
    return menuListInternalPadding(style, BottomPadding);
}

// static
void RenderThemeChromiumSkia::setDefaultFontSize(int fontSize)
{
    RenderThemeChromiumFontProvider::setDefaultFontSize(fontSize);
}

double RenderThemeChromiumSkia::caretBlinkIntervalInternal() const
{
    return RenderTheme::caretBlinkInterval();
}

int RenderThemeChromiumSkia::menuListArrowPadding() const
{
    return ScrollbarTheme::theme()->scrollbarThickness();
}

// static
void RenderThemeChromiumSkia::setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

int RenderThemeChromiumSkia::menuListInternalPadding(RenderStyle* style, int paddingType) const
{
    // This internal padding is in addition to the user-supplied padding.
    // Matches the FF behavior.
    int padding = styledMenuListInternalPadding[paddingType];

    // Reserve the space for right arrow here. The rest of the padding is
    // set by adjustMenuListStyle, since PopMenuWin.cpp uses the padding from
    // RenderMenuList to lay out the individual items in the popup.
    // If the MenuList actually has appearance "NoAppearance", then that means
    // we don't draw a button, so don't reserve space for it.
    const int barType = style->direction() == LTR ? RightPadding : LeftPadding;
    if (paddingType == barType && style->appearance() != NoControlPart)
        padding += menuListArrowPadding();

    return padding;
}

bool RenderThemeChromiumSkia::shouldShowPlaceholderWhenFocused() const
{
    return true;
}

#if ENABLE(DATALIST_ELEMENT)
LayoutUnit RenderThemeChromiumSkia::sliderTickSnappingThreshold() const
{
    return RenderThemeChromiumCommon::sliderTickSnappingThreshold();
}
#endif

//
// Following values are come from default of GTK+
//
static const int progressDeltaPixelsPerSecond = 100;
static const int progressActivityBlocks = 5;
static const int progressAnimationFrmaes = 10;
static const double progressAnimationInterval = 0.125;

IntRect RenderThemeChromiumSkia::determinateProgressValueRectFor(RenderProgress* renderProgress, const IntRect& rect) const
{
    int dx = rect.width() * renderProgress->position();
    return IntRect(rect.x(), rect.y(), dx, rect.height());
}

IntRect RenderThemeChromiumSkia::indeterminateProgressValueRectFor(RenderProgress* renderProgress, const IntRect& rect) const
{

    int valueWidth = rect.width() / progressActivityBlocks;
    int movableWidth = rect.width() - valueWidth;
    if (movableWidth <= 0)
        return IntRect();

    double progress = renderProgress->animationProgress();
    if (progress < 0.5)
        return IntRect(rect.x() + progress * 2 * movableWidth, rect.y(), valueWidth, rect.height());
    return IntRect(rect.x() + (1.0 - progress) * 2 * movableWidth, rect.y(), valueWidth, rect.height());
}

double RenderThemeChromiumSkia::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    return progressAnimationInterval;
}

double RenderThemeChromiumSkia::animationDurationForProgressBar(RenderProgress* renderProgress) const
{
    return progressAnimationInterval * progressAnimationFrmaes * 2; // "2" for back and forth
}

IntRect RenderThemeChromiumSkia::progressValueRectFor(RenderProgress* renderProgress, const IntRect& rect) const
{
    return renderProgress->isDeterminate() ? determinateProgressValueRectFor(renderProgress, rect) : indeterminateProgressValueRectFor(renderProgress, rect);
}

RenderThemeChromiumSkia::DirectionFlippingScope::DirectionFlippingScope(RenderObject* renderer, const PaintInfo& paintInfo, const IntRect& rect)
    : m_needsFlipping(!renderer->style()->isLeftToRightDirection())
    , m_paintInfo(paintInfo)
{
    if (!m_needsFlipping)
        return;
    m_paintInfo.context->save();
    m_paintInfo.context->translate(2 * rect.x() + rect.width(), 0);
    m_paintInfo.context->scale(FloatSize(-1, 1));
}

RenderThemeChromiumSkia::DirectionFlippingScope::~DirectionFlippingScope()
{
    if (!m_needsFlipping)
        return;
    m_paintInfo.context->restore();
}

} // namespace WebCore
