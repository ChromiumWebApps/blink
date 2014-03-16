/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "core/loader/FormSubmission.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/html/DOMFormData.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "heap/Handle.h"
#include "platform/network/FormData.h"
#include "platform/network/FormDataBuilder.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextEncoding.h"

namespace WebCore {

using namespace HTMLNames;

static int64_t generateFormDataIdentifier()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static int64_t nextIdentifier = static_cast<int64_t>(currentTime() * 1000000.0);
    return ++nextIdentifier;
}

static void appendMailtoPostFormDataToURL(KURL& url, const FormData& data, const String& encodingType)
{
    String body = data.flattenToString();

    if (equalIgnoringCase(encodingType, "text/plain")) {
        // Convention seems to be to decode, and s/&/\r\n/. Also, spaces are encoded as %20.
        body = decodeURLEscapeSequences(body.replaceWithLiteral('&', "\r\n").replace('+', ' ') + "\r\n");
    }

    Vector<char> bodyData;
    bodyData.append("body=", 5);
    FormDataBuilder::encodeStringAsFormData(bodyData, body.utf8());
    body = String(bodyData.data(), bodyData.size()).replaceWithLiteral('+', "%20");

    StringBuilder query;
    query.append(url.query());
    if (!query.isEmpty())
        query.append('&');
    query.append(body);
    url.setQuery(query.toString());
}

void FormSubmission::Attributes::parseAction(const String& action)
{
    // FIXME: Can we parse into a KURL?
    m_action = stripLeadingAndTrailingHTMLSpaces(action);
}

AtomicString FormSubmission::Attributes::parseEncodingType(const String& type)
{
    if (equalIgnoringCase(type, "multipart/form-data"))
        return AtomicString("multipart/form-data", AtomicString::ConstructFromLiteral);
    if (equalIgnoringCase(type, "text/plain"))
        return AtomicString("text/plain", AtomicString::ConstructFromLiteral);
    return AtomicString("application/x-www-form-urlencoded", AtomicString::ConstructFromLiteral);
}

void FormSubmission::Attributes::updateEncodingType(const String& type)
{
    m_encodingType = parseEncodingType(type);
    m_isMultiPartForm = (m_encodingType == "multipart/form-data");
}

FormSubmission::Method FormSubmission::Attributes::parseMethodType(const String& type)
{
    if (equalIgnoringCase(type, "post"))
        return FormSubmission::PostMethod;
    if (RuntimeEnabledFeatures::dialogElementEnabled() && equalIgnoringCase(type, "dialog"))
        return FormSubmission::DialogMethod;
    return FormSubmission::GetMethod;
}

void FormSubmission::Attributes::updateMethodType(const String& type)
{
    m_method = parseMethodType(type);
}

