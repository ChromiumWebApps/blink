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
 */
function InspectorBackendClass()
{
    this._connection = null;
    this._agentPrototypes = {};
    this._dispatcherPrototypes = {};
    this._initialized = false;
    this._enums = {};
    this._initProtocolAgentsConstructor();
}

InspectorBackendClass.prototype = {

    _initProtocolAgentsConstructor: function()
    {
        window.Protocol = {};

        /**
         * @constructor
         * @param {!Object.<string, !Object>} agentsMap
         */
        window.Protocol.Agents = function(agentsMap) {
            this._agentsMap = agentsMap;
        };
    },

    /**
     * @param {string} domain
     */
    _addAgentGetterMethodToProtocolAgentsPrototype: function(domain)
    {
        var upperCaseLength = 0;
        while (upperCaseLength < domain.length && domain[upperCaseLength].toLowerCase() !== domain[upperCaseLength])
            ++upperCaseLength;

        var methodName = domain.substr(0, upperCaseLength).toLowerCase() + domain.slice(upperCaseLength) + "Agent";

        /**
         * @this {Protocol.Agents}
         */
        function agentGetter()
        {
            return this._agentsMap[domain];
        }

        window.Protocol.Agents.prototype[methodName] = agentGetter;

        /**
         * @this {Protocol.Agents}
         */
        function registerDispatcher(dispatcher)
        {
            this.registerDispatcher(domain, dispatcher)
        }

        window.Protocol.Agents.prototype["register" + domain + "Dispatcher"] = registerDispatcher;
    },

    /**
     * @return {!InspectorBackendClass.Connection}
     */
    connection: function()
    {
        if (!this._connection)
            throw "Main connection was not initialized";
        return this._connection;
    },

    /**
     * @param {!InspectorBackendClass.MainConnection} connection
     */
    setConnection: function(connection)
    {
        this._connection = connection;
        this._connection.initialize(this._agentPrototypes, this._dispatcherPrototypes);

        this._connection.registerAgentsOn(window);
        for (var type in this._enums) {
            var domainAndMethod = type.split(".");
            window[domainAndMethod[0] + "Agent"][domainAndMethod[1]] = this._enums[type];
        }
    },

    /**
     * @param {string} domain
     * @return {!InspectorBackendClass.AgentPrototype}
     */
    _agentPrototype: function(domain)
    {
        if (!this._agentPrototypes[domain]) {
            this._agentPrototypes[domain] = new InspectorBackendClass.AgentPrototype(domain);
            this._addAgentGetterMethodToProtocolAgentsPrototype(domain);
        }

        return this._agentPrototypes[domain];
    },

    /**
     * @param {string} domain
     * @return {!InspectorBackendClass.DispatcherPrototype}
     */
    _dispatcherPrototype: function(domain)
    {
        if (!this._dispatcherPrototypes[domain])
            this._dispatcherPrototypes[domain] = new InspectorBackendClass.DispatcherPrototype();
        return this._dispatcherPrototypes[domain];
    },

    /**
     * @param {string} method
     * @param {!Array.<!Object>} signature
     * @param {!Array.<string>} replyArgs
     * @param {boolean} hasErrorData
     */
    registerCommand: function(method, signature, replyArgs, hasErrorData)
    {
        var domainAndMethod = method.split(".");
        this._agentPrototype(domainAndMethod[0]).registerCommand(domainAndMethod[1], signature, replyArgs, hasErrorData);
        this._initialized = true;
    },

    /**
     * @param {string} type
     * @param {!Object} values
     */
    registerEnum: function(type, values)
    {
        this._enums[type] = values;
        this._initialized = true;
    },

    /**
     * @param {string} eventName
     * @param {!Object} params
     */
    registerEvent: function(eventName, params)
    {
        var domain = eventName.split(".")[0];
        this._dispatcherPrototype(domain).registerEvent(eventName, params);
        this._initialized = true;
    },

    /**
     * @param {string} domain
     * @param {!Object} dispatcher
     */
    registerDomainDispatcher: function(domain, dispatcher)
    {
        this._connection.registerDispatcher(domain, dispatcher);
    },

    /**
     * @param {string} jsonUrl
     */
    loadFromJSONIfNeeded: function(jsonUrl)
    {
        if (this._initialized)
            return;

        var xhr = new XMLHttpRequest();
        xhr.open("GET", jsonUrl, false);
        xhr.send(null);

        var schema = JSON.parse(xhr.responseText);
        var code = InspectorBackendClass._generateCommands(schema);
        eval(code);
    },

    /**
     * @param {function(T)} clientCallback
     * @param {string} errorPrefix
     * @param {function(new:T,S)=} constructor
     * @param {T=} defaultValue
     * @return {function(?string, S)}
     * @template T,S
     */
    wrapClientCallback: function(clientCallback, errorPrefix, constructor, defaultValue)
    {
        /**
         * @param {?string} error
         * @param {S} value
         * @template S
         */
        function callbackWrapper(error, value)
        {
            if (error) {
                console.error(errorPrefix + error);
                clientCallback(defaultValue);
                return;
            }
            if (constructor)
                clientCallback(new constructor(value));
            else
                clientCallback(value);
        }
        return callbackWrapper;
    }
}

