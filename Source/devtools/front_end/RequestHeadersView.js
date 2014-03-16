/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) IBM Corp. 2009  All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @extends {WebInspector.VBox}
 * @param {!WebInspector.NetworkRequest} request
 */
WebInspector.RequestHeadersView = function(request)
{
    WebInspector.VBox.call(this);
    this.registerRequiredCSS("resourceView.css");
    this.element.classList.add("resource-headers-view");

    this._request = request;

    this._headersListElement = document.createElement("ol");
    this._headersListElement.className = "outline-disclosure";
    this.element.appendChild(this._headersListElement);

    this._headersTreeOutline = new TreeOutline(this._headersListElement);
    this._headersTreeOutline.expandTreeElementsWhenArrowing = true;

    this._remoteAddressTreeElement = new TreeElement("", null, false);
    this._remoteAddressTreeElement.selectable = false;
    this._remoteAddressTreeElement.hidden = true;
    this._headersTreeOutline.appendChild(this._remoteAddressTreeElement);

    this._urlTreeElement = new TreeElement("", null, false);
    this._urlTreeElement.selectable = false;
    this._headersTreeOutline.appendChild(this._urlTreeElement);

    this._requestMethodTreeElement = new TreeElement("", null, false);
    this._requestMethodTreeElement.selectable = false;
    this._headersTreeOutline.appendChild(this._requestMethodTreeElement);

    this._statusCodeTreeElement = new TreeElement("", null, false);
    this._statusCodeTreeElement.selectable = false;
    this._headersTreeOutline.appendChild(this._statusCodeTreeElement);

    this._requestHeadersTreeElement = new TreeElement("", null, true);
    this._requestHeadersTreeElement.expanded = true;
    this._requestHeadersTreeElement.selectable = false;
    this._headersTreeOutline.appendChild(this._requestHeadersTreeElement);

    this._decodeRequestParameters = true;

    this._showRequestHeadersText = false;
    this._showResponseHeadersText = false;

    this._queryStringTreeElement = new TreeElement("", null, true);
    this._queryStringTreeElement.expanded = true;
    this._queryStringTreeElement.selectable = false;
    this._queryStringTreeElement.hidden = true;
    this._headersTreeOutline.appendChild(this._queryStringTreeElement);

    this._formDataTreeElement = new TreeElement("", null, true);
    this._formDataTreeElement.expanded = true;
    this._formDataTreeElement.selectable = false;
    this._formDataTreeElement.hidden = true;
    this._headersTreeOutline.appendChild(this._formDataTreeElement);

    this._requestPayloadTreeElement = new TreeElement(WebInspector.UIString("Request Payload"), null, true);
    this._requestPayloadTreeElement.expanded = true;
    this._requestPayloadTreeElement.selectable = false;
    this._requestPayloadTreeElement.hidden = true;
    this._headersTreeOutline.appendChild(this._requestPayloadTreeElement);

    this._responseHeadersTreeElement = new TreeElement("", null, true);
    this._responseHeadersTreeElement.expanded = true;
    this._responseHeadersTreeElement.selectable = false;
    this._headersTreeOutline.appendChild(this._responseHeadersTreeElement);
}

