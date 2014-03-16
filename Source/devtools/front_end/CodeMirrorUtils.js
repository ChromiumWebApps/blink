/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @extends {WebInspector.InplaceEditor}
 */
WebInspector.CodeMirrorUtils = function()
{
    WebInspector.InplaceEditor.call(this);
}

WebInspector.CodeMirrorUtils.prototype = {
    /**
     * @return {string}
     */
    editorContent: function(editingContext) {
        return editingContext.codeMirror.getValue();
    },

    /**
     * @param {?Event} e
     */
    _consumeCopy: function(e)
    {
        e.consume();
    },

    setUpEditor: function(editingContext)
    {
        var element = editingContext.element;
        var config = editingContext.config;
        loadScript("CodeMirrorTextEditor.js");
        editingContext.cssLoadView = new WebInspector.CodeMirrorCSSLoadView();
        editingContext.cssLoadView.show(element);
        WebInspector.setCurrentFocusElement(element);
        element.addEventListener("copy", this._consumeCopy, false);
        var codeMirror = window.CodeMirror(element, {
            mode: config.mode,
            lineWrapping: config.lineWrapping,
            smartIndent: config.smartIndent,
            autofocus: true,
            theme: config.theme,
            value: config.initialValue
        });
        codeMirror.getWrapperElement().classList.add("source-code");
        codeMirror.on("cursorActivity", function(cm) {
            cm.display.cursor.scrollIntoViewIfNeeded(false);
        });
        editingContext.codeMirror = codeMirror;
    },

    closeEditor: function(editingContext)
    {
        editingContext.element.removeEventListener("copy", this._consumeCopy, false);
        editingContext.cssLoadView.detach();
    },

    cancelEditing: function(editingContext)
    {
        editingContext.codeMirror.setValue(editingContext.oldText);
    },

    augmentEditingHandle: function(editingContext, handle)
    {
        function setWidth(editingContext, width)
        {
            var padding = 30;
            var codeMirror = editingContext.codeMirror;
            codeMirror.getWrapperElement().style.width = (width - codeMirror.getWrapperElement().offsetLeft - padding) + "px";
            codeMirror.refresh();
        }

        handle.codeMirror = editingContext.codeMirror;
        handle.setWidth = setWidth.bind(null, editingContext);
    },

    __proto__: WebInspector.InplaceEditor.prototype
}

/**
 * @constructor
 * @implements {WebInspector.TokenizerFactory}
 */
WebInspector.CodeMirrorUtils.TokenizerFactory = function() { }

WebInspector.CodeMirrorUtils.TokenizerFactory.prototype = {
    /**
     * @param {string} mimeType
     * @return {function(string, function(string, ?string, number, number))}
     */
    createTokenizer: function(mimeType)
    {
        var mode = CodeMirror.getMode({indentUnit: 2}, mimeType);
        var state = CodeMirror.startState(mode);
        function tokenize(line, callback)
        {
            var stream = new CodeMirror.StringStream(line);
            while (!stream.eol()) {
                var style = mode.token(stream, state);
                var value = stream.current();
                callback(value, style, stream.start, stream.start + value.length);
                stream.start = stream.pos;
            }
        }
        return tokenize;
    }
}

/**
 * This bogus view is needed to load/unload CodeMirror-related CSS on demand.
 *
 * @constructor
 * @extends {WebInspector.VBox}
 */
WebInspector.CodeMirrorCSSLoadView = function()
{
    WebInspector.VBox.call(this);
    this.element.classList.add("hidden");
    this.registerRequiredCSS("cm/codemirror.css");
    this.registerRequiredCSS("cm/cmdevtools.css");
}

WebInspector.CodeMirrorCSSLoadView.prototype = {
    __proto__: WebInspector.VBox.prototype
}