/**
 * @param {*} schema
 * @return {string}
 */
InspectorBackendClass._generateCommands = function(schema) {
    var jsTypes = { integer: "number", array: "object" };
    var rawTypes = {};
    var result = [];

    var domains = schema["domains"] || [];
    for (var i = 0; i < domains.length; ++i) {
        var domain = domains[i];
        for (var j = 0; domain.types && j < domain.types.length; ++j) {
            var type = domain.types[j];
            rawTypes[domain.domain + "." + type.id] = jsTypes[type.type] || type.type;
        }
    }

    function toUpperCase(groupIndex, group0, group1)
    {
        return [group0, group1][groupIndex].toUpperCase();
    }
    function generateEnum(enumName, items)
    {
        var members = []
        for (var m = 0; m < items.length; ++m) {
            var value = items[m];
            var name = value.replace(/-(\w)/g, toUpperCase.bind(null, 1)).toTitleCase();
            name = name.replace(/HTML|XML|WML|API/ig, toUpperCase.bind(null, 0));
            members.push(name + ": \"" + value +"\"");
        }
        return "InspectorBackend.registerEnum(\"" + enumName + "\", {" + members.join(", ") + "});";
    }

    for (var i = 0; i < domains.length; ++i) {
        var domain = domains[i];

        var types = domain["types"] || [];
        for (var j = 0; j < types.length; ++j) {
            var type = types[j];
            if ((type["type"] === "string") && type["enum"])
                result.push(generateEnum(domain.domain + "." + type.id, type["enum"]));
            else if (type["type"] === "object") {
                var properties = type["properties"] || [];
                for (var k = 0; k < properties.length; ++k) {
                    var property = properties[k];
                    if ((property["type"] === "string") && property["enum"])
                        result.push(generateEnum(domain.domain + "." + type.id + property["name"].toTitleCase(), property["enum"]));
                }
            }
        }

        var commands = domain["commands"] || [];
        for (var j = 0; j < commands.length; ++j) {
            var command = commands[j];
            var parameters = command["parameters"];
            var paramsText = [];
            for (var k = 0; parameters && k < parameters.length; ++k) {
                var parameter = parameters[k];

                var type;
                if (parameter.type)
                    type = jsTypes[parameter.type] || parameter.type;
                else {
                    var ref = parameter["$ref"];
                    if (ref.indexOf(".") !== -1)
                        type = rawTypes[ref];
                    else
                        type = rawTypes[domain.domain + "." + ref];
                }

                var text = "{\"name\": \"" + parameter.name + "\", \"type\": \"" + type + "\", \"optional\": " + (parameter.optional ? "true" : "false") + "}";
                paramsText.push(text);
            }

            var returnsText = [];
            var returns = command["returns"] || [];
            for (var k = 0; k < returns.length; ++k) {
                var parameter = returns[k];
                returnsText.push("\"" + parameter.name + "\"");
            }
            var hasErrorData = String(Boolean(command.error));
            result.push("InspectorBackend.registerCommand(\"" + domain.domain + "." + command.name + "\", [" + paramsText.join(", ") + "], [" + returnsText.join(", ") + "], " + hasErrorData + ");");
        }

        for (var j = 0; domain.events && j < domain.events.length; ++j) {
            var event = domain.events[j];
            var paramsText = [];
            for (var k = 0; event.parameters && k < event.parameters.length; ++k) {
                var parameter = event.parameters[k];
                paramsText.push("\"" + parameter.name + "\"");
            }
            result.push("InspectorBackend.registerEvent(\"" + domain.domain + "." + event.name + "\", [" + paramsText.join(", ") + "]);");
        }

        result.push("InspectorBackend.register" + domain.domain + "Dispatcher = InspectorBackend.registerDomainDispatcher.bind(InspectorBackend, \"" + domain.domain + "\");");
    }
    return result.join("\n");
}

