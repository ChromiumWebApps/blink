/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * @extends {WebInspector.Object}
 * @implements {WebInspector.ContentProvider}
 * @param {!WebInspector.Project} project
 * @param {string} parentPath
 * @param {string} name
 * @param {string} url
 * @param {!WebInspector.ResourceType} contentType
 * @param {boolean} isEditable
 */
WebInspector.UISourceCode = function(project, parentPath, name, originURL, url, contentType, isEditable)
{
    this._project = project;
    this._parentPath = parentPath;
    this._name = name;
    this._originURL = originURL;
    this._url = url;
    this._contentType = contentType;
    this._isEditable = isEditable;
    /** @type {!Array.<function(?string)>} */
    this._requestContentCallbacks = [];
    /** @type {!Array.<!WebInspector.PresentationConsoleMessage>} */
    this._consoleMessages = [];
    
    /** @type {!Array.<!WebInspector.Revision>} */
    this.history = [];
    if (this.isEditable() && this._url)
        this._restoreRevisionHistory();
}

WebInspector.UISourceCode.Events = {
    WorkingCopyChanged: "WorkingCopyChanged",
    WorkingCopyCommitted: "WorkingCopyCommitted",
    TitleChanged: "TitleChanged",
    SavedStateUpdated: "SavedStateUpdated",
    ConsoleMessageAdded: "ConsoleMessageAdded",
    ConsoleMessageRemoved: "ConsoleMessageRemoved",
    ConsoleMessagesCleared: "ConsoleMessagesCleared",
    SourceMappingChanged: "SourceMappingChanged",
}