String FormSubmission::Attributes::methodString(Method method)
{
    switch (method) {
    case GetMethod:
        return "get";
    case PostMethod:
        return "post";
    case DialogMethod:
        return "dialog";
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

void FormSubmission::Attributes::copyFrom(const Attributes& other)
{
    m_method = other.m_method;
    m_isMultiPartForm = other.m_isMultiPartForm;

    m_action = other.m_action;
    m_target = other.m_target;
    m_encodingType = other.m_encodingType;
    m_acceptCharset = other.m_acceptCharset;
}

inline FormSubmission::FormSubmission(Method method, const KURL& action, const AtomicString& target, const AtomicString& contentType, PassRefPtr<FormState> state, PassRefPtr<FormData> data, const String& boundary, PassRefPtr<Event> event)
    : m_method(method)
    , m_action(action)
    , m_target(target)
    , m_contentType(contentType)
    , m_formState(state)
    , m_formData(data)
    , m_boundary(boundary)
    , m_event(event)
{
}

inline FormSubmission::FormSubmission(const String& result)
    : m_method(DialogMethod)
    , m_result(result)
{
}

PassRefPtr<FormSubmission> FormSubmission::create(HTMLFormElement* form, const Attributes& attributes, PassRefPtr<Event> event, FormSubmissionTrigger trigger)
{
    ASSERT(form);

    HTMLFormControlElement* submitButton = 0;
    if (event && event->target()) {
        for (Node* node = event->target()->toNode(); node; node = node->parentOrShadowHostNode()) {
            if (node->isElementNode() && toElement(node)->isFormControlElement()) {
                submitButton = toHTMLFormControlElement(node);
                break;
            }
        }
    }

    FormSubmission::Attributes copiedAttributes;
    copiedAttributes.copyFrom(attributes);
    if (submitButton) {
        AtomicString attributeValue;
        if (!(attributeValue = submitButton->fastGetAttribute(formactionAttr)).isNull())
            copiedAttributes.parseAction(attributeValue);
        if (!(attributeValue = submitButton->fastGetAttribute(formenctypeAttr)).isNull())
            copiedAttributes.updateEncodingType(attributeValue);
        if (!(attributeValue = submitButton->fastGetAttribute(formmethodAttr)).isNull())
            copiedAttributes.updateMethodType(attributeValue);
        if (!(attributeValue = submitButton->fastGetAttribute(formtargetAttr)).isNull())
            copiedAttributes.setTarget(attributeValue);
    }

    if (copiedAttributes.method() == DialogMethod) {
        if (submitButton)
            return adoptRef(new FormSubmission(submitButton->resultForDialogSubmit()));
        return adoptRef(new FormSubmission(""));
    }

    Document& document = form->document();
    KURL actionURL = document.completeURL(copiedAttributes.action().isEmpty() ? document.url().string() : copiedAttributes.action());
    bool isMailtoForm = actionURL.protocolIs("mailto");
    bool isMultiPartForm = false;
    AtomicString encodingType = copiedAttributes.encodingType();

    if (copiedAttributes.method() == PostMethod) {
        isMultiPartForm = copiedAttributes.isMultiPartForm();
        if (isMultiPartForm && isMailtoForm) {
            encodingType = AtomicString("application/x-www-form-urlencoded", AtomicString::ConstructFromLiteral);
            isMultiPartForm = false;
        }
    }
    WTF::TextEncoding dataEncoding = isMailtoForm ? UTF8Encoding() : FormDataBuilder::encodingFromAcceptCharset(copiedAttributes.acceptCharset(), document.inputEncoding(), document.defaultCharset());
    RefPtrWillBeRawPtr<DOMFormData> domFormData = DOMFormData::create(dataEncoding.encodingForFormSubmission());

    bool containsPasswordData = false;
    for (unsigned i = 0; i < form->associatedElements().size(); ++i) {
        FormAssociatedElement* control = form->associatedElements()[i];
        ASSERT(control);
        HTMLElement& element = toHTMLElement(*control);
        if (!element.isDisabledFormControl())
            control->appendFormData(*domFormData, isMultiPartForm);
        if (isHTMLInputElement(element)) {
            HTMLInputElement& input = toHTMLInputElement(element);
            if (input.isPasswordField() && !input.value().isEmpty())
                containsPasswordData = true;
        }
    }

    RefPtr<FormData> formData;
    String boundary;

    if (isMultiPartForm) {
        formData = domFormData->createMultiPartFormData(domFormData->encoding());
        boundary = formData->boundary().data();
    } else {
        formData = domFormData->createFormData(domFormData->encoding(), attributes.method() == GetMethod ? FormData::FormURLEncoded : FormData::parseEncodingType(encodingType));
        if (copiedAttributes.method() == PostMethod && isMailtoForm) {
            // Convert the form data into a string that we put into the URL.
            appendMailtoPostFormDataToURL(actionURL, *formData, encodingType);
            formData = FormData::create();
        }
    }

    formData->setIdentifier(generateFormDataIdentifier());
    formData->setContainsPasswordData(containsPasswordData);
    AtomicString targetOrBaseTarget = copiedAttributes.target().isEmpty() ? document.baseTarget() : copiedAttributes.target();
    RefPtr<FormState> formState = FormState::create(*form, trigger);
    return adoptRef(new FormSubmission(copiedAttributes.method(), actionURL, targetOrBaseTarget, encodingType, formState.release(), formData.release(), boundary, event));
}

KURL FormSubmission::requestURL() const
{
    if (m_method == FormSubmission::PostMethod)
        return m_action;

    KURL requestURL(m_action);
    requestURL.setQuery(m_formData->flattenToString());
    return requestURL;
}

void FormSubmission::populateFrameLoadRequest(FrameLoadRequest& frameRequest)
{
    if (!m_target.isEmpty())
        frameRequest.setFrameName(m_target);

    if (!m_referrer.referrer.isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_referrer);

    if (m_method == FormSubmission::PostMethod) {
        frameRequest.resourceRequest().setHTTPMethod("POST");
        frameRequest.resourceRequest().setHTTPBody(m_formData);

        // construct some user headers if necessary
        if (m_boundary.isEmpty())
            frameRequest.resourceRequest().setHTTPContentType(m_contentType);
        else
            frameRequest.resourceRequest().setHTTPContentType(m_contentType + "; boundary=" + m_boundary);
    }

    frameRequest.resourceRequest().setURL(requestURL());
    FrameLoader::addHTTPOriginIfNeeded(frameRequest.resourceRequest(), AtomicString(m_origin));
}

}
