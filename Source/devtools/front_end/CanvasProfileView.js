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
 * @extends {WebInspector.VBox}
 * @param {!WebInspector.CanvasProfileHeader} profile
 */
WebInspector.CanvasProfileView = function(profile)
{
    WebInspector.VBox.call(this);
    this.registerRequiredCSS("canvasProfiler.css");
    this.element.classList.add("canvas-profile-view");
    this._profile = profile;
    this._traceLogId = profile.traceLogId();
    this._traceLogPlayer = /** @type {!WebInspector.CanvasTraceLogPlayerProxy} */ (profile.traceLogPlayer());
    this._linkifier = new WebInspector.Linkifier();

    const defaultReplayLogWidthPercent = 0.34;
    this._replayInfoSplitView = new WebInspector.SplitView(true, true, "canvasProfileViewReplaySplitViewState", defaultReplayLogWidthPercent);
    this._replayInfoSplitView.setMainElementConstraints(defaultReplayLogWidthPercent, defaultReplayLogWidthPercent);
    this._replayInfoSplitView.show(this.element);

    this._imageSplitView = new WebInspector.SplitView(false, true, "canvasProfileViewSplitViewState", 300);
    this._imageSplitView.show(this._replayInfoSplitView.mainElement());

    var replayImageContainer = this._imageSplitView.mainElement().createChild("div");
    replayImageContainer.id = "canvas-replay-image-container";
    this._replayImageElement = replayImageContainer.createChild("img", "canvas-replay-image");
    this._debugInfoElement = replayImageContainer.createChild("div", "canvas-debug-info hidden");
    this._spinnerIcon = replayImageContainer.createChild("div", "spinner-icon small hidden");

    var replayLogContainer = this._imageSplitView.sidebarElement();
    var controlsContainer = replayLogContainer.createChild("div", "status-bar");
    var logGridContainer = replayLogContainer.createChild("div", "canvas-replay-log");

    this._createControlButton(controlsContainer, "canvas-replay-first-step", WebInspector.UIString("First call."), this._onReplayFirstStepClick.bind(this));
    this._createControlButton(controlsContainer, "canvas-replay-prev-step", WebInspector.UIString("Previous call."), this._onReplayStepClick.bind(this, false));
    this._createControlButton(controlsContainer, "canvas-replay-next-step", WebInspector.UIString("Next call."), this._onReplayStepClick.bind(this, true));
    this._createControlButton(controlsContainer, "canvas-replay-prev-draw", WebInspector.UIString("Previous drawing call."), this._onReplayDrawingCallClick.bind(this, false));
    this._createControlButton(controlsContainer, "canvas-replay-next-draw", WebInspector.UIString("Next drawing call."), this._onReplayDrawingCallClick.bind(this, true));
    this._createControlButton(controlsContainer, "canvas-replay-last-step", WebInspector.UIString("Last call."), this._onReplayLastStepClick.bind(this));

    this._replayContextSelector = new WebInspector.StatusBarComboBox(this._onReplayContextChanged.bind(this));
    this._replayContextSelector.createOption(WebInspector.UIString("<screenshot auto>"), WebInspector.UIString("Show screenshot of the last replayed resource."), "");
    controlsContainer.appendChild(this._replayContextSelector.element);

    this._installReplayInfoSidebarWidgets(controlsContainer);

    this._replayStateView = new WebInspector.CanvasReplayStateView(this._traceLogPlayer);
    this._replayStateView.show(this._replayInfoSplitView.sidebarElement());

    /** @type {!Object.<string, boolean>} */
    this._replayContexts = {};

    var columns = [
        {title: "#", sortable: false, width: "5%"},
        {title: WebInspector.UIString("Call"), sortable: false, width: "75%", disclosure: true},
        {title: WebInspector.UIString("Location"), sortable: false, width: "20%"}
    ];

    this._logGrid = new WebInspector.DataGrid(columns);
    this._logGrid.element.classList.add("fill");
    this._logGrid.show(logGridContainer);
    this._logGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode, this._replayTraceLog, this);

    this.element.addEventListener("mousedown", this._onMouseClick.bind(this), true);

    this._popoverHelper = new WebInspector.ObjectPopoverHelper(this.element, this._popoverAnchor.bind(this), this._resolveObjectForPopover.bind(this), this._onHidePopover.bind(this), true);
    this._popoverHelper.setRemoteObjectFormatter(this._hexNumbersFormatter.bind(this));

    this._requestTraceLog(0);
}

/**
 * @const
 * @type {number}
 */
WebInspector.CanvasProfileView.TraceLogPollingInterval = 500;