WebInspector.UISourceCode.prototype = {
    /**
     * @return {string}
     */
    get url()
    {
        return this._url;
    },

    /**
     * @return {string}
     */
    name: function()
    {
        return this._name;
    },

    /**
     * @return {string}
     */
    parentPath: function()
    {
        return this._parentPath;
    },

    /**
     * @return {string}
     */
    path: function()
    {
        return this._parentPath ? this._parentPath + "/" + this._name : this._name;
    },

    /**
     * @return {string}
     */
    fullDisplayName: function()
    {
        return this._project.displayName() + "/" + (this._parentPath ? this._parentPath + "/" : "") + this.displayName(true);
    },

    /**
     * @param {boolean=} skipTrim
     * @return {string}
     */
    displayName: function(skipTrim)
    {
        var displayName = this.name() || WebInspector.UIString("(index)");
        return skipTrim ? displayName : displayName.trimEnd(100);
    },

    /**
     * @return {string}
     */
    uri: function()
    {
        var path = this.path();
        if (!this._project.id())
            return path;
        if (!path)
            return this._project.id();
        return this._project.id() + "/" + path;
    },

    /**
     * @return {string}
     */
    originURL: function()
    {
        return this._originURL;
    },

    /**
     * @return {boolean}
     */
    canRename: function()
    {
        return this._project.canRename();
    },

    /**
     * @param {string} newName
     * @param {function(boolean)} callback
     */
    rename: function(newName, callback)
    {
        this._project.rename(this, newName, innerCallback.bind(this));

        /**
         * @param {boolean} success
         * @param {string=} newName
         * @param {string=} newURL
         * @param {string=} newOriginURL
         * @param {!WebInspector.ResourceType=} newContentType
         * @this {WebInspector.UISourceCode}
         */
        function innerCallback(success, newName, newURL, newOriginURL, newContentType)
        {
            if (success)
                this._updateName(/** @type {string} */ (newName), /** @type {string} */ (newURL), /** @type {string} */ (newOriginURL), /** @type {!WebInspector.ResourceType} */ (newContentType));
            callback(success);
        }
    },

    remove: function()
    {
        this._project.deleteFile(this.path());
    },

    /**
     * @param {string} name
     * @param {string} url
     * @param {string} originURL
     * @param {!WebInspector.ResourceType=} contentType
     */
    _updateName: function(name, url, originURL, contentType)
    {
        var oldURI = this.uri();
        this._name = name;
        if (url)
            this._url = url;
        if (originURL)
            this._originURL = originURL;
        if (contentType)
            this._contentType = contentType;
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.TitleChanged, oldURI);
    },

    /**
     * @return {string}
     */
    contentURL: function()
    {
        return this.originURL();
    },

    /**
     * @return {!WebInspector.ResourceType}
     */
    contentType: function()
    {
        return this._contentType;
    },

    /**
     * @return {?WebInspector.ScriptFile}
     */
    scriptFile: function()
    {
        return this._scriptFile;
    },

    /**
     * @param {?WebInspector.ScriptFile} scriptFile
     */
    setScriptFile: function(scriptFile)
    {
        this._scriptFile = scriptFile;
    },

    /**
     * @return {!WebInspector.Project}
     */
    project: function()
    {
        return this._project;
    },

    /**
     * @param {function(?Date, ?number)} callback
     */
    requestMetadata: function(callback)
    {
        this._project.requestMetadata(this, callback);
    },

    /**
     * @param {function(?string)} callback
     */
    requestContent: function(callback)
    {
        if (this._content || this._contentLoaded) {
            callback(this._content);
            return;
        }
        this._requestContentCallbacks.push(callback);
        if (this._requestContentCallbacks.length === 1)
            this._project.requestFileContent(this, this._fireContentAvailable.bind(this));
    },

    /**
     * @param {function()=} callback
     */
    checkContentUpdated: function(callback)
    {
        if (!this._project.canSetFileContent())
            return;
        if (this._checkingContent)
            return;
        this._checkingContent = true;
        this._project.requestFileContent(this, contentLoaded.bind(this));

        /**
         * @param {?string} updatedContent
         * @this {WebInspector.UISourceCode}
         */
        function contentLoaded(updatedContent)
        {
            if (updatedContent === null) {
                var workingCopy = this.workingCopy();
                this._commitContent("", false);
                this.setWorkingCopy(workingCopy);
                delete this._checkingContent;
                if (callback)
                    callback();
                return;
            }
            if (typeof this._lastAcceptedContent === "string" && this._lastAcceptedContent === updatedContent) {
                delete this._checkingContent;
                if (callback)
                    callback();
                return;
            }
            if (this._content === updatedContent) {
                delete this._lastAcceptedContent;
                delete this._checkingContent;
                if (callback)
                    callback();
                return;
            }

            if (!this.isDirty()) {
                this._commitContent(updatedContent, false);
                delete this._checkingContent;
                if (callback)
                    callback();
                return;
            }

            var shouldUpdate = window.confirm(WebInspector.UIString("This file was changed externally. Would you like to reload it?"));
            if (shouldUpdate)
                this._commitContent(updatedContent, false);
            else
                this._lastAcceptedContent = updatedContent;
            delete this._checkingContent;
            if (callback)
                callback();
        }
    },

    /**
     * @param {function(?string)} callback
     */
    requestOriginalContent: function(callback)
    {
        this._project.requestFileContent(this, callback);
    },

    /**
     * @param {string} content
     * @param {boolean} shouldSetContentInProject
     */
    _commitContent: function(content, shouldSetContentInProject)
    {
        delete this._lastAcceptedContent;
        this._content = content;
        this._contentLoaded = true;

        var lastRevision = this.history.length ? this.history[this.history.length - 1] : null;
        if (!lastRevision || lastRevision._content !== this._content) {
            var revision = new WebInspector.Revision(this, this._content, new Date());
            this.history.push(revision);
            revision._persist();
        }

        this._innerResetWorkingCopy();
        this._hasCommittedChanges = true;
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.WorkingCopyCommitted);
        if (this._url && WebInspector.fileManager.isURLSaved(this._url))
            this._saveURLWithFileManager(false, this._content);
        if (shouldSetContentInProject)
            this._project.setFileContent(this, this._content, function() { });
    },

    /**
     * @param {boolean} forceSaveAs
     * @param {?string} content
     */
    _saveURLWithFileManager: function(forceSaveAs, content)
    {
        WebInspector.fileManager.save(this._url, /** @type {string} */ (content), forceSaveAs, callback.bind(this));
        WebInspector.fileManager.close(this._url);

        /**
         * @param {boolean} accepted
         * @this {WebInspector.UISourceCode}
         */
        function callback(accepted)
        {
            if (!accepted)
                return;
            this._savedWithFileManager = true;
            this.dispatchEventToListeners(WebInspector.UISourceCode.Events.SavedStateUpdated);
        }
    },

    /**
     * @param {boolean} forceSaveAs
     */
    saveToFileSystem: function(forceSaveAs)
    {
        if (this.isDirty()) {
            this._saveURLWithFileManager(forceSaveAs, this.workingCopy());
            this.commitWorkingCopy(function() { });
            return;
        }
        this.requestContent(this._saveURLWithFileManager.bind(this, forceSaveAs));
    },

    /**
     * @return {boolean}
     */
    hasUnsavedCommittedChanges: function()
    {
        if (this._savedWithFileManager || this.project().canSetFileContent() || !this._isEditable)
            return false;
        if (this._project.workspace().hasResourceContentTrackingExtensions())
            return false;
        return !!this._hasCommittedChanges;
    },

    /**
     * @param {string} content
     */
    addRevision: function(content)
    {
        this._commitContent(content, true);
    },

    _restoreRevisionHistory: function()
    {
        if (!window.localStorage)
            return;

        var registry = WebInspector.Revision._revisionHistoryRegistry();
        var historyItems = registry[this.url];
        if (!historyItems)
            return;

        function filterOutStale(historyItem)
        {
            // FIXME: Main frame might not have been loaded yet when uiSourceCodes for snippets are created.
            if (!WebInspector.resourceTreeModel.mainFrame)
                return false;
            return historyItem.loaderId === WebInspector.resourceTreeModel.mainFrame.loaderId;
        }

        historyItems = historyItems.filter(filterOutStale);
        if (!historyItems.length)
            return;

        for (var i = 0; i < historyItems.length; ++i) {
            var content = window.localStorage[historyItems[i].key];
            var timestamp = new Date(historyItems[i].timestamp);
            var revision = new WebInspector.Revision(this, content, timestamp);
            this.history.push(revision);
        }
        this._content = this.history[this.history.length - 1].content;
        this._hasCommittedChanges = true;
        this._contentLoaded = true;
    },

    _clearRevisionHistory: function()
    {
        if (!window.localStorage)
            return;

        var registry = WebInspector.Revision._revisionHistoryRegistry();
        var historyItems = registry[this.url];
        for (var i = 0; historyItems && i < historyItems.length; ++i)
            delete window.localStorage[historyItems[i].key];
        delete registry[this.url];
        window.localStorage["revision-history"] = JSON.stringify(registry);
    },
   
    revertToOriginal: function()
    {
        /**
         * @this {WebInspector.UISourceCode}
         * @param {?string} content
         */
        function callback(content)
        {
            if (typeof content !== "string")
                return;

            this.addRevision(content);
        }

        this.requestOriginalContent(callback.bind(this));

        WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction, {
            action: WebInspector.UserMetrics.UserActionNames.ApplyOriginalContent,
            url: this.url
        });
    },

    /**
     * @param {function(!WebInspector.UISourceCode)} callback
     */
    revertAndClearHistory: function(callback)
    {
        /**
         * @this {WebInspector.UISourceCode}
         * @param {?string} content
         */
        function revert(content)
        {
            if (typeof content !== "string")
                return;

            this.addRevision(content);
            this._clearRevisionHistory();
            this.history = [];
            callback(this);
        }

        this.requestOriginalContent(revert.bind(this));

        WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction, {
            action: WebInspector.UserMetrics.UserActionNames.RevertRevision,
            url: this.url
        });
    },

    /**
     * @return {boolean}
     */
    isEditable: function()
    {
        return this._isEditable;
    },

    /**
     * @return {string}
     */
    workingCopy: function()
    {
        if (this._workingCopyGetter) {
            this._workingCopy = this._workingCopyGetter();
            delete this._workingCopyGetter;
        }
        if (this.isDirty())
            return this._workingCopy;
        return this._content;
    },

    resetWorkingCopy: function()
    {
        this._innerResetWorkingCopy();
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.WorkingCopyChanged);
    },

    _innerResetWorkingCopy: function()
    {
        delete this._workingCopy;
        delete this._workingCopyGetter;
    },

    /**
     * @param {string} newWorkingCopy
     */
    setWorkingCopy: function(newWorkingCopy)
    {
        this._workingCopy = newWorkingCopy;
        delete this._workingCopyGetter;
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.WorkingCopyChanged);
    },

    setWorkingCopyGetter: function(workingCopyGetter)
    {
        this._workingCopyGetter = workingCopyGetter;
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.WorkingCopyChanged);
    },

    removeWorkingCopyGetter: function()
    {
        if (!this._workingCopyGetter)
            return;
        this._workingCopy = this._workingCopyGetter();
        delete this._workingCopyGetter;
    },

    /**
     * @param {function(?string)} callback
     */
    commitWorkingCopy: function(callback)
    {
        if (!this.isDirty()) {
            callback(null);
            return;
        }

        this._commitContent(this.workingCopy(), true);
        callback(null);

        WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction, {
            action: WebInspector.UserMetrics.UserActionNames.FileSaved,
            url: this.url
        });
    },

    /**
     * @return {boolean}
     */
    isDirty: function()
    {
        return typeof this._workingCopy !== "undefined" || typeof this._workingCopyGetter !== "undefined";
    },

    /**
     * @return {string}
     */
    highlighterType: function()
    {
        var lastIndexOfDot = this._name.lastIndexOf(".");
        var extension = lastIndexOfDot !== -1 ? this._name.substr(lastIndexOfDot + 1) : "";
        var indexOfQuestionMark = extension.indexOf("?");
        if (indexOfQuestionMark !== -1)
            extension = extension.substr(0, indexOfQuestionMark);
        var mimeType = WebInspector.ResourceType.mimeTypesForExtensions[extension.toLowerCase()];
        return mimeType || this.contentType().canonicalMimeType();
    },

    /**
     * @return {?string}
     */
    content: function()
    {
        return this._content;
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(!Array.<!WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        var content = this.content();
        if (content) {
            var provider = new WebInspector.StaticContentProvider(this.contentType(), content);
            provider.searchInContent(query, caseSensitive, isRegex, callback);
            return;
        }

        this._project.searchInFileContent(this, query, caseSensitive, isRegex, callback);
    },

    /**
     * @param {?string} content
     */
    _fireContentAvailable: function(content)
    {
        this._contentLoaded = true;
        this._content = content;

        var callbacks = this._requestContentCallbacks.slice();
        this._requestContentCallbacks = [];
        for (var i = 0; i < callbacks.length; ++i)
            callbacks[i](content);
    },

    /**
     * @return {boolean}
     */
    contentLoaded: function()
    {
        return this._contentLoaded;
    },

    /**
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {?WebInspector.RawLocation}
     */
    uiLocationToRawLocation: function(lineNumber, columnNumber)
    {
        if (!this._sourceMapping)
            return null;
        return this._sourceMapping.uiLocationToRawLocation(this, lineNumber, columnNumber);
    },

    /**
     * @return {!Array.<!WebInspector.PresentationConsoleMessage>}
     */
    consoleMessages: function()
    {
        return this._consoleMessages;
    },

    /**
     * @param {!WebInspector.PresentationConsoleMessage} message
     */
    consoleMessageAdded: function(message)
    {
        this._consoleMessages.push(message);
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessageAdded, message);
    },

    /**
     * @param {!WebInspector.PresentationConsoleMessage} message
     */
    consoleMessageRemoved: function(message)
    {
        this._consoleMessages.remove(message);
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessageRemoved, message);
    },

    consoleMessagesCleared: function()
    {
        this._consoleMessages = [];
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessagesCleared);
    },

    /**
     * @return {boolean}
     */
    hasSourceMapping: function()
    {
        return !!this._sourceMapping;
    },

    /**
     * @param {?WebInspector.SourceMapping} sourceMapping
     */
    setSourceMapping: function(sourceMapping)
    {
        if (this._sourceMapping === sourceMapping)
            return;
        this._sourceMapping = sourceMapping;
        var data = {};
        data.isIdentity = this._sourceMapping && this._sourceMapping.isIdentity();
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.SourceMappingChanged, data);
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @param {!WebInspector.UISourceCode} uiSourceCode
 * @param {number} lineNumber
 * @param {number} columnNumber
 */
