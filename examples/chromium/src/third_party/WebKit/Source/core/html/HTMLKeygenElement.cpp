/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "core/html/HTMLKeygenElement.h"

#include "HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Text.h"
#include "core/html/FormDataList.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/platform/SSLKeyGenerator.h"
#include <wtf/StdLibExtras.h>

using namespace WebCore;

namespace WebCore {

using namespace HTMLNames;

class KeygenSelectElement : public HTMLSelectElement {
public:
    static PassRefPtr<KeygenSelectElement> create(Document* document)
    {
        return adoptRef(new KeygenSelectElement(document));
    }

protected:
    KeygenSelectElement(Document* document)
        : HTMLSelectElement(selectTag, document, 0)
    {
        DEFINE_STATIC_LOCAL(AtomicString, pseudoId, ("-webkit-keygen-select", AtomicString::ConstructFromLiteral));
        setPseudo(pseudoId);
    }

private:
    virtual PassRefPtr<Element> cloneElementWithoutAttributesAndChildren()
    {
        return create(document());
    }
};

inline HTMLKeygenElement::HTMLKeygenElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
    : HTMLFormControlElementWithState(tagName, document, form)
{
    ASSERT(hasTagName(keygenTag));
    ScriptWrappable::init(this);

    // Create a select element with one option element for each key size.
    Vector<String> keys;
    getSupportedKeySizes(keys);

    RefPtr<HTMLSelectElement> select = KeygenSelectElement::create(document);
    for (size_t i = 0; i < keys.size(); ++i) {
        RefPtr<HTMLOptionElement> option = HTMLOptionElement::create(document);
        select->appendChild(option, IGNORE_EXCEPTION);
        option->appendChild(Text::create(document, keys[i]), IGNORE_EXCEPTION);
    }

    ensureUserAgentShadowRoot()->appendChild(select, IGNORE_EXCEPTION);
}

PassRefPtr<HTMLKeygenElement> HTMLKeygenElement::create(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
{
    return adoptRef(new HTMLKeygenElement(tagName, document, form));
}

void HTMLKeygenElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    // Reflect disabled attribute on the shadow select element
    if (name == disabledAttr)
        shadowSelect()->setAttribute(name, value);

    HTMLFormControlElement::parseAttribute(name, value);
}

bool HTMLKeygenElement::appendFormData(FormDataList& encoded_values, bool)
{
    // Only RSA is supported at this time.
    const AtomicString& keyType = fastGetAttribute(keytypeAttr);
    if (!keyType.isNull() && !equalIgnoringCase(keyType, "rsa"))
        return false;
    String value = signedPublicKeyAndChallengeString(shadowSelect()->selectedIndex(), fastGetAttribute(challengeAttr), document()->baseURL());
    if (value.isNull())
        return false;
    encoded_values.appendData(name(), value.utf8());
    return true;
}

const AtomicString& HTMLKeygenElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, keygen, ("keygen", AtomicString::ConstructFromLiteral));
    return keygen;
}

void HTMLKeygenElement::reset()
{
    static_cast<HTMLFormControlElement*>(shadowSelect())->reset();
}

bool HTMLKeygenElement::shouldSaveAndRestoreFormControlState() const
{
    return false;
}

HTMLSelectElement* HTMLKeygenElement::shadowSelect() const
{
    ShadowRoot* root = userAgentShadowRoot();
    return root ? toHTMLSelectElement(root->firstChild()) : 0;
}

} // namespace