WebInspector.CanvasProfileView.prototype = {
    dispose: function()
    {
        this._linkifier.reset();
    },

    get statusBarItems()
    {
        return [];
    },

    get profile()
    {
        return this._profile;
    },

    /**
     * @override
     * @return {!Array.<!Element>}
     */
    elementsToRestoreScrollPositionsFor: function()
    {
        return [this._logGrid.scrollContainer];
    },

    /**
     * @param {!Element} controlsContainer
     */
    _installReplayInfoSidebarWidgets: function(controlsContainer)
    {
        this._replayInfoResizeWidgetElement = controlsContainer.createChild("div", "resizer-widget");
        this._replayInfoSplitView.addEventListener(WebInspector.SplitView.Events.ShowModeChanged, this._updateReplayInfoResizeWidget, this);
        this._updateReplayInfoResizeWidget();
        this._replayInfoSplitView.installResizer(this._replayInfoResizeWidgetElement);

        this._toggleReplayStateSidebarButton = this._replayInfoSplitView.createShowHideSidebarButton("sidebar", "canvas-sidebar-show-hide-button");

        controlsContainer.appendChild(this._toggleReplayStateSidebarButton.element);
        this._replayInfoSplitView.hideSidebar();
    },

    _updateReplayInfoResizeWidget: function()
    {
        this._replayInfoResizeWidgetElement.classList.toggle("hidden", this._replayInfoSplitView.showMode() !== WebInspector.SplitView.ShowMode.Both);
    },

    /**
     * @param {?Event} event
     */
    _onMouseClick: function(event)
    {
        var resourceLinkElement = event.target.enclosingNodeOrSelfWithClass("canvas-formatted-resource");
        if (resourceLinkElement) {
            this._replayInfoSplitView.showBoth();
            this._replayStateView.selectResource(resourceLinkElement.__resourceId);
            event.consume(true);
            return;
        }
        if (event.target.enclosingNodeOrSelfWithClass("webkit-html-resource-link"))
            event.consume(false);
    },

    /**
     * @param {!Element} parent
     * @param {string} className
     * @param {string} title
     * @param {function(this:WebInspector.CanvasProfileView)} clickCallback
     */
    _createControlButton: function(parent, className, title, clickCallback)
    {
        var button = new WebInspector.StatusBarButton(title, className + " canvas-replay-button");
        parent.appendChild(button.element);

        button.makeLongClickEnabled();
        button.addEventListener("click", clickCallback, this);
        button.addEventListener("longClickDown", clickCallback, this);
        button.addEventListener("longClickPress", clickCallback, this);
    },

    _onReplayContextChanged: function()
    {
        var selectedContextId = this._replayContextSelector.selectedOption().value;

        /**
         * @param {?CanvasAgent.ResourceState} resourceState
         * @this {WebInspector.CanvasProfileView}
         */
        function didReceiveResourceState(resourceState)
        {
            this._enableWaitIcon(false);
            if (selectedContextId !== this._replayContextSelector.selectedOption().value)
                return;
            var imageURL = (resourceState && resourceState.imageURL) || "";
            this._replayImageElement.src = imageURL;
            this._replayImageElement.style.visibility = imageURL ? "" : "hidden";
        }

        this._enableWaitIcon(true);
        this._traceLogPlayer.getResourceState(selectedContextId, didReceiveResourceState.bind(this));
    },

    /**
     * @param {boolean} forward
     */
    _onReplayStepClick: function(forward)
    {
        var selectedNode = this._logGrid.selectedNode;
        if (!selectedNode)
            return;
        var nextNode = selectedNode;
        do {
            nextNode = forward ? nextNode.traverseNextNode(false) : nextNode.traversePreviousNode(false);
        } while (nextNode && typeof nextNode.index !== "number");
        (nextNode || selectedNode).revealAndSelect();
    },

    /**
     * @param {boolean} forward
     */
    _onReplayDrawingCallClick: function(forward)
    {
        var selectedNode = this._logGrid.selectedNode;
        if (!selectedNode)
            return;
        var nextNode = selectedNode;
        while (nextNode) {
            var sibling = forward ? nextNode.nextSibling : nextNode.previousSibling;
            if (sibling) {
                nextNode = sibling;
                if (nextNode.hasChildren || nextNode.call.isDrawingCall)
                    break;
            } else {
                nextNode = nextNode.parent;
                if (!forward)
                    break;
            }
        }
        if (!nextNode && forward)
            this._onReplayLastStepClick();
        else
            (nextNode || selectedNode).revealAndSelect();
    },

    _onReplayFirstStepClick: function()
    {
        var firstNode = this._logGrid.rootNode().children[0];
        if (firstNode)
            firstNode.revealAndSelect();
    },

    _onReplayLastStepClick: function()
    {
        var lastNode = this._logGrid.rootNode().children.peekLast();
        if (!lastNode)
            return;
        while (lastNode.expanded) {
            var lastChild = lastNode.children.peekLast();
            if (!lastChild)
                break;
            lastNode = lastChild;
        }
        lastNode.revealAndSelect();
    },

    /**
     * @param {boolean} enable
     */
    _enableWaitIcon: function(enable)
    {
        this._spinnerIcon.classList.toggle("hidden", !enable);
        this._debugInfoElement.classList.toggle("hidden", enable);
    },

    _replayTraceLog: function()
    {
        if (this._pendingReplayTraceLogEvent)
            return;
        var index = this._selectedCallIndex();
        if (index === -1 || index === this._lastReplayCallIndex)
            return;
        this._lastReplayCallIndex = index;
        this._pendingReplayTraceLogEvent = true;

        /**
         * @param {?CanvasAgent.ResourceState} resourceState
         * @param {number} replayTime
         * @this {WebInspector.CanvasProfileView}
         */
        function didReplayTraceLog(resourceState, replayTime)
        {
            delete this._pendingReplayTraceLogEvent;
            this._enableWaitIcon(false);

            this._debugInfoElement.textContent = WebInspector.UIString("Replay time: %s", Number.secondsToString(replayTime / 1000, true));
            this._onReplayContextChanged();

            if (index !== this._selectedCallIndex())
                this._replayTraceLog();
        }
        this._enableWaitIcon(true);
        this._traceLogPlayer.replayTraceLog(index, didReplayTraceLog.bind(this));
    },

    /**
     * @param {number} offset
     */
    _requestTraceLog: function(offset)
    {
        /**
         * @param {?CanvasAgent.TraceLog} traceLog
         * @this {WebInspector.CanvasProfileView}
         */
        function didReceiveTraceLog(traceLog)
        {
            this._enableWaitIcon(false);
            if (!traceLog)
                return;
            var callNodes = [];
            var calls = traceLog.calls;
            var index = traceLog.startOffset;
            for (var i = 0, n = calls.length; i < n; ++i)
                callNodes.push(this._createCallNode(index++, calls[i]));
            var contexts = traceLog.contexts;
            for (var i = 0, n = contexts.length; i < n; ++i) {
                var contextId = contexts[i].resourceId || "";
                var description = contexts[i].description || "";
                if (this._replayContexts[contextId])
                    continue;
                this._replayContexts[contextId] = true;
                this._replayContextSelector.createOption(description, WebInspector.UIString("Show screenshot of this context's canvas."), contextId);
            }
            this._appendCallNodes(callNodes);
            if (traceLog.alive)
                setTimeout(this._requestTraceLog.bind(this, index), WebInspector.CanvasProfileView.TraceLogPollingInterval);
            else
                this._flattenSingleFrameNode();
            this._profile._updateCapturingStatus(traceLog);
            this._onReplayLastStepClick(); // Automatically replay the last step.
        }
        this._enableWaitIcon(true);
        this._traceLogPlayer.getTraceLog(offset, undefined, didReceiveTraceLog.bind(this));
    },

    /**
     * @return {number}
     */
    _selectedCallIndex: function()
    {
        var node = this._logGrid.selectedNode;
        return node ? this._peekLastRecursively(node).index : -1;
    },

    /**
     * @param {!WebInspector.DataGridNode} node
     * @return {!WebInspector.DataGridNode}
     */
    _peekLastRecursively: function(node)
    {
        var lastChild;
        while ((lastChild = node.children.peekLast()))
            node = lastChild;
        return node;
    },

    /**
     * @param {!Array.<!WebInspector.DataGridNode>} callNodes
     */
    _appendCallNodes: function(callNodes)
    {
        var rootNode = this._logGrid.rootNode();
        var frameNode = rootNode.children.peekLast();
        if (frameNode && this._peekLastRecursively(frameNode).call.isFrameEndCall)
            frameNode = null;
        for (var i = 0, n = callNodes.length; i < n; ++i) {
            if (!frameNode) {
                var index = rootNode.children.length;
                var data = {};
                data[0] = "";
                data[1] = WebInspector.UIString("Frame #%d", index + 1);
                data[2] = "";
                frameNode = new WebInspector.DataGridNode(data);
                frameNode.selectable = true;
                rootNode.appendChild(frameNode);
            }
            var nextFrameCallIndex = i + 1;
            while (nextFrameCallIndex < n && !callNodes[nextFrameCallIndex - 1].call.isFrameEndCall)
                ++nextFrameCallIndex;
            this._appendCallNodesToFrameNode(frameNode, callNodes, i, nextFrameCallIndex);
            i = nextFrameCallIndex - 1;
            frameNode = null;
        }
    },

    /**
     * @param {!WebInspector.DataGridNode} frameNode
     * @param {!Array.<!WebInspector.DataGridNode>} callNodes
     * @param {number} fromIndex
     * @param {number} toIndex not inclusive
     */
    _appendCallNodesToFrameNode: function(frameNode, callNodes, fromIndex, toIndex)
    {
        var self = this;
        function appendDrawCallGroup()
        {
            var index = self._drawCallGroupsCount || 0;
            var data = {};
            data[0] = "";
            data[1] = WebInspector.UIString("Draw call group #%d", index + 1);
            data[2] = "";
            var node = new WebInspector.DataGridNode(data);
            node.selectable = true;
            self._drawCallGroupsCount = index + 1;
            frameNode.appendChild(node);
            return node;
        }

        function splitDrawCallGroup(drawCallGroup)
        {
            var splitIndex = 0;
            var splitNode;
            while ((splitNode = drawCallGroup.children[splitIndex])) {
                if (splitNode.call.isDrawingCall)
                    break;
                ++splitIndex;
            }
            var newDrawCallGroup = appendDrawCallGroup();
            var lastNode;
            while ((lastNode = drawCallGroup.children[splitIndex + 1]))
                newDrawCallGroup.appendChild(lastNode);
            return newDrawCallGroup;
        }

        var drawCallGroup = frameNode.children.peekLast();
        var groupHasDrawCall = false;
        if (drawCallGroup) {
            for (var i = 0, n = drawCallGroup.children.length; i < n; ++i) {
                if (drawCallGroup.children[i].call.isDrawingCall) {
                    groupHasDrawCall = true;
                    break;
                }
            }
        } else
            drawCallGroup = appendDrawCallGroup();

        for (var i = fromIndex; i < toIndex; ++i) {
            var node = callNodes[i];
            drawCallGroup.appendChild(node);
            if (node.call.isDrawingCall) {
                if (groupHasDrawCall)
                    drawCallGroup = splitDrawCallGroup(drawCallGroup);
                else
                    groupHasDrawCall = true;
            }
        }
    },

    /**
     * @param {number} index
     * @param {!CanvasAgent.Call} call
     * @return {!WebInspector.DataGridNode}
     */
    _createCallNode: function(index, call)
    {
        var callViewElement = document.createElement("div");

        var data = {};
        data[0] = index + 1;
        data[1] = callViewElement;
        data[2] = "";
        if (call.sourceURL) {
            // FIXME(62725): stack trace line/column numbers are one-based.
            var lineNumber = Math.max(0, call.lineNumber - 1) || 0;
            var columnNumber = Math.max(0, call.columnNumber - 1) || 0;
            data[2] = this._linkifier.linkifyLocation(call.sourceURL, lineNumber, columnNumber);
        }

        callViewElement.createChild("span", "canvas-function-name").textContent = call.functionName || "context." + call.property;

        if (call.arguments) {
            callViewElement.createTextChild("(");
            for (var i = 0, n = call.arguments.length; i < n; ++i) {
                var argument = /** @type {!CanvasAgent.CallArgument} */ (call.arguments[i]);
                if (i)
                    callViewElement.createTextChild(", ");
                var element = WebInspector.CanvasProfileDataGridHelper.createCallArgumentElement(argument);
                element.__argumentIndex = i;
                callViewElement.appendChild(element);
            }
            callViewElement.createTextChild(")");
        } else if (call.value) {
            callViewElement.createTextChild(" = ");
            callViewElement.appendChild(WebInspector.CanvasProfileDataGridHelper.createCallArgumentElement(call.value));
        }

        if (call.result) {
            callViewElement.createTextChild(" => ");
            callViewElement.appendChild(WebInspector.CanvasProfileDataGridHelper.createCallArgumentElement(call.result));
        }

        var node = new WebInspector.DataGridNode(data);
        node.index = index;
        node.selectable = true;
        node.call = call;
        return node;
    },

    _popoverAnchor: function(element, event)
    {
        var argumentElement = element.enclosingNodeOrSelfWithClass("canvas-call-argument");
        if (!argumentElement || argumentElement.__suppressPopover)
            return null;
        return argumentElement;
    },

    _resolveObjectForPopover: function(argumentElement, showCallback, objectGroupName)
    {
        /**
         * @param {?Protocol.Error} error
         * @param {!RuntimeAgent.RemoteObject=} result
         * @param {!CanvasAgent.ResourceState=} resourceState
         * @this {WebInspector.CanvasProfileView}
         */
        function showObjectPopover(error, result, resourceState)
        {
            if (error)
                return;

            // FIXME: handle resourceState also
            if (!result)
                return;

            this._popoverAnchorElement = argumentElement.cloneNode(true);
            this._popoverAnchorElement.classList.add("canvas-popover-anchor");
            this._popoverAnchorElement.classList.add("source-frame-eval-expression");
            argumentElement.parentElement.appendChild(this._popoverAnchorElement);

            var diffLeft = this._popoverAnchorElement.boxInWindow().x - argumentElement.boxInWindow().x;
            this._popoverAnchorElement.style.left = this._popoverAnchorElement.offsetLeft - diffLeft + "px";

            showCallback(WebInspector.RemoteObject.fromPayload(result), false, this._popoverAnchorElement);
        }

        var evalResult = argumentElement.__evalResult;
        if (evalResult)
            showObjectPopover.call(this, null, evalResult);
        else {
            var dataGridNode = this._logGrid.dataGridNodeFromNode(argumentElement);
            if (!dataGridNode || typeof dataGridNode.index !== "number") {
                this._popoverHelper.hidePopover();
                return;
            }
            var callIndex = dataGridNode.index;
            var argumentIndex = argumentElement.__argumentIndex;
            if (typeof argumentIndex !== "number")
                argumentIndex = -1;
            CanvasAgent.evaluateTraceLogCallArgument(this._traceLogId, callIndex, argumentIndex, objectGroupName, showObjectPopover.bind(this));
        }
    },

    /**
     * @param {!WebInspector.RemoteObject} object
     * @return {string}
     */
    _hexNumbersFormatter: function(object)
    {
        if (object.type === "number") {
            // Show enum values in hex with min length of 4 (e.g. 0x0012).
            var str = "0000" + Number(object.description).toString(16).toUpperCase();
            str = str.replace(/^0+(.{4,})$/, "$1");
            return "0x" + str;
        }
        return object.description || "";
    },

    _onHidePopover: function()
    {
        if (this._popoverAnchorElement) {
            this._popoverAnchorElement.remove()
            delete this._popoverAnchorElement;
        }
    },

    _flattenSingleFrameNode: function()
    {
        var rootNode = this._logGrid.rootNode();
        if (rootNode.children.length !== 1)
            return;
        var frameNode = rootNode.children[0];
        while (frameNode.children[0])
            rootNode.appendChild(frameNode.children[0]);
        rootNode.removeChild(frameNode);
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @constructor
 * @extends {WebInspector.ProfileType}
 */
WebInspector.CanvasProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.CanvasProfileType.TypeId, WebInspector.UIString("Capture Canvas Frame"));
    this._recording = false;
    this._lastProfileHeader = null;

    this._capturingModeSelector = new WebInspector.StatusBarComboBox(this._dispatchViewUpdatedEvent.bind(this));
    this._capturingModeSelector.element.title = WebInspector.UIString("Canvas capture mode.");
    this._capturingModeSelector.createOption(WebInspector.UIString("Single Frame"), WebInspector.UIString("Capture a single canvas frame."), "");
    this._capturingModeSelector.createOption(WebInspector.UIString("Consecutive Frames"), WebInspector.UIString("Capture consecutive canvas frames."), "1");

    /** @type {!Object.<string, !Element>} */
    this._frameOptions = {};

    /** @type {!Object.<string, boolean>} */
    this._framesWithCanvases = {};

    this._frameSelector = new WebInspector.StatusBarComboBox(this._dispatchViewUpdatedEvent.bind(this));
    this._frameSelector.element.title = WebInspector.UIString("Frame containing the canvases to capture.");
    this._frameSelector.element.classList.add("hidden");
    WebInspector.resourceTreeModel.frames().forEach(this._addFrame, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameAdded, this._frameAdded, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, this._frameRemoved, this);

    this._dispatcher = new WebInspector.CanvasDispatcher(this);
    this._canvasAgentEnabled = false;

    this._decorationElement = document.createElement("div");
    this._decorationElement.className = "profile-canvas-decoration";
    this._updateDecorationElement();
}

