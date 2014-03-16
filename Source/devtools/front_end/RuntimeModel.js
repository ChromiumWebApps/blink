/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @param {!WebInspector.Target} target
 */
WebInspector.RuntimeModel = function(target)
{
    target.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameAdded, this._frameAdded, this);
    target.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameNavigated, this._frameNavigated, this);
    target.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, this._frameDetached, this);
    target.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.CachedResourcesLoaded, this._didLoadCachedResources, this);
    this._target = target;
    this._debuggerModel = target.debuggerModel;
    this._agent = target.runtimeAgent();
    this._frameIdToContextList = {};
}

WebInspector.RuntimeModel.Events = {
    FrameExecutionContextListAdded: "FrameExecutionContextListAdded",
    FrameExecutionContextListRemoved: "FrameExecutionContextListRemoved",
}

WebInspector.RuntimeModel.prototype = {
    /**
     * @param {?WebInspector.ExecutionContext} executionContext
     */
    setCurrentExecutionContext: function(executionContext)
    {
        this._currentExecutionContext = executionContext;
    },

    /**
     * @return {?WebInspector.ExecutionContext}
     */
    currentExecutionContext: function()
    {
        return this._currentExecutionContext;
    },

    /**
     * @return {!Array.<!WebInspector.FrameExecutionContextList>}
     */
    contextLists: function()
    {
        return Object.values(this._frameIdToContextList);
    },

    /**
     * @param {!WebInspector.ResourceTreeFrame} frame
     * @return {!WebInspector.FrameExecutionContextList}
     */
    contextListByFrame: function(frame)
    {
        return this._frameIdToContextList[frame.id];
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _frameAdded: function(event)
    {
        var frame = /** @type {!WebInspector.ResourceTreeFrame} */ (event.data);
        var context = new WebInspector.FrameExecutionContextList(frame);
        this._frameIdToContextList[frame.id] = context;
        this.dispatchEventToListeners(WebInspector.RuntimeModel.Events.FrameExecutionContextListAdded, context);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _frameNavigated: function(event)
    {
        var frame = /** @type {!WebInspector.ResourceTreeFrame} */ (event.data);
        var context = this._frameIdToContextList[frame.id];
        if (context)
            context._frameNavigated(frame);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _frameDetached: function(event)
    {
        var frame = /** @type {!WebInspector.ResourceTreeFrame} */ (event.data);
        var context = this._frameIdToContextList[frame.id];
        if (!context)
            return;
        this.dispatchEventToListeners(WebInspector.RuntimeModel.Events.FrameExecutionContextListRemoved, context);
        delete this._frameIdToContextList[frame.id];
    },

    _didLoadCachedResources: function()
    {
        this._target.registerRuntimeDispatcher(new WebInspector.RuntimeDispatcher(this));
        this._agent.enable();
    },

    _executionContextCreated: function(context)
    {
        var contextList = this._frameIdToContextList[context.frameId];
        console.assert(contextList);
        contextList._addExecutionContext(new WebInspector.ExecutionContext(context.id, context.name, context.isPageContext));
    },

    /**
     * @param {string} expression
     * @param {string} objectGroup
     * @param {boolean} includeCommandLineAPI
     * @param {boolean} doNotPauseOnExceptionsAndMuteConsole
     * @param {boolean} returnByValue
     * @param {boolean} generatePreview
     * @param {function(?WebInspector.RemoteObject, boolean, ?RuntimeAgent.RemoteObject=)} callback
     */
    evaluate: function(expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, callback)
    {
        if (this._debuggerModel.selectedCallFrame()) {
            this._debuggerModel.evaluateOnSelectedCallFrame(expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, callback);
            return;
        }

        if (!expression) {
            // There is no expression, so the completion should happen against global properties.
            expression = "this";
        }

        /**
         * @this {WebInspector.RuntimeModel}
         * @param {?Protocol.Error} error
         * @param {!RuntimeAgent.RemoteObject} result
         * @param {boolean=} wasThrown
         */
        function evalCallback(error, result, wasThrown)
        {
            if (error) {
                callback(null, false);
                return;
            }

            if (returnByValue)
                callback(null, !!wasThrown, wasThrown ? null : result);
            else
                callback(WebInspector.RemoteObject.fromPayload(result, this._target), !!wasThrown);
        }
        this._agent.evaluate(expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, this._currentExecutionContext ? this._currentExecutionContext.id : undefined, returnByValue, generatePreview, evalCallback.bind(this));
    },

    /**
     * @param {!Element} proxyElement
     * @param {!Range} wordRange
     * @param {boolean} force
     * @param {function(!Array.<string>, number=)} completionsReadyCallback
     */
    completionsForTextPrompt: function(proxyElement, wordRange, force, completionsReadyCallback)
    {
        // Pass less stop characters to rangeOfWord so the range will be a more complete expression.
        var expressionRange = wordRange.startContainer.rangeOfWord(wordRange.startOffset, " =:[({;,!+-*/&|^<>", proxyElement, "backward");
        var expressionString = expressionRange.toString();
        var prefix = wordRange.toString();
        this._completionsForExpression(expressionString, prefix, force, completionsReadyCallback);
    },

    /**
     * @param {string} expressionString
     * @param {string} prefix
     * @param {boolean} force
     * @param {function(!Array.<string>, number=)} completionsReadyCallback
     */
    _completionsForExpression: function(expressionString, prefix, force, completionsReadyCallback)
    {
        var lastIndex = expressionString.length - 1;

        var dotNotation = (expressionString[lastIndex] === ".");
        var bracketNotation = (expressionString[lastIndex] === "[");

        if (dotNotation || bracketNotation)
            expressionString = expressionString.substr(0, lastIndex);

        if (expressionString && parseInt(expressionString, 10) == expressionString) {
            // User is entering float value, do not suggest anything.
            completionsReadyCallback([]);
            return;
        }

        if (!prefix && !expressionString && !force) {
            completionsReadyCallback([]);
            return;
        }

        if (!expressionString && this._debuggerModel.selectedCallFrame())
            this._debuggerModel.getSelectedCallFrameVariables(receivedPropertyNames.bind(this));
        else
            this.evaluate(expressionString, "completion", true, true, false, false, evaluated.bind(this));

        /**
         * @this {WebInspector.RuntimeModel}
         */
        function evaluated(result, wasThrown)
        {
            if (!result || wasThrown) {
                completionsReadyCallback([]);
                return;
            }

            /**
             * @param {string} primitiveType
             * @this {WebInspector.RuntimeModel}
             */
            function getCompletions(primitiveType)
            {
                var object;
                if (primitiveType === "string")
                    object = new String("");
                else if (primitiveType === "number")
                    object = new Number(0);
                else if (primitiveType === "boolean")
                    object = new Boolean(false);
                else
                    object = this;

                var resultSet = {};
                for (var o = object; o; o = o.__proto__) {
                    try {
                        var names = Object.getOwnPropertyNames(o);
                        for (var i = 0; i < names.length; ++i)
                            resultSet[names[i]] = true;
                    } catch (e) {
                    }
                }
                return resultSet;
            }

            if (result.type === "object" || result.type === "function")
                result.callFunctionJSON(getCompletions, undefined, receivedPropertyNames.bind(this));
            else if (result.type === "string" || result.type === "number" || result.type === "boolean")
                this.evaluate("(" + getCompletions + ")(\"" + result.type + "\")", "completion", false, true, true, false, receivedPropertyNamesFromEval.bind(this));
        }

        /**
         * @param {?WebInspector.RemoteObject} notRelevant
         * @param {boolean} wasThrown
         * @param {?RuntimeAgent.RemoteObject=} result
         * @this {WebInspector.RuntimeModel}
         */
        function receivedPropertyNamesFromEval(notRelevant, wasThrown, result)
        {
            if (result && !wasThrown)
                receivedPropertyNames.call(this, result.value);
            else
                completionsReadyCallback([]);
        }

        /**
         * @this {WebInspector.RuntimeModel}
         */
        function receivedPropertyNames(propertyNames)
        {
            this._agent.releaseObjectGroup("completion");
            if (!propertyNames) {
                completionsReadyCallback([]);
                return;
            }
            var includeCommandLineAPI = (!dotNotation && !bracketNotation);
            if (includeCommandLineAPI) {
                const commandLineAPI = ["dir", "dirxml", "keys", "values", "profile", "profileEnd", "monitorEvents", "unmonitorEvents", "inspect", "copy", "clear",
                    "getEventListeners", "debug", "undebug", "monitor", "unmonitor", "table", "$", "$$", "$x"];
                for (var i = 0; i < commandLineAPI.length; ++i)
                    propertyNames[commandLineAPI[i]] = true;
            }
            this._reportCompletions(completionsReadyCallback, dotNotation, bracketNotation, expressionString, prefix, Object.keys(propertyNames));
        }
    },

    /**
     * @param {function(!Array.<string>, number=)} completionsReadyCallback
     * @param {boolean} dotNotation
     * @param {boolean} bracketNotation
     * @param {string} expressionString
     * @param {string} prefix
     * @param {!Array.<string>} properties
     */
    _reportCompletions: function(completionsReadyCallback, dotNotation, bracketNotation, expressionString, prefix, properties) {
        if (bracketNotation) {
            if (prefix.length && prefix[0] === "'")
                var quoteUsed = "'";
            else
                var quoteUsed = "\"";
        }

        var results = [];

        if (!expressionString) {
            const keywords = ["break", "case", "catch", "continue", "default", "delete", "do", "else", "finally", "for", "function", "if", "in",
                              "instanceof", "new", "return", "switch", "this", "throw", "try", "typeof", "var", "void", "while", "with"];
            properties = properties.concat(keywords);
        }

        properties.sort();

        for (var i = 0; i < properties.length; ++i) {
            var property = properties[i];

            // Assume that all non-ASCII characters are letters and thus can be used as part of identifier.
            if (dotNotation && !/^[a-zA-Z_$\u008F-\uFFFF][a-zA-Z0-9_$\u008F-\uFFFF]*$/.test(property))
                continue;

            if (bracketNotation) {
                if (!/^[0-9]+$/.test(property))
                    property = quoteUsed + property.escapeCharacters(quoteUsed + "\\") + quoteUsed;
                property += "]";
            }

            if (property.length < prefix.length)
                continue;
            if (prefix.length && !property.startsWith(prefix))
                continue;

            results.push(property);
        }
        completionsReadyCallback(results);
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @type {!WebInspector.RuntimeModel}
 */
WebInspector.runtimeModel;

/**
 * @constructor
 * @implements {RuntimeAgent.Dispatcher}
 * @param {!WebInspector.RuntimeModel} runtimeModel
 */
WebInspector.RuntimeDispatcher = function(runtimeModel)
{
    this._runtimeModel = runtimeModel;
}

WebInspector.RuntimeDispatcher.prototype = {
    executionContextCreated: function(context)
    {
        this._runtimeModel._executionContextCreated(context);
    }
}

/**
 * @constructor
 */
WebInspector.ExecutionContext = function(id, name, isPageContext)
{
    this.id = id;
    this.name = (isPageContext && !name) ? "<page context>" : name;
    this.isMainWorldContext = isPageContext;
}

/**
 * @param {!WebInspector.ExecutionContext} a
 * @param {!WebInspector.ExecutionContext} b
 * @return {number}
 */
WebInspector.ExecutionContext.comparator = function(a, b)
{
    // Main world context should always go first.
    if (a.isMainWorldContext)
        return -1;
    if (b.isMainWorldContext)
        return +1;
    return a.name.localeCompare(b.name);
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {!WebInspector.ResourceTreeFrame} frame
 */
WebInspector.FrameExecutionContextList = function(frame)
{
    this._frame = frame;
    this._executionContexts = [];
}

WebInspector.FrameExecutionContextList.EventTypes = {
    ContextsUpdated: "ContextsUpdated",
    ContextAdded: "ContextAdded"
}

WebInspector.FrameExecutionContextList.prototype =
{
    /**
     * @param {!WebInspector.ResourceTreeFrame} frame
     */
    _frameNavigated: function(frame)
    {
        this._frame = frame;
        this._executionContexts = [];
        this.dispatchEventToListeners(WebInspector.FrameExecutionContextList.EventTypes.ContextsUpdated, this);
    },

    /**
     * @param {!WebInspector.ExecutionContext} context
     */
    _addExecutionContext: function(context)
    {
        var insertAt = insertionIndexForObjectInListSortedByFunction(context, this._executionContexts, WebInspector.ExecutionContext.comparator);
        this._executionContexts.splice(insertAt, 0, context);
        this.dispatchEventToListeners(WebInspector.FrameExecutionContextList.EventTypes.ContextAdded, this);
    },

    /**
     * @return {!Array.<!WebInspector.ExecutionContext>}
     */
    executionContexts: function()
    {
        return this._executionContexts;
    },

    /**
     * @return {!WebInspector.ExecutionContext}
     */
    mainWorldContext: function() 
    {
        return this._executionContexts[0];
    },

    /**
     * @param {string} securityOrigin
     * @return {?WebInspector.ExecutionContext}
     */
    contextBySecurityOrigin: function(securityOrigin)
    {
        for (var i = 0; i < this._executionContexts.length; ++i) {
            var context = this._executionContexts[i];
            if (!context.isMainWorldContext && context.name === securityOrigin)
                return context; 
        }
        return null;
    },

    /**
     * @return {string}
     */
    get frameId()
    {
        return this._frame.id;
    },

    /**
     * @return {string}
     */
    get url()
    {
        return this._frame.url;
    },

    /**
     * @return {string}
     */
    get displayName()
    {
        return this._frame.displayName();
    },

    __proto__: WebInspector.Object.prototype
}
