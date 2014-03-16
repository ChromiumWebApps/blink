/*
 * Copyright (C) 2009 Joseph Pecoraro
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

importScript("ConsoleViewMessage.js");
importScript("ConsoleView.js");

/**
 * @constructor
 * @extends {WebInspector.Panel}
 */
WebInspector.ConsolePanel = function()
{
    WebInspector.Panel.call(this, "console");
    this._view = WebInspector.ConsolePanel._view();
}

/**
 * @return {!WebInspector.ConsoleView}
 */
WebInspector.ConsolePanel._view = function()
{
    if (!WebInspector.ConsolePanel._consoleView) {
        WebInspector.ConsolePanel._consoleView = new WebInspector.ConsoleView(!Capabilities.isMainFrontend);
        WebInspector.console.setUIDelegate(WebInspector.ConsolePanel._consoleView);
    }
    return WebInspector.ConsolePanel._consoleView;
}

WebInspector.ConsolePanel.prototype = {
    /**
     * @return {!Element}
     */
    defaultFocusedElement: function()
    {
        return this._view.defaultFocusedElement();
    },

    wasShown: function()
    {
        WebInspector.Panel.prototype.wasShown.call(this);
        this._view.show(this.element);
    },

    willHide: function()
    {
        WebInspector.Panel.prototype.willHide.call(this);
        if (WebInspector.ConsolePanel.WrapperView._instance)
            WebInspector.ConsolePanel.WrapperView._instance._showViewInWrapper();
    },

    __proto__: WebInspector.Panel.prototype
}

/**
 * @constructor
 * @implements {WebInspector.Drawer.ViewFactory}
 */
WebInspector.ConsolePanel.ViewFactory = function()
{
}

WebInspector.ConsolePanel.ViewFactory.prototype = {
    /**
     * @return {!WebInspector.View}
     */
    createView: function()
    {
        if (!WebInspector.ConsolePanel.WrapperView._instance)
            WebInspector.ConsolePanel.WrapperView._instance = new WebInspector.ConsolePanel.WrapperView();
        return WebInspector.ConsolePanel.WrapperView._instance;
    }
}

/**
 * @constructor
 * @extends {WebInspector.VBox}
 */
WebInspector.ConsolePanel.WrapperView = function()
{
    WebInspector.VBox.call(this);
    this.element.classList.add("console-view-wrapper");

    this._view = WebInspector.ConsolePanel._view();
    // FIXME: this won't be needed once drawer becomes a view.
    this.wasShown();
}

WebInspector.ConsolePanel.WrapperView.prototype = {
    wasShown: function()
    {
        if (!WebInspector.inspectorView.currentPanel() || WebInspector.inspectorView.currentPanel().name !== "console")
            this._showViewInWrapper();
    },

    /**
     * @return {!Element}
     */
    defaultFocusedElement: function()
    {
        return this._view.defaultFocusedElement();
    },

    focus: function()
    {
        this._view.focus();
    },

    _showViewInWrapper: function()
    {
        this._view.show(this.element);
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @constructor
 * @implements {WebInspector.Revealer}
 */
WebInspector.ConsolePanel.ConsoleRevealer = function()
{
}

WebInspector.ConsolePanel.ConsoleRevealer.prototype = {
    /**
     * @param {!Object} object
     */
    reveal: function(object)
    {
        if (!(object instanceof WebInspector.ConsoleModel))
            return;

        var consoleView = WebInspector.ConsolePanel._view();
        if (consoleView.isShowing()) {
            consoleView.focus();
            return;
        }
        WebInspector.inspectorView.showViewInDrawer("console");
    }
}