/**
 *  @constructor
 *  @extends {WebInspector.Object}
 */
InspectorBackendClass.Connection = function()
{
    this._lastMessageId = 1;
    this._pendingResponsesCount = 0;
    this._agents = {};
    this._dispatchers = {};
    this._callbacks = {};
}

InspectorBackendClass.Connection.Events = {
    Disconnected: "Disconnected",
}

InspectorBackendClass.Connection.prototype = {

    /**
     * @param {!Object.<string, !InspectorBackendClass.AgentPrototype>} agentPrototypes
     * @param {!Object.<string, !InspectorBackendClass.DispatcherPrototype>} dispatcherPrototypes
     */
    initialize: function(agentPrototypes, dispatcherPrototypes)
    {
        for (var domain in agentPrototypes) {
            this._agents[domain] = Object.create(agentPrototypes[domain]);
            this._agents[domain].setConnection(this);
        }

        for (var domain in dispatcherPrototypes)
            this._dispatchers[domain] = Object.create(dispatcherPrototypes[domain])

    },

    /**
     * @param {!Object} object
     */
    registerAgentsOn: function(object)
    {
        for (var domain in this._agents)
            object[domain + "Agent"]  = this._agents[domain];
    },

    /**
     * @return {number}
     */
    nextMessageId: function()
    {
        return this._lastMessageId++;
    },

    /**
     * @param {string} domain
     * @return {!InspectorBackendClass.AgentPrototype}
     */
    agent: function(domain)
    {
        return this._agents[domain];
    },

    /**
     * @return {!Object.<string, !Object>}
     */
    agentsMap: function()
    {
        return this._agents;
    },

    /**
     * @param {string} domain
     * @param {string} method
     * @param {?Object} params
     * @param {?function(*)} callback
     * @private
     */
    _wrapCallbackAndSendMessageObject: function(domain, method, params, callback)
    {
        var messageObject = {};
        messageObject.method = method;
        if (params)
            messageObject.params = params;

        var wrappedCallback = this._wrap(callback, domain, method);

        var messageId = this.nextMessageId();
        messageObject.id = messageId;

        if (InspectorBackendClass.Options.dumpInspectorProtocolMessages)
            console.log("frontend: " + JSON.stringify(messageObject));

        this.sendMessage(messageObject);
        ++this._pendingResponsesCount;
        this._callbacks[messageId] = wrappedCallback;
    },

    /**
     * @param {?function(*)} callback
     * @param {string} method
     * @param {string} domain
     * @return {!function(*)}
     */
    _wrap: function(callback, domain, method)
    {
        if (!callback)
            callback = function() {};

        callback.methodName = method;
        callback.domain = domain;
        if (InspectorBackendClass.Options.dumpInspectorTimeStats)
            callback.sendRequestTime = Date.now();

        return callback;
    },

    /**
     * @param {!Object} messageObject
     */
    sendMessage: function(messageObject)
    {
        throw "Not implemented";
    },

    /**
     * @param {!Object} messageObject
     */
    reportProtocolError: function(messageObject)
    {
        console.error("Protocol Error: the message with wrong id. Message =  " + JSON.stringify(messageObject));
    },

    /**
     * @param {!Object|string} message
     */
    dispatch: function(message)
    {
        if (InspectorBackendClass.Options.dumpInspectorProtocolMessages)
            console.log("backend: " + ((typeof message === "string") ? message : JSON.stringify(message)));

        var messageObject = /** @type {!Object} */ ((typeof message === "string") ? JSON.parse(message) : message);

        if ("id" in messageObject) { // just a response for some request

            var callback = this._callbacks[messageObject.id];
            if (!callback) {
                this.reportProtocolError(messageObject);
                return;
            }

            var processingStartTime;
            if (InspectorBackendClass.Options.dumpInspectorTimeStats)
                processingStartTime = Date.now();

            this.agent(callback.domain).dispatchResponse(messageObject.id, messageObject, callback.methodName, callback);
            --this._pendingResponsesCount;
            delete this._callbacks[messageObject.id];

            if (InspectorBackendClass.Options.dumpInspectorTimeStats)
                console.log("time-stats: " + callback.methodName + " = " + (processingStartTime - callback.sendRequestTime) + " + " + (Date.now() - processingStartTime));

            if (this._scripts && !this._pendingResponsesCount)
                this.runAfterPendingDispatches();
            return;
        } else {
            var method = messageObject.method.split(".");
            var domainName = method[0];
            if (!(domainName in this._dispatchers)) {
                console.error("Protocol Error: the message " + messageObject.method + " is for non-existing domain '" + domainName + "'");
                return;
            }

            this._dispatchers[domainName].dispatch(method[1], messageObject);

        }

    },

    /**
     * @param {string} domain
     * @param {!Object} dispatcher
     */
    registerDispatcher: function(domain, dispatcher)
    {
        if (!this._dispatchers[domain])
            return;

        this._dispatchers[domain].setDomainDispatcher(dispatcher);
    },

    /**
     * @param {string=} script
     */
    runAfterPendingDispatches: function(script)
    {
        if (!this._scripts)
            this._scripts = [];

        if (script)
            this._scripts.push(script);

        if (!this._pendingResponsesCount) {
            var scripts = this._scripts;
            this._scripts = []
            for (var id = 0; id < scripts.length; ++id)
                scripts[id].call(this);
        }
    },

    /**
     * @param {string} reason
     */
    fireDisconnected: function(reason)
    {
        this.dispatchEventToListeners(InspectorBackendClass.Connection.Events.Disconnected, {reason: reason});
    },

    __proto__: WebInspector.Object.prototype

}