WebInspector.CanvasProfileType.TypeId = "CANVAS_PROFILE";

WebInspector.CanvasProfileType.prototype = {
    get statusBarItems()
    {
        return [this._capturingModeSelector.element, this._frameSelector.element];
    },

    get buttonTooltip()
    {
        if (this._isSingleFrameMode())
            return WebInspector.UIString("Capture next canvas frame.");
        else
            return this._recording ? WebInspector.UIString("Stop capturing canvas frames.") : WebInspector.UIString("Start capturing canvas frames.");
    },

    /**
     * @override
     * @return {boolean}
     */
    buttonClicked: function()
    {
        if (!this._canvasAgentEnabled)
            return false;
        if (this._recording) {
            this._recording = false;
            this._stopFrameCapturing();
        } else if (this._isSingleFrameMode()) {
            this._recording = false;
            this._runSingleFrameCapturing();
        } else {
            this._recording = true;
            this._startFrameCapturing();
        }
        return this._recording;
    },

    _runSingleFrameCapturing: function()
    {
        var frameId = this._selectedFrameId();
        CanvasAgent.captureFrame(frameId, this._didStartCapturingFrame.bind(this, frameId));
    },

    _startFrameCapturing: function()
    {
        var frameId = this._selectedFrameId();
        CanvasAgent.startCapturing(frameId, this._didStartCapturingFrame.bind(this, frameId));
    },

    _stopFrameCapturing: function()
    {
        if (!this._lastProfileHeader)
            return;
        var profileHeader = this._lastProfileHeader;
        var traceLogId = profileHeader.traceLogId();
        this._lastProfileHeader = null;
        function didStopCapturing()
        {
            profileHeader._updateCapturingStatus();
        }
        CanvasAgent.stopCapturing(traceLogId, didStopCapturing.bind(this));
    },

    /**
     * @param {string|undefined} frameId
     * @param {?Protocol.Error} error
     * @param {!CanvasAgent.TraceLogId} traceLogId
     */
    _didStartCapturingFrame: function(frameId, error, traceLogId)
    {
        if (error || this._lastProfileHeader && this._lastProfileHeader.traceLogId() === traceLogId)
            return;
        var profileHeader = new WebInspector.CanvasProfileHeader(this, traceLogId, frameId);
        this._lastProfileHeader = profileHeader;
        this.addProfile(profileHeader);
        profileHeader._updateCapturingStatus();
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("CANVAS PROFILE");
    },

    get description()
    {
        return WebInspector.UIString("Canvas calls instrumentation");
    },

    /**
     * @override
     * @return {!Element}
     */
    decorationElement: function()
    {
        return this._decorationElement;
    },

    /**
     * @override
     * @param {!WebInspector.ProfileHeader} profile
     */
    removeProfile: function(profile)
    {
        WebInspector.ProfileType.prototype.removeProfile.call(this, profile);
        if (this._recording && profile === this._lastProfileHeader)
            this._recording = false;
    },

    /**
     * @param {boolean=} forcePageReload
     */
    _updateDecorationElement: function(forcePageReload)
    {
        this._decorationElement.removeChildren();
        this._decorationElement.createChild("div", "warning-icon-small");
        this._decorationElement.appendChild(document.createTextNode(this._canvasAgentEnabled ? WebInspector.UIString("Canvas Profiler is enabled.") : WebInspector.UIString("Canvas Profiler is disabled.")));
        var button = this._decorationElement.createChild("button");
        button.type = "button";
        button.textContent = this._canvasAgentEnabled ? WebInspector.UIString("Disable") : WebInspector.UIString("Enable");
        button.addEventListener("click", this._onProfilerEnableButtonClick.bind(this, !this._canvasAgentEnabled), false);

        /**
         * @param {?Protocol.Error} error
         * @param {boolean} result
         */
        function hasUninstrumentedCanvasesCallback(error, result)
        {
            if (error || result)
                WebInspector.resourceTreeModel.reloadPage();
        }

        if (forcePageReload) {
            if (this._canvasAgentEnabled) {
                CanvasAgent.hasUninstrumentedCanvases(hasUninstrumentedCanvasesCallback.bind(this));
            } else {
                for (var frameId in this._framesWithCanvases) {
                    if (this._framesWithCanvases.hasOwnProperty(frameId)) {
                        WebInspector.resourceTreeModel.reloadPage();
                        break;
                    }
                }
            }
        }
    },

    /**
     * @param {boolean} enable
     */
    _onProfilerEnableButtonClick: function(enable)
    {
        if (this._canvasAgentEnabled === enable)
            return;

        /**
         * @param {?Protocol.Error} error
         * @this {WebInspector.CanvasProfileType}
         */
        function callback(error)
        {
            if (error)
                return;
            this._canvasAgentEnabled = enable;
            this._updateDecorationElement(true);
            this._dispatchViewUpdatedEvent();
        }
        if (enable)
            CanvasAgent.enable(callback.bind(this));
        else
            CanvasAgent.disable(callback.bind(this));
    },

    /**
     * @return {boolean}
     */
    _isSingleFrameMode: function()
    {
        return !this._capturingModeSelector.selectedOption().value;
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _frameAdded: function(event)
    {
        var frame = /** @type {!WebInspector.ResourceTreeFrame} */ (event.data);
        this._addFrame(frame);
    },

    /**
     * @param {!WebInspector.ResourceTreeFrame} frame
     */
    _addFrame: function(frame)
    {
        var frameId = frame.id;
        var option = document.createElement("option");
        option.text = frame.displayName();
        option.title = frame.url;
        option.value = frameId;

        this._frameOptions[frameId] = option;

        if (this._framesWithCanvases[frameId]) {
            this._frameSelector.addOption(option);
            this._dispatchViewUpdatedEvent();
        }
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _frameRemoved: function(event)
    {
        var frame = /** @type {!WebInspector.ResourceTreeFrame} */ (event.data);
        var frameId = frame.id;
        var option = this._frameOptions[frameId];
        if (option && this._framesWithCanvases[frameId]) {
            this._frameSelector.removeOption(option);
            this._dispatchViewUpdatedEvent();
        }
        delete this._frameOptions[frameId];
        delete this._framesWithCanvases[frameId];
    },

    /**
     * @param {string} frameId
     */
    _contextCreated: function(frameId)
    {
        if (this._framesWithCanvases[frameId])
            return;
        this._framesWithCanvases[frameId] = true;
        var option = this._frameOptions[frameId];
        if (option) {
            this._frameSelector.addOption(option);
            this._dispatchViewUpdatedEvent();
        }
    },

    /**
     * @param {!PageAgent.FrameId=} frameId
     * @param {!CanvasAgent.TraceLogId=} traceLogId
     */
    _traceLogsRemoved: function(frameId, traceLogId)
    {
        var sidebarElementsToDelete = [];
        var sidebarElements = /** @type {!Array.<!WebInspector.ProfileSidebarTreeElement>} */ ((this.treeElement && this.treeElement.children) || []);
        for (var i = 0, n = sidebarElements.length; i < n; ++i) {
            var header = /** @type {!WebInspector.CanvasProfileHeader} */ (sidebarElements[i].profile);
            if (!header)
                continue;
            if (frameId && frameId !== header.frameId())
                continue;
            if (traceLogId && traceLogId !== header.traceLogId())
                continue;
            sidebarElementsToDelete.push(sidebarElements[i]);
        }
        for (var i = 0, n = sidebarElementsToDelete.length; i < n; ++i)
            sidebarElementsToDelete[i].ondelete();
    },

    /**
     * @return {string|undefined}
     */
    _selectedFrameId: function()
    {
        var option = this._frameSelector.selectedOption();
        return option ? option.value : undefined;
    },

    _dispatchViewUpdatedEvent: function()
    {
        this._frameSelector.element.classList.toggle("hidden", this._frameSelector.size() <= 1);
        this.dispatchEventToListeners(WebInspector.ProfileType.Events.ViewUpdated);
    },

    /**
     * @override
     * @return {boolean}
     */
    isInstantProfile: function()
    {
        return this._isSingleFrameMode();
    },

    /**
     * @override
     * @return {boolean}
     */
    isEnabled: function()
    {
        return this._canvasAgentEnabled;
    },

    __proto__: WebInspector.ProfileType.prototype
}

/**
 * @constructor
 * @implements {CanvasAgent.Dispatcher}
 * @param {!WebInspector.CanvasProfileType} profileType
 */
WebInspector.CanvasDispatcher = function(profileType)
{
    this._profileType = profileType;
    InspectorBackend.registerCanvasDispatcher(this);
}

WebInspector.CanvasDispatcher.prototype = {
    /**
     * @param {string} frameId
     */
    contextCreated: function(frameId)
    {
        this._profileType._contextCreated(frameId);
    },

    /**
     * @param {!PageAgent.FrameId=} frameId
     * @param {!CanvasAgent.TraceLogId=} traceLogId
     */
    traceLogsRemoved: function(frameId, traceLogId)
    {
        this._profileType._traceLogsRemoved(frameId, traceLogId);
    }
}

/**
 * @constructor
 * @extends {WebInspector.ProfileHeader}
 * @param {!WebInspector.CanvasProfileType} type
 * @param {!CanvasAgent.TraceLogId=} traceLogId
 * @param {!PageAgent.FrameId=} frameId
 */
WebInspector.CanvasProfileHeader = function(type, traceLogId, frameId)
{
    WebInspector.ProfileHeader.call(this, type, WebInspector.UIString("Trace Log %d", type._nextProfileUid));
    /** @type {!CanvasAgent.TraceLogId} */
    this._traceLogId = traceLogId || "";
    this._frameId = frameId;
    this._alive = true;
    this._traceLogSize = 0;
    this._traceLogPlayer = traceLogId ? new WebInspector.CanvasTraceLogPlayerProxy(traceLogId) : null;
}

WebInspector.CanvasProfileHeader.prototype = {
    /**
     * @return {!CanvasAgent.TraceLogId}
     */
    traceLogId: function()
    {
        return this._traceLogId;
    },

    /**
     * @return {?WebInspector.CanvasTraceLogPlayerProxy}
     */
    traceLogPlayer: function()
    {
        return this._traceLogPlayer;
    },

    /**
     * @return {!PageAgent.FrameId|undefined}
     */
    frameId: function()
    {
        return this._frameId;
    },

    /**
     * @override
     * @return {!WebInspector.ProfileSidebarTreeElement}
     */
    createSidebarTreeElement: function()
    {
        return new WebInspector.ProfileSidebarTreeElement(this, "profile-sidebar-tree-item");
    },

    /**
     * @override
     * @return {!WebInspector.CanvasProfileView}
     */
    createView: function()
    {
        return new WebInspector.CanvasProfileView(this);
    },

    /**
     * @override
     */
    dispose: function()
    {
        if (this._traceLogPlayer)
            this._traceLogPlayer.dispose();
        clearTimeout(this._requestStatusTimer);
        this._alive = false;
    },

    /**
     * @param {!CanvasAgent.TraceLog=} traceLog
     */
    _updateCapturingStatus: function(traceLog)
    {
        if (!this._traceLogId)
            return;

        if (traceLog) {
            this._alive = traceLog.alive;
            this._traceLogSize = traceLog.totalAvailableCalls;
        }

        var subtitle = this._alive ? WebInspector.UIString("Capturing\u2026 %d calls", this._traceLogSize) : WebInspector.UIString("Captured %d calls", this._traceLogSize);
        this.updateStatus(subtitle, this._alive);

        if (this._alive) {
            clearTimeout(this._requestStatusTimer);
            this._requestStatusTimer = setTimeout(this._requestCapturingStatus.bind(this), WebInspector.CanvasProfileView.TraceLogPollingInterval);
        }
    },

    _requestCapturingStatus: function()
    {
        /**
         * @param {?CanvasAgent.TraceLog} traceLog
         * @this {WebInspector.CanvasProfileHeader}
         */
        function didReceiveTraceLog(traceLog)
        {
            if (!traceLog)
                return;
            this._alive = traceLog.alive;
            this._traceLogSize = traceLog.totalAvailableCalls;
            this._updateCapturingStatus();
        }
        this._traceLogPlayer.getTraceLog(0, 0, didReceiveTraceLog.bind(this));
    },

    __proto__: WebInspector.ProfileHeader.prototype
}

WebInspector.CanvasProfileDataGridHelper = {
    /**
     * @param {!CanvasAgent.CallArgument} callArgument
     * @return {!Element}
     */
    createCallArgumentElement: function(callArgument)
    {
        if (callArgument.enumName)
            return WebInspector.CanvasProfileDataGridHelper.createEnumValueElement(callArgument.enumName, +callArgument.description);
        var element = document.createElement("span");
        element.className = "canvas-call-argument";
        var description = callArgument.description;
        if (callArgument.type === "string") {
            const maxStringLength = 150;
            element.createTextChild("\"");
            element.createChild("span", "canvas-formatted-string").textContent = description.trimMiddle(maxStringLength);
            element.createTextChild("\"");
            element.__suppressPopover = (description.length <= maxStringLength && !/[\r\n]/.test(description));
            if (!element.__suppressPopover)
                element.__evalResult = WebInspector.RemoteObject.fromPrimitiveValue(description);
        } else {
            var type = callArgument.subtype || callArgument.type;
            if (type) {
                element.classList.add("canvas-formatted-" + type);
                if (["null", "undefined", "boolean", "number"].indexOf(type) >= 0)
                    element.__suppressPopover = true;
            }
            element.textContent = description;
            if (callArgument.remoteObject)
                element.__evalResult = WebInspector.RemoteObject.fromPayload(callArgument.remoteObject);
        }
        if (callArgument.resourceId) {
            element.classList.add("canvas-formatted-resource");
            element.__resourceId = callArgument.resourceId;
        }
        return element;
    },

    /**
     * @param {string} enumName
     * @param {number} enumValue
     * @return {!Element}
     */
    createEnumValueElement: function(enumName, enumValue)
    {
        var element = document.createElement("span");
        element.className = "canvas-call-argument canvas-formatted-number";
        element.textContent = enumName;
        element.__evalResult = WebInspector.RemoteObject.fromPrimitiveValue(enumValue);
        return element;
    }
}

/**
 * @extends {WebInspector.Object}
 * @constructor
 * @param {!CanvasAgent.TraceLogId} traceLogId
 */
WebInspector.CanvasTraceLogPlayerProxy = function(traceLogId)
{
    this._traceLogId = traceLogId;
    /** @type {!Object.<string, !CanvasAgent.ResourceState>} */
    this._currentResourceStates = {};
    /** @type {?CanvasAgent.ResourceId} */
    this._defaultResourceId = null;
}

/** @enum {string} */
WebInspector.CanvasTraceLogPlayerProxy.Events = {
    CanvasTraceLogReceived: "CanvasTraceLogReceived",
    CanvasReplayStateChanged: "CanvasReplayStateChanged",
    CanvasResourceStateReceived: "CanvasResourceStateReceived",
}

WebInspector.CanvasTraceLogPlayerProxy.prototype = {
    /**
     * @param {number|undefined} startOffset
     * @param {number|undefined} maxLength
     * @param {function(?CanvasAgent.TraceLog):void} userCallback
     */
    getTraceLog: function(startOffset, maxLength, userCallback)
    {
        /**
         * @param {?Protocol.Error} error
         * @param {!CanvasAgent.TraceLog} traceLog
         * @this {WebInspector.CanvasTraceLogPlayerProxy}
         */
        function callback(error, traceLog)
        {
            if (error || !traceLog) {
                userCallback(null);
                return;
            }
            userCallback(traceLog);
            this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasTraceLogReceived, traceLog);
        }
        CanvasAgent.getTraceLog(this._traceLogId, startOffset, maxLength, callback.bind(this));
    },

    dispose: function()
    {
        this._currentResourceStates = {};
        CanvasAgent.dropTraceLog(this._traceLogId);
        this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasReplayStateChanged);
    },

    /**
     * @param {?CanvasAgent.ResourceId} resourceId
     * @param {function(?CanvasAgent.ResourceState):void} userCallback
     */
    getResourceState: function(resourceId, userCallback)
    {
        resourceId = resourceId || this._defaultResourceId;
        if (!resourceId) {
            userCallback(null); // Has not been replayed yet.
            return;
        }
        var effectiveResourceId = /** @type {!CanvasAgent.ResourceId} */ (resourceId);
        if (this._currentResourceStates[effectiveResourceId]) {
            userCallback(this._currentResourceStates[effectiveResourceId]);
            return;
        }

        /**
         * @param {?Protocol.Error} error
         * @param {!CanvasAgent.ResourceState} resourceState
         * @this {WebInspector.CanvasTraceLogPlayerProxy}
         */
        function callback(error, resourceState)
        {
            if (error || !resourceState) {
                userCallback(null);
                return;
            }
            this._currentResourceStates[effectiveResourceId] = resourceState;
            userCallback(resourceState);
            this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasResourceStateReceived, resourceState);
        }
        CanvasAgent.getResourceState(this._traceLogId, effectiveResourceId, callback.bind(this));
    },

    /**
     * @param {number} index
     * @param {function(?CanvasAgent.ResourceState, number):void} userCallback
     */
    replayTraceLog: function(index, userCallback)
    {
        /**
         * @param {?Protocol.Error} error
         * @param {!CanvasAgent.ResourceState} resourceState
         * @param {number} replayTime
         * @this {WebInspector.CanvasTraceLogPlayerProxy}
         */
        function callback(error, resourceState, replayTime)
        {
            this._currentResourceStates = {};
            if (error) {
                userCallback(null, replayTime);
            } else {
                this._defaultResourceId = resourceState.id;
                this._currentResourceStates[resourceState.id] = resourceState;
                userCallback(resourceState, replayTime);
            }
            this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasReplayStateChanged);
            if (!error)
                this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasResourceStateReceived, resourceState);
        }
        CanvasAgent.replayTraceLog(this._traceLogId, index, callback.bind(this));
    },

    clearResourceStates: function()
    {
        this._currentResourceStates = {};
        this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasReplayStateChanged);
    },

    __proto__: WebInspector.Object.prototype
}