WebInspector.RequestHeadersView.prototype = {

    wasShown: function()
    {
        this._request.addEventListener(WebInspector.NetworkRequest.Events.RemoteAddressChanged, this._refreshRemoteAddress, this);
        this._request.addEventListener(WebInspector.NetworkRequest.Events.RequestHeadersChanged, this._refreshRequestHeaders, this);
        this._request.addEventListener(WebInspector.NetworkRequest.Events.ResponseHeadersChanged, this._refreshResponseHeaders, this);
        this._request.addEventListener(WebInspector.NetworkRequest.Events.FinishedLoading, this._refreshHTTPInformation, this);

        this._refreshURL();
        this._refreshQueryString();
        this._refreshRequestHeaders();
        this._refreshResponseHeaders();
        this._refreshHTTPInformation();
        this._refreshRemoteAddress();
    },

    willHide: function()
    {
        this._request.removeEventListener(WebInspector.NetworkRequest.Events.RemoteAddressChanged, this._refreshRemoteAddress, this);
        this._request.removeEventListener(WebInspector.NetworkRequest.Events.RequestHeadersChanged, this._refreshRequestHeaders, this);
        this._request.removeEventListener(WebInspector.NetworkRequest.Events.ResponseHeadersChanged, this._refreshResponseHeaders, this);
        this._request.removeEventListener(WebInspector.NetworkRequest.Events.FinishedLoading, this._refreshHTTPInformation, this);
    },

    /**
     * @param {string} name
     * @param {string} value
     * @return {!DocumentFragment}
     */
    _formatHeader: function(name, value)
    {
        var fragment = document.createDocumentFragment();
        fragment.createChild("div", "header-name").textContent = name + ":";
        fragment.createChild("div", "header-value source-code").textContent = value;

        return fragment;
    },

    /**
     * @param {string} value
     * @param {string} className
     * @param {boolean} decodeParameters
     * @return {!Element}
     */
    _formatParameter: function(value, className, decodeParameters)
    {
        var errorDecoding = false;

        if (decodeParameters) {
            value = value.replace(/\+/g, " ");
            if (value.indexOf("%") >= 0) {
                try {
                    value = decodeURIComponent(value);
                } catch (e) {
                    errorDecoding = true;
                }
            }
        }
        var div = document.createElement("div");
        div.className = className;
        if (errorDecoding)
            div.createChild("span", "error-message").textContent = WebInspector.UIString("(unable to decode value)");
        else
            div.textContent = value;
        return div;
    },

    _refreshURL: function()
    {
        this._urlTreeElement.title = this._formatHeader(WebInspector.UIString("Request URL"), this._request.url);
    },

    _refreshQueryString: function()
    {
        var queryString = this._request.queryString();
        var queryParameters = this._request.queryParameters;
        this._queryStringTreeElement.hidden = !queryParameters;
        if (queryParameters)
            this._refreshParams(WebInspector.UIString("Query String Parameters"), queryParameters, queryString, this._queryStringTreeElement);
    },

    _refreshFormData: function()
    {
        this._formDataTreeElement.hidden = true;
        this._requestPayloadTreeElement.hidden = true;

        var formData = this._request.requestFormData;
        if (!formData)
            return;

        var formParameters = this._request.formParameters;
        if (formParameters) {
            this._formDataTreeElement.hidden = false;
            this._refreshParams(WebInspector.UIString("Form Data"), formParameters, formData, this._formDataTreeElement);
        } else {
            this._requestPayloadTreeElement.hidden = false;
            try {
                var json = JSON.parse(formData);
                this._refreshRequestJSONPayload(json, formData);
            } catch (e) {
                this._populateTreeElementWithSourceText(this._requestPayloadTreeElement, formData);
            }
        }
    },

    /**
     * @param {!TreeElement} treeElement
     * @param {?string} sourceText
     */
    _populateTreeElementWithSourceText: function(treeElement, sourceText)
    {
        var sourceTextElement = document.createElement("span");
        sourceTextElement.classList.add("header-value");
        sourceTextElement.classList.add("source-code");
        sourceTextElement.textContent = String(sourceText || "").trim();

        var sourceTreeElement = new TreeElement(sourceTextElement);
        sourceTreeElement.selectable = false;
        treeElement.removeChildren();
        treeElement.appendChild(sourceTreeElement);
    },

    /**
     * @param {string} title
     * @param {?Array.<!WebInspector.NetworkRequest.NameValue>} params
     * @param {?string} sourceText
     * @param {!TreeElement} paramsTreeElement
     */
    _refreshParams: function(title, params, sourceText, paramsTreeElement)
    {
        paramsTreeElement.removeChildren();

        paramsTreeElement.listItemElement.removeChildren();
        paramsTreeElement.listItemElement.appendChild(document.createTextNode(title));

        var headerCount = document.createElement("span");
        headerCount.classList.add("header-count");
        headerCount.textContent = WebInspector.UIString(" (%d)", params.length);
        paramsTreeElement.listItemElement.appendChild(headerCount);

        /**
         * @param {?Event} event
         * @this {WebInspector.RequestHeadersView}
         */
        function toggleViewSource(event)
        {
            paramsTreeElement._viewSource = !paramsTreeElement._viewSource;
            this._refreshParams(title, params, sourceText, paramsTreeElement);
        }

        paramsTreeElement.listItemElement.appendChild(this._createViewSourceToggle(paramsTreeElement._viewSource, toggleViewSource.bind(this)));

        if (paramsTreeElement._viewSource) {
            this._populateTreeElementWithSourceText(paramsTreeElement, sourceText);
            return;
        }

        var toggleTitle = this._decodeRequestParameters ? WebInspector.UIString("view URL encoded") : WebInspector.UIString("view decoded");
        var toggleButton = this._createToggleButton(toggleTitle);
        toggleButton.addEventListener("click", this._toggleURLDecoding.bind(this), false);
        paramsTreeElement.listItemElement.appendChild(toggleButton);

        for (var i = 0; i < params.length; ++i) {
            var paramNameValue = document.createDocumentFragment();
            var name = this._formatParameter(params[i].name + ":", "header-name", this._decodeRequestParameters);
            var value = this._formatParameter(params[i].value, "header-value source-code", this._decodeRequestParameters);
            paramNameValue.appendChild(name);
            paramNameValue.appendChild(value);

            var parmTreeElement = new TreeElement(paramNameValue, null, false);
            parmTreeElement.selectable = false;
            paramsTreeElement.appendChild(parmTreeElement);
        }
    },

    /**
     * @param {*} parsedObject
     * @param {string} sourceText
     */
    _refreshRequestJSONPayload: function(parsedObject, sourceText)
    {
        var treeElement = this._requestPayloadTreeElement;
        treeElement.removeChildren();

        var listItem = this._requestPayloadTreeElement.listItemElement;
        listItem.removeChildren();
        listItem.appendChild(document.createTextNode(this._requestPayloadTreeElement.title));

        /**
         * @param {?Event} event
         * @this {WebInspector.RequestHeadersView}
         */
        function toggleViewSource(event)
        {
            treeElement._viewSource = !treeElement._viewSource;
            this._refreshRequestJSONPayload(parsedObject, sourceText);
        }

        listItem.appendChild(this._createViewSourceToggle(treeElement._viewSource, toggleViewSource.bind(this)));
        if (treeElement._viewSource) {
            this._populateTreeElementWithSourceText(this._requestPayloadTreeElement, sourceText);
        } else {
            var object = WebInspector.RemoteObject.fromLocalObject(parsedObject);
            var section = new WebInspector.ObjectPropertiesSection(object, object.description);
            section.expand();
            section.editable = false;
            listItem.appendChild(section.element);
        }
    },

    /**
     * @param {boolean} viewSource
     * @param {function(?Event)} handler
     * @return {!Element}
     */
    _createViewSourceToggle: function(viewSource, handler)
    {
        var viewSourceToggleTitle = viewSource ? WebInspector.UIString("view parsed") : WebInspector.UIString("view source");
        var viewSourceToggleButton = this._createToggleButton(viewSourceToggleTitle);
        viewSourceToggleButton.addEventListener("click", handler, false);
        return viewSourceToggleButton;
    },

    /**
     * @param {?Event} event
     */
    _toggleURLDecoding: function(event)
    {
        this._decodeRequestParameters = !this._decodeRequestParameters;
        this._refreshQueryString();
        this._refreshFormData();
    },

    _refreshRequestHeaders: function()
    {
        var treeElement = this._requestHeadersTreeElement;

        var headers = this._request.requestHeaders();
        headers = headers.slice();
        headers.sort(function(a, b) { return a.name.toLowerCase().compareTo(b.name.toLowerCase()) });
        var headersText = this._request.requestHeadersText();

        if (this._showRequestHeadersText && headersText)
            this._refreshHeadersText(WebInspector.UIString("Request Headers"), headers.length, headersText, treeElement);
        else
            this._refreshHeaders(WebInspector.UIString("Request Headers"), headers, treeElement);

        if (headersText === undefined) {
            var caution = WebInspector.UIString(" CAUTION: Provisional headers are shown.");
            treeElement.listItemElement.createChild("span", "caution").textContent = caution;
        }

        if (headersText) {
            var toggleButton = this._createHeadersToggleButton(this._showRequestHeadersText);
            toggleButton.addEventListener("click", this._toggleRequestHeadersText.bind(this), false);
            treeElement.listItemElement.appendChild(toggleButton);
        }

        this._refreshFormData();
    },

    _refreshResponseHeaders: function()
    {
        var treeElement = this._responseHeadersTreeElement;
        var headers = this._request.sortedResponseHeaders;
        var headersText = this._request.responseHeadersText;

        if (this._showResponseHeadersText)
            this._refreshHeadersText(WebInspector.UIString("Response Headers"), headers.length, headersText, treeElement);
        else
            this._refreshHeaders(WebInspector.UIString("Response Headers"), headers, treeElement);

        if (headersText) {
            var toggleButton = this._createHeadersToggleButton(this._showResponseHeadersText);
            toggleButton.addEventListener("click", this._toggleResponseHeadersText.bind(this), false);
            treeElement.listItemElement.appendChild(toggleButton);
        }
    },

    _refreshHTTPInformation: function()
    {
        var requestMethodElement = this._requestMethodTreeElement;
        requestMethodElement.hidden = !this._request.statusCode;
        var statusCodeElement = this._statusCodeTreeElement;
        statusCodeElement.hidden = !this._request.statusCode;

        if (this._request.statusCode) {
            var statusCodeFragment = document.createDocumentFragment();
            statusCodeFragment.createChild("div", "header-name").textContent = WebInspector.UIString("Status Code") + ":";

            var statusCodeImage = statusCodeFragment.createChild("div", "resource-status-image");
            statusCodeImage.title = this._request.statusCode + " " + this._request.statusText;

            if (this._request.statusCode < 300 || this._request.statusCode === 304)
                statusCodeImage.classList.add("green-ball");
            else if (this._request.statusCode < 400)
                statusCodeImage.classList.add("orange-ball");
            else
                statusCodeImage.classList.add("red-ball");

            requestMethodElement.title = this._formatHeader(WebInspector.UIString("Request Method"), this._request.requestMethod);

            var value = statusCodeFragment.createChild("div", "header-value source-code");
            value.textContent = this._request.statusCode + " " + this._request.statusText;
            if (this._request.cached)
                value.createChild("span", "status-from-cache").textContent = " " + WebInspector.UIString("(from cache)");

            statusCodeElement.title = statusCodeFragment;
        }
    },

    /**
     * @param {string} title
     * @param {!TreeElement} headersTreeElement
     * @param {number} headersLength
     */
    _refreshHeadersTitle: function(title, headersTreeElement, headersLength)
    {
        headersTreeElement.listItemElement.removeChildren();
        headersTreeElement.listItemElement.createTextChild(title);

        var headerCount = WebInspector.UIString(" (%d)", headersLength);
        headersTreeElement.listItemElement.createChild("span", "header-count").textContent = headerCount;
    },

    /**
     * @param {string} title
     * @param {!Array.<!WebInspector.NetworkRequest.NameValue>} headers
     * @param {!TreeElement} headersTreeElement
     */
    _refreshHeaders: function(title, headers, headersTreeElement)
    {
        headersTreeElement.removeChildren();

        var length = headers.length;
        this._refreshHeadersTitle(title, headersTreeElement, length);
        headersTreeElement.hidden = !length;
        for (var i = 0; i < length; ++i) {
            var headerTreeElement = new TreeElement(this._formatHeader(headers[i].name, headers[i].value));
            headerTreeElement.selectable = false;
            headersTreeElement.appendChild(headerTreeElement);
        }
    },

    /**
     * @param {string} title
     * @param {number} count
     * @param {string} headersText
     * @param {!TreeElement} headersTreeElement
     */
    _refreshHeadersText: function(title, count, headersText, headersTreeElement)
    {
        this._populateTreeElementWithSourceText(headersTreeElement, headersText);
        this._refreshHeadersTitle(title, headersTreeElement, count);
    },

    _refreshRemoteAddress: function()
    {
        var remoteAddress = this._request.remoteAddress();
        var treeElement = this._remoteAddressTreeElement;
        treeElement.hidden = !remoteAddress;
        if (remoteAddress)
            treeElement.title = this._formatHeader(WebInspector.UIString("Remote Address"), remoteAddress);
    },

    /**
     * @param {?Event} event
     */
    _toggleRequestHeadersText: function(event)
    {
        this._showRequestHeadersText = !this._showRequestHeadersText;
        this._refreshRequestHeaders();
    },

    /**
     * @param {?Event} event
     */
    _toggleResponseHeadersText: function(event)
    {
        this._showResponseHeadersText = !this._showResponseHeadersText;
        this._refreshResponseHeaders();
    },

    /**
     * @param {string} title
     * @return {!Element}
     */
    _createToggleButton: function(title)
    {
        var button = document.createElement("span");
        button.classList.add("header-toggle");
        button.textContent = title;
        return button;
    },

    /**
     * @param {boolean} isHeadersTextShown
     * @return {!Element}
     */
    _createHeadersToggleButton: function(isHeadersTextShown)
    {
        var toggleTitle = isHeadersTextShown ? WebInspector.UIString("view parsed") : WebInspector.UIString("view source");
        return this._createToggleButton(toggleTitle);
    },

    __proto__: WebInspector.VBox.prototype
}