/**
 * @constructor
 * @extends {InspectorBackendClass.Connection}
 * @param {!function(!InspectorBackendClass.Connection)} onConnectionReady
 */
InspectorBackendClass.MainConnection = function(onConnectionReady)
{
    InspectorBackendClass.Connection.call(this);
    onConnectionReady(this);
}

InspectorBackendClass.MainConnection.prototype = {

    /**
     * @param {!Object} messageObject
     */
    sendMessage: function(messageObject)
    {
        var message = JSON.stringify(messageObject);
        InspectorFrontendHost.sendMessageToBackend(message);
    },

    __proto__: InspectorBackendClass.Connection.prototype
}

/**
 * @constructor
 * @extends {InspectorBackendClass.Connection}
 * @param {string} url
 * @param {!function(!InspectorBackendClass.Connection)} onConnectionReady
 */
InspectorBackendClass.WebSocketConnection = function(url, onConnectionReady)
{
    InspectorBackendClass.Connection.call(this);
    this._socket = new WebSocket(url);
    this._socket.onmessage = this._onMessage.bind(this);
    this._socket.onerror = this._onError.bind(this);
    this._socket.onopen = onConnectionReady.bind(null, this);
    this._socket.onclose = this.fireDisconnected.bind(this, "websocket_closed");
}

