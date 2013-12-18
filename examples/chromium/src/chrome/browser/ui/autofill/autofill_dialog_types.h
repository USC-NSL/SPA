// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/string16.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/browser/field_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"

namespace autofill {

class AutofillField;

// This struct describes a single input control for the imperative autocomplete
// dialog.
struct DetailInput {
  // Multiple DetailInput structs with the same row_id go on the same row. The
  // actual order of the rows is determined by their order of appearance in
  // kBillingInputs.
  int row_id;
  AutofillFieldType type;
  // Placeholder text resource ID.
  int placeholder_text_rid;
  // The section suffix that the field must have to match up to this input.
  const char* section_suffix;
  // A number between 0 and 1.0 that describes how much of the horizontal space
  // in the row should be allotted to this input. 0 is equivalent to 1.
  float expand_weight;
  // When non-empty, indicates the starting value for this input. This will be
  // used when the user is editing existing data.
  string16 initial_value;
};

// Determines whether |input| and |field| match.
typedef base::Callback<bool(const DetailInput& input,
                            const AutofillField& field)>
    InputFieldComparator;

// Sections of the dialog --- all fields that may be shown to the user fit under
// one of these sections.
enum DialogSection {
  // Lower boundary value for looping over all sections.
  SECTION_MIN,

  // The Autofill-backed dialog uses separate CC and billing sections.
  SECTION_CC = SECTION_MIN,
  SECTION_BILLING,
  // The wallet-backed dialog uses a combined CC and billing section.
  SECTION_CC_BILLING,
  SECTION_SHIPPING,
  SECTION_EMAIL,

  // Upper boundary value for looping over all sections.
  SECTION_MAX = SECTION_EMAIL
};

// A notification to show in the autofill dialog. Ranges from information to
// seriously scary security messages, and will give you the color it should be
// displayed (if you ask it).
class DialogNotification {
 public:
  enum Type {
    NONE,
    AUTOCHECKOUT_ERROR,
    EXPLANATORY_MESSAGE,
    REQUIRED_ACTION,
    SECURITY_WARNING,
    VALIDATION_ERROR,
    WALLET_ERROR,
    WALLET_SIGNIN_PROMO,
    WALLET_USAGE_CONFIRMATION,
  };

  DialogNotification();
  DialogNotification(Type type, const string16& display_text);

  // Returns the appropriate background or text color for the view's
  // notification area based on |type_|.
  SkColor GetBackgroundColor() const;
  SkColor GetTextColor() const;

  // Whether this notification has an arrow pointing up at the account chooser.
  bool HasArrow() const;

  // Whether this notifications has the "Save details to wallet" checkbox.
  bool HasCheckbox() const;

  const string16& display_text() const { return display_text_; }
  Type type() const { return type_; }

  void set_checked(bool checked) { checked_ = checked; }
  bool checked() const { return checked_; }

  void set_interactive(bool interactive) { interactive_ = interactive; }
  bool interactive() const { return interactive_; }

 private:
  Type type_;
  string16 display_text_;

  // Whether the dialog notification's checkbox should be checked. Only applies
  // when |HasCheckbox()| is true.
  bool checked_;

  // When false, this disables user interaction with the notification. For
  // example, WALLET_USAGE_CONFIRMATION notifications set this to false after
  // the submit flow has started.
  bool interactive_;
};

enum DialogSignedInState {
  REQUIRES_RESPONSE,
  REQUIRES_SIGN_IN,
  REQUIRES_PASSIVE_SIGN_IN,
  SIGNED_IN,
};

struct SuggestionState {
  SuggestionState(const string16& text,
                  gfx::Font::FontStyle text_style,
                  const gfx::Image& icon,
                  const string16& extra_text,
                  const gfx::Image& extra_icon,
                  bool editable);
  ~SuggestionState();
  string16 text;
  gfx::Font::FontStyle text_style;
  gfx::Image icon;
  string16 extra_text;
  gfx::Image extra_icon;
  bool editable;
};

typedef std::vector<DetailInput> DetailInputs;
typedef std::map<const DetailInput*, string16> DetailOutputMap;

// Returns the AutofillMetrics::DIALOG_UI_*_EDIT_UI_SHOWN metric corresponding
// to the |section|.
AutofillMetrics::DialogUiEvent DialogSectionToUiEditEvent(
    DialogSection section);

// Returns the AutofillMetrics::DIALOG_UI_*_ITEM_ADDED metric corresponding
// to the |section|.
AutofillMetrics::DialogUiEvent DialogSectionToUiItemAddedEvent(
    DialogSection section);

// Returns the AutofillMetrics::DIALOG_UI_*_ITEM_ADDED metric corresponding
// to the |section|.
AutofillMetrics::DialogUiEvent DialogSectionToUiSelectionChangedEvent(
    DialogSection section);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