WebInspector.UILocation = function(uiSourceCode, lineNumber, columnNumber)
{
    this.uiSourceCode = uiSourceCode;
    this.lineNumber = lineNumber;
    this.columnNumber = columnNumber;
}

WebInspector.UILocation.prototype = {
    /**
     * @return {?WebInspector.RawLocation}
     */
    uiLocationToRawLocation: function()
    {
        return this.uiSourceCode.uiLocationToRawLocation(this.lineNumber, this.columnNumber);
    },

    /**
     * @return {?string}
     */
    url: function()
    {
        return this.uiSourceCode.contentURL();
    },

    /**
     * @return {string}
     */
    linkText: function()
    {
        var linkText = this.uiSourceCode.displayName();
        if (typeof this.lineNumber === "number")
            linkText += ":" + (this.lineNumber + 1);
        return linkText;
    }
}

/**
 * @interface
 */
WebInspector.RawLocation = function()
{
}

/**
 * @constructor
 * @param {!WebInspector.RawLocation} rawLocation
 * @param {function(!WebInspector.UILocation):(boolean|undefined)} updateDelegate
 */
WebInspector.LiveLocation = function(rawLocation, updateDelegate)
{
    this._rawLocation = rawLocation;
    this._updateDelegate = updateDelegate;
}