InspectorBackendClass.WebSocketConnection.prototype = {

    /**
     * @param {!MessageEvent} message
     */
    _onMessage: function(message)
    {
        var data = /** @type {string} */ (message.data)
        this.dispatch(data);
    },

    /**
     * @param {!Event} error
     */
    _onError: function(error)
    {
        console.error(error);
    },

    /**
     * @param {!Object} messageObject
     */
    sendMessage: function(messageObject)
    {
        var message = JSON.stringify(messageObject);
        this._socket.send(message);
    },

    __proto__: InspectorBackendClass.Connection.prototype
}


/**
 * @constructor
 * @extends {InspectorBackendClass.Connection}
 * @param {!function(!InspectorBackendClass.Connection)} onConnectionReady
 */
InspectorBackendClass.StubConnection = function(onConnectionReady)
{
    InspectorBackendClass.Connection.call(this);
    onConnectionReady(this);
}

InspectorBackendClass.StubConnection.prototype = {

    /**
     * @param {!Object} messageObject
     */
    sendMessage: function(messageObject)
    {
        var message = JSON.stringify(messageObject);
        setTimeout(this._echoResponse.bind(this, messageObject), 0);
    },

    /**
     * @param {!Object} messageObject
     */
    _echoResponse: function(messageObject)
    {
        this.dispatch(messageObject)
    },

    __proto__: InspectorBackendClass.Connection.prototype
}

/**
 * @constructor
 * @param {string} domain
 */
InspectorBackendClass.AgentPrototype = function(domain)
{
    this._replyArgs = {};
    this._hasErrorData = {};
    this._domain = domain;
}