WebInspector.LiveLocation.prototype = {
    update: function()
    {
        var uiLocation = this.uiLocation();
        if (!uiLocation)
            return;
        if (this._updateDelegate(uiLocation))
            this.dispose();
    },

    /**
     * @return {!WebInspector.RawLocation}
     */
    rawLocation: function()
    {
        return this._rawLocation;
    },

    /**
     * @return {!WebInspector.UILocation}
     */
    uiLocation: function()
    {
        throw "Not implemented";
    },

    dispose: function()
    {
        // Overridden by subclasses.
    }
}

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 * @param {!WebInspector.UISourceCode} uiSourceCode
 * @param {?string|undefined} content
 * @param {!Date} timestamp
 */
WebInspector.Revision = function(uiSourceCode, content, timestamp)
{
    this._uiSourceCode = uiSourceCode;
    this._content = content;
    this._timestamp = timestamp;
}

WebInspector.Revision._revisionHistoryRegistry = function()
{
    if (!WebInspector.Revision._revisionHistoryRegistryObject) {
        if (window.localStorage) {
            var revisionHistory = window.localStorage["revision-history"];
            try {
                WebInspector.Revision._revisionHistoryRegistryObject = revisionHistory ? JSON.parse(revisionHistory) : {};
            } catch (e) {
                WebInspector.Revision._revisionHistoryRegistryObject = {};
            }
        } else
            WebInspector.Revision._revisionHistoryRegistryObject = {};
    }
    return WebInspector.Revision._revisionHistoryRegistryObject;
}