InspectorBackendClass.AgentPrototype.prototype = {

    /**
     * @param {!InspectorBackendClass.Connection} connection
     */
    setConnection: function(connection)
    {
        this._connection = connection;
    },

    /**
     * @param {string} methodName
     * @param {!Array.<!Object>} signature
     * @param {!Array.<string>} replyArgs
     * @param {boolean} hasErrorData
     */
    registerCommand: function(methodName, signature, replyArgs, hasErrorData)
    {
        var domainAndMethod = this._domain + "." + methodName;

        /**
         * @this {InspectorBackendClass.AgentPrototype}
         */
        function sendMessage(vararg)
        {
            var params = [domainAndMethod, signature].concat(Array.prototype.slice.call(arguments));
            InspectorBackendClass.AgentPrototype.prototype._sendMessageToBackend.apply(this, params);
        }

        this[methodName] = sendMessage;

        /**
         * @this {InspectorBackendClass.AgentPrototype}
         */
        function invoke(vararg)
        {
            var params = [domainAndMethod].concat(Array.prototype.slice.call(arguments));
            InspectorBackendClass.AgentPrototype.prototype._invoke.apply(this, params);
        }

        this["invoke_" + methodName] = invoke;

        this._replyArgs[domainAndMethod] = replyArgs;
        if (hasErrorData)
            this._hasErrorData[domainAndMethod] = true;

    },

    /**
     * @param {string} method
     * @param {!Array.<!Object>} signature
     * @param {*} vararg
     * @private
     */
    _sendMessageToBackend: function(method, signature, vararg)
    {
        var args = Array.prototype.slice.call(arguments, 2);
        var callback = (args.length && typeof args[args.length - 1] === "function") ? args.pop() : null;

        var params = {};
        var hasParams = false;
        for (var i = 0; i < signature.length; ++i) {
            var param = signature[i];
            var paramName = param["name"];
            var typeName = param["type"];
            var optionalFlag = param["optional"];

            if (!args.length && !optionalFlag) {
                console.error("Protocol Error: Invalid number of arguments for method '" + method + "' call. It must have the following arguments '" + JSON.stringify(signature) + "'.");
                return;
            }

            var value = args.shift();
            if (optionalFlag && typeof value === "undefined") {
                continue;
            }

            if (typeof value !== typeName) {
                console.error("Protocol Error: Invalid type of argument '" + paramName + "' for method '" + method + "' call. It must be '" + typeName + "' but it is '" + typeof value + "'.");
                return;
            }

            params[paramName] = value;
            hasParams = true;
        }

        if (args.length === 1 && !callback && (typeof args[0] !== "undefined")) {
            console.error("Protocol Error: Optional callback argument for method '" + method + "' call must be a function but its type is '" + typeof args[0] + "'.");
            return;
        }

        this._connection._wrapCallbackAndSendMessageObject(this._domain, method, hasParams ? params : null, callback);
    },

    /**
     * @param {string} method
     * @param {?Object} args
     * @param {?function(*)} callback
     */
    _invoke: function(method, args, callback)
    {
        this._connection._wrapCallbackAndSendMessageObject(this._domain, method, args, callback);
    },

    /**
     * @param {number} messageId
     * @param {!Object} messageObject
     */
    dispatchResponse: function(messageId, messageObject, methodName, callback)
    {
        if (messageObject.error && messageObject.error.code !== -32000)
            console.error("Request with id = " + messageObject.id + " failed. " + JSON.stringify(messageObject.error));

        var argumentsArray = [];
        argumentsArray[0] = messageObject.error ? messageObject.error.message: null;

        if (this._hasErrorData[methodName])
            argumentsArray[1] = messageObject.error ? messageObject.error.data : null;

        if (messageObject.result) {
            var paramNames = this._replyArgs[methodName] || [];
            for (var i = 0; i < paramNames.length; ++i)
                argumentsArray.push(messageObject.result[paramNames[i]]);
        }

        callback.apply(null, argumentsArray);
    }
}

/**
 * @constructor
 */
InspectorBackendClass.DispatcherPrototype = function()
{
    this._eventArgs = {};
    this._dispatcher = null;
}

InspectorBackendClass.DispatcherPrototype.prototype = {

    /**
     * @param {string} eventName
     * @param {!Object} params
     */
    registerEvent: function(eventName, params)
    {
        this._eventArgs[eventName] = params
    },

    /**
     * @param {!Object} dispatcher
     */
    setDomainDispatcher: function(dispatcher)
    {
        this._dispatcher = dispatcher;
    },

    /**
     * @param {string} functionName
     * @param {!Object} messageObject
     */
    dispatch: function(functionName, messageObject)
    {
        if (!this._dispatcher)
            return;

        if (!(functionName in this._dispatcher)) {
            console.error("Protocol Error: Attempted to dispatch an unimplemented method '" + messageObject.method + "'");
            return;
        }

        if (!this._eventArgs[messageObject.method]) {
            console.error("Protocol Error: Attempted to dispatch an unspecified method '" + messageObject.method + "'");
            return;
        }

        var params = [];
        if (messageObject.params) {
            var paramNames = this._eventArgs[messageObject.method];
            for (var i = 0; i < paramNames.length; ++i)
                params.push(messageObject.params[paramNames[i]]);
        }

        var processingStartTime;
        if (InspectorBackendClass.Options.dumpInspectorTimeStats)
            processingStartTime = Date.now();

        this._dispatcher[functionName].apply(this._dispatcher, params);

        if (InspectorBackendClass.Options.dumpInspectorTimeStats)
            console.log("time-stats: " + messageObject.method + " = " + (Date.now() - processingStartTime));
    }

}

InspectorBackendClass.Options = {
    dumpInspectorTimeStats: false,
    dumpInspectorProtocolMessages: false
}

InspectorBackend = new InspectorBackendClass();