WebInspector.Revision.filterOutStaleRevisions = function()
{
    if (!window.localStorage)
        return;

    var registry = WebInspector.Revision._revisionHistoryRegistry();
    var filteredRegistry = {};
    for (var url in registry) {
        var historyItems = registry[url];
        var filteredHistoryItems = [];
        for (var i = 0; historyItems && i < historyItems.length; ++i) {
            var historyItem = historyItems[i];
            if (historyItem.loaderId === WebInspector.resourceTreeModel.mainFrame.loaderId) {
                filteredHistoryItems.push(historyItem);
                filteredRegistry[url] = filteredHistoryItems;
            } else
                delete window.localStorage[historyItem.key];
        }
    }
    WebInspector.Revision._revisionHistoryRegistryObject = filteredRegistry;

    function persist()
    {
        window.localStorage["revision-history"] = JSON.stringify(filteredRegistry);
    }

    // Schedule async storage.
    setTimeout(persist, 0);
}

WebInspector.Revision.prototype = {
    /**
     * @return {!WebInspector.UISourceCode}
     */
    get uiSourceCode()
    {
        return this._uiSourceCode;
    },

    /**
     * @return {!Date}
     */
    get timestamp()
    {
        return this._timestamp;
    },

    /**
     * @return {?string}
     */
    get content()
    {
        return this._content || null;
    },

    revertToThis: function()
    {
        /**
         * @param {string} content
         * @this {WebInspector.Revision}
         */
        function revert(content)
        {
            if (this._uiSourceCode._content !== content)
                this._uiSourceCode.addRevision(content);
        }
        this.requestContent(revert.bind(this));
    },

    /**
     * @return {string}
     */
    contentURL: function()
    {
        return this._uiSourceCode.originURL();
    },

    /**
     * @return {!WebInspector.ResourceType}
     */
    contentType: function()
    {
        return this._uiSourceCode.contentType();
    },

    /**
     * @param {function(string)} callback
     */
    requestContent: function(callback)
    {
        callback(this._content || "");
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(!Array.<!WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        callback([]);
    },

    _persist: function()
    {
        if (this._uiSourceCode.project().type() === WebInspector.projectTypes.FileSystem)
            return;

        if (!window.localStorage)
            return;

        var url = this.contentURL();
        if (!url || url.startsWith("inspector://"))
            return;

        var loaderId = WebInspector.resourceTreeModel.mainFrame.loaderId;
        var timestamp = this.timestamp.getTime();
        var key = "revision-history|" + url + "|" + loaderId + "|" + timestamp;

        var registry = WebInspector.Revision._revisionHistoryRegistry();

        var historyItems = registry[url];
        if (!historyItems) {
            historyItems = [];
            registry[url] = historyItems;
        }
        historyItems.push({url: url, loaderId: loaderId, timestamp: timestamp, key: key});

        /**
         * @this {WebInspector.Revision}
         */
        function persist()
        {
            window.localStorage[key] = this._content;
            window.localStorage["revision-history"] = JSON.stringify(registry);
        }

        // Schedule async storage.
        setTimeout(persist.bind(this), 0);
    }
}
