/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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
 */
WebInspector.TimelineUIUtils = function() { }

WebInspector.TimelineUIUtils.categories = function()
{
    if (WebInspector.TimelineUIUtils._categories)
        return WebInspector.TimelineUIUtils._categories;
    WebInspector.TimelineUIUtils._categories = {
        loading: new WebInspector.TimelineCategory("loading", WebInspector.UIString("Loading"), 0, "hsl(214, 53%, 58%)", "hsl(214, 67%, 90%)", "hsl(214, 67%, 74%)", "hsl(214, 67%, 66%)"),
        scripting: new WebInspector.TimelineCategory("scripting", WebInspector.UIString("Scripting"), 1, "hsl(43, 68%, 53%)", "hsl(43, 83%, 90%)", "hsl(43, 83%, 72%)", "hsl(43, 83%, 64%) "),
        rendering: new WebInspector.TimelineCategory("rendering", WebInspector.UIString("Rendering"), 2, "hsl(256, 50%, 60%)", "hsl(256, 67%, 90%)", "hsl(256, 67%, 76%)", "hsl(256, 67%, 70%)"),
        painting: new WebInspector.TimelineCategory("painting", WebInspector.UIString("Painting"), 2, "hsl(109, 33%, 47%)", "hsl(109, 33%, 90%)", "hsl(109, 33%, 64%)", "hsl(109, 33%, 55%)"),
        other: new WebInspector.TimelineCategory("other", WebInspector.UIString("Other"), -1, "hsl(0, 0%, 73%)", "hsl(0, 0%, 90%)", "hsl(0, 0%, 87%)", "hsl(0, 0%, 79%)"),
        idle: new WebInspector.TimelineCategory("idle", WebInspector.UIString("Idle"), -1, "hsl(0, 0%, 87%)", "hsl(0, 100%, 100%)", "hsl(0, 100%, 100%)", "hsl(0, 100%, 100%)")
    };
    return WebInspector.TimelineUIUtils._categories;
};

/**
 * @return {!Object.<string, !{title: string, category: !WebInspector.TimelineCategory}>}
 */
WebInspector.TimelineUIUtils._initRecordStyles = function()
{
    if (WebInspector.TimelineUIUtils._recordStylesMap)
        return WebInspector.TimelineUIUtils._recordStylesMap;

    var recordTypes = WebInspector.TimelineModel.RecordType;
    var categories = WebInspector.TimelineUIUtils.categories();

    var recordStyles = {};
    recordStyles[recordTypes.Root] = { title: "#root", category: categories["loading"] };
    recordStyles[recordTypes.Program] = { title: WebInspector.UIString("Other"), category: categories["other"] };
    recordStyles[recordTypes.EventDispatch] = { title: WebInspector.UIString("Event"), category: categories["scripting"] };
    recordStyles[recordTypes.BeginFrame] = { title: WebInspector.UIString("Frame Start"), category: categories["rendering"] };
    recordStyles[recordTypes.ScheduleStyleRecalculation] = { title: WebInspector.UIString("Schedule Style Recalculation"), category: categories["rendering"] };
    recordStyles[recordTypes.RecalculateStyles] = { title: WebInspector.UIString("Recalculate Style"), category: categories["rendering"] };
    recordStyles[recordTypes.InvalidateLayout] = { title: WebInspector.UIString("Invalidate Layout"), category: categories["rendering"] };
    recordStyles[recordTypes.Layout] = { title: WebInspector.UIString("Layout"), category: categories["rendering"] };
    recordStyles[recordTypes.AutosizeText] = { title: WebInspector.UIString("Autosize Text"), category: categories["rendering"] };
    recordStyles[recordTypes.PaintSetup] = { title: WebInspector.UIString("Paint Setup"), category: categories["painting"] };
    recordStyles[recordTypes.Paint] = { title: WebInspector.UIString("Paint"), category: categories["painting"] };
    recordStyles[recordTypes.Rasterize] = { title: WebInspector.UIString("Paint"), category: categories["painting"] };
    recordStyles[recordTypes.ScrollLayer] = { title: WebInspector.UIString("Scroll"), category: categories["rendering"] };
    recordStyles[recordTypes.DecodeImage] = { title: WebInspector.UIString("Image Decode"), category: categories["painting"] };
    recordStyles[recordTypes.ResizeImage] = { title: WebInspector.UIString("Image Resize"), category: categories["painting"] };
    recordStyles[recordTypes.CompositeLayers] = { title: WebInspector.UIString("Composite Layers"), category: categories["painting"] };
    recordStyles[recordTypes.ParseHTML] = { title: WebInspector.UIString("Parse HTML"), category: categories["loading"] };
    recordStyles[recordTypes.TimerInstall] = { title: WebInspector.UIString("Install Timer"), category: categories["scripting"] };
    recordStyles[recordTypes.TimerRemove] = { title: WebInspector.UIString("Remove Timer"), category: categories["scripting"] };
    recordStyles[recordTypes.TimerFire] = { title: WebInspector.UIString("Timer Fired"), category: categories["scripting"] };
    recordStyles[recordTypes.XHRReadyStateChange] = { title: WebInspector.UIString("XHR Ready State Change"), category: categories["scripting"] };
    recordStyles[recordTypes.XHRLoad] = { title: WebInspector.UIString("XHR Load"), category: categories["scripting"] };
    recordStyles[recordTypes.EvaluateScript] = { title: WebInspector.UIString("Evaluate Script"), category: categories["scripting"] };
    recordStyles[recordTypes.ResourceSendRequest] = { title: WebInspector.UIString("Send Request"), category: categories["loading"] };
    recordStyles[recordTypes.ResourceReceiveResponse] = { title: WebInspector.UIString("Receive Response"), category: categories["loading"] };
    recordStyles[recordTypes.ResourceFinish] = { title: WebInspector.UIString("Finish Loading"), category: categories["loading"] };
    recordStyles[recordTypes.FunctionCall] = { title: WebInspector.UIString("Function Call"), category: categories["scripting"] };
    recordStyles[recordTypes.ResourceReceivedData] = { title: WebInspector.UIString("Receive Data"), category: categories["loading"] };
    recordStyles[recordTypes.GCEvent] = { title: WebInspector.UIString("GC Event"), category: categories["scripting"] };
    recordStyles[recordTypes.MarkDOMContent] = { title: WebInspector.UIString("DOMContentLoaded event"), category: categories["scripting"] };
    recordStyles[recordTypes.MarkLoad] = { title: WebInspector.UIString("Load event"), category: categories["scripting"] };
    recordStyles[recordTypes.MarkFirstPaint] = { title: WebInspector.UIString("First paint"), category: categories["painting"] };
    recordStyles[recordTypes.TimeStamp] = { title: WebInspector.UIString("Stamp"), category: categories["scripting"] };
    recordStyles[recordTypes.ConsoleTime] = { title: WebInspector.UIString("Console Time"), category: categories["scripting"] };
    recordStyles[recordTypes.ScheduleResourceRequest] = { title: WebInspector.UIString("Schedule Request"), category: categories["loading"] };
    recordStyles[recordTypes.RequestAnimationFrame] = { title: WebInspector.UIString("Request Animation Frame"), category: categories["scripting"] };
    recordStyles[recordTypes.CancelAnimationFrame] = { title: WebInspector.UIString("Cancel Animation Frame"), category: categories["scripting"] };
    recordStyles[recordTypes.FireAnimationFrame] = { title: WebInspector.UIString("Animation Frame Fired"), category: categories["scripting"] };
    recordStyles[recordTypes.WebSocketCreate] = { title: WebInspector.UIString("Create WebSocket"), category: categories["scripting"] };
    recordStyles[recordTypes.WebSocketSendHandshakeRequest] = { title: WebInspector.UIString("Send WebSocket Handshake"), category: categories["scripting"] };
    recordStyles[recordTypes.WebSocketReceiveHandshakeResponse] = { title: WebInspector.UIString("Receive WebSocket Handshake"), category: categories["scripting"] };
    recordStyles[recordTypes.WebSocketDestroy] = { title: WebInspector.UIString("Destroy WebSocket"), category: categories["scripting"] };
    recordStyles[recordTypes.EmbedderCallback] = { title: WebInspector.UIString("Embedder Callback"), category: categories["scripting"] };

    WebInspector.TimelineUIUtils._recordStylesMap = recordStyles;
    return recordStyles;
}

/**
 * @param {!WebInspector.TimelineModel.Record} record
 * @return {!{title: string, category: !WebInspector.TimelineCategory}}
 */
WebInspector.TimelineUIUtils.recordStyle = function(record)
{
    var recordStyles = WebInspector.TimelineUIUtils._initRecordStyles();
    var result = recordStyles[record.type];
    if (!result) {
        result = {
            title: WebInspector.UIString("Unknown: %s", record.type),
            category: WebInspector.TimelineUIUtils.categories()["other"]
        };
        recordStyles[record.type] = result;
    }
    return result;
}

/**
 * @param {!WebInspector.TimelineModel.Record} record
 * @return {!WebInspector.TimelineCategory}
 */
WebInspector.TimelineUIUtils.categoryForRecord = function(record)
{
    return WebInspector.TimelineUIUtils.recordStyle(record).category;
}

/**
 * @param {!WebInspector.TimelineModel.Record} record
 */
WebInspector.TimelineUIUtils.isEventDivider = function(record)
{
    var recordTypes = WebInspector.TimelineModel.RecordType;
    if (record.type === recordTypes.TimeStamp)
        return true;
    if (record.type === recordTypes.MarkFirstPaint)
        return true;
    if (record.type === recordTypes.MarkDOMContent || record.type === recordTypes.MarkLoad) {
        if (record.data && ((typeof record.data.isMainFrame) === "boolean"))
            return record.data.isMainFrame;
    }
    return false;
}

/**
 * @param {string=} recordType
 * @return {boolean}
 */
WebInspector.TimelineUIUtils.needsPreviewElement = function(recordType)
{
    if (!recordType)
        return false;
    const recordTypes = WebInspector.TimelineModel.RecordType;
    switch (recordType) {
    case recordTypes.ScheduleResourceRequest:
    case recordTypes.ResourceSendRequest:
    case recordTypes.ResourceReceiveResponse:
    case recordTypes.ResourceReceivedData:
    case recordTypes.ResourceFinish:
        return true;
    default:
        return false;
    }
}

/**
 * @param {string} recordType
 * @param {string=} title
 */
WebInspector.TimelineUIUtils.createEventDivider = function(recordType, title)
{
    var eventDivider = document.createElement("div");
    eventDivider.className = "resources-event-divider";
    var recordTypes = WebInspector.TimelineModel.RecordType;

    if (recordType === recordTypes.MarkDOMContent)
        eventDivider.className += " resources-blue-divider";
    else if (recordType === recordTypes.MarkLoad)
        eventDivider.className += " resources-red-divider";
    else if (recordType === recordTypes.MarkFirstPaint)
        eventDivider.className += " resources-green-divider";
    else if (recordType === recordTypes.TimeStamp)
        eventDivider.className += " resources-orange-divider";
    else if (recordType === recordTypes.BeginFrame)
        eventDivider.className += " timeline-frame-divider";

    if (title)
        eventDivider.title = title;

    return eventDivider;
}


/**
 * @param {!WebInspector.TimelineModel} model
 * @param {!{name: string, tasks: !Array.<!{startTime: number, endTime: number}>, firstTaskIndex: number, lastTaskIndex: number}} info
 * @return {!Element}
 */
WebInspector.TimelineUIUtils.generateMainThreadBarPopupContent = function(model, info)
{
    var firstTaskIndex = info.firstTaskIndex;
    var lastTaskIndex = info.lastTaskIndex;
    var tasks = info.tasks;
    var messageCount = lastTaskIndex - firstTaskIndex + 1;
    var cpuTime = 0;

    for (var i = firstTaskIndex; i <= lastTaskIndex; ++i) {
        var task = tasks[i];
        cpuTime += task.endTime - task.startTime;
    }
    var startTime = tasks[firstTaskIndex].startTime;
    var endTime = tasks[lastTaskIndex].endTime;
    var duration = endTime - startTime;

    var contentHelper = new WebInspector.TimelinePopupContentHelper(info.name);
    var durationText = WebInspector.UIString("%s (at %s)", Number.millisToString(duration, true),
        Number.millisToString(startTime - model.minimumRecordTime(), true));
    contentHelper.appendTextRow(WebInspector.UIString("Duration"), durationText);
    contentHelper.appendTextRow(WebInspector.UIString("CPU time"), Number.millisToString(cpuTime, true));
    contentHelper.appendTextRow(WebInspector.UIString("Message Count"), messageCount);
    return contentHelper.contentTable();
}

/**
 * @param {!WebInspector.TimelineModel.Record} record
 * @return {string}
 */
WebInspector.TimelineUIUtils.recordTitle = function(record)
{
    if (record.type === WebInspector.TimelineModel.RecordType.TimeStamp)
        return record.data["message"];
    if (WebInspector.TimelineUIUtils.isEventDivider(record)) {
        var startTime = Number.millisToString(record.startTimeOffset);
        return WebInspector.UIString("%s at %s", WebInspector.TimelineUIUtils.recordStyle(record).title, startTime, true);
    }
    return WebInspector.TimelineUIUtils.recordStyle(record).title;
}

/**
 * @param {!Object} total
 * @param {!Object} addend
 */
WebInspector.TimelineUIUtils.aggregateTimeByCategory = function(total, addend)
{
    for (var category in addend)
        total[category] = (total[category] || 0) + addend[category];
}

/**
 * @param {!Object} total
 * @param {!WebInspector.TimelineModel.Record} record
 */
WebInspector.TimelineUIUtils.aggregateTimeForRecord = function(total, record)
{
    var childrenTime = 0;
    var children = record.children;
    for (var i = 0; i < children.length; ++i) {
        WebInspector.TimelineUIUtils.aggregateTimeForRecord(total, children[i]);
        childrenTime += children[i].endTime - children[i].startTime;
    }
    var categoryName = WebInspector.TimelineUIUtils.recordStyle(record).category.name;
    var ownTime = record.endTime - record.startTime - childrenTime;
    total[categoryName] = (total[categoryName] || 0) + ownTime;
}

/**
 * @param {!Object} aggregatedStats
 */
WebInspector.TimelineUIUtils._generateAggregatedInfo = function(aggregatedStats)
{
    var cell = document.createElement("span");
    cell.className = "timeline-aggregated-info";
    for (var index in aggregatedStats) {
        var label = document.createElement("div");
        label.className = "timeline-aggregated-category timeline-" + index;
        cell.appendChild(label);
        var text = document.createElement("span");
        text.textContent = Number.millisToString(aggregatedStats[index], true);
        cell.appendChild(text);
    }
    return cell;
}

/**
 * @param {!Object} aggregatedStats
 * @param {!WebInspector.TimelineCategory=} selfCategory
 * @param {number=} selfTime
 * @return {!Element}
 */
WebInspector.TimelineUIUtils.generatePieChart = function(aggregatedStats, selfCategory, selfTime)
{
    var element = document.createElement("div");
    element.className = "timeline-aggregated-info";

    var total = 0;
    for (var categoryName in aggregatedStats)
        total += aggregatedStats[categoryName];

    function formatter(value)
    {
        return Number.millisToString(value, true);
    }
    var pieChart = new WebInspector.PieChart(total, formatter);
    element.appendChild(pieChart.element);
    var footerElement = element.createChild("div", "timeline-aggregated-info-legend");

    // In case of self time, first add self, then children of the same category.
    if (selfCategory && selfTime) {
        // Self.
        pieChart.addSlice(selfTime, selfCategory.fillColorStop1);
        var rowElement = footerElement.createChild("div");
        rowElement.createChild("div", "timeline-aggregated-category timeline-" + selfCategory.name);
        rowElement.createTextChild(WebInspector.UIString("%s %s (Self)", formatter(selfTime), selfCategory.title));

        // Children of the same category.
        var categoryTime = aggregatedStats[selfCategory.name];
        var value = categoryTime - selfTime;
        if (value > 0) {
            pieChart.addSlice(value, selfCategory.fillColorStop0);
            rowElement = footerElement.createChild("div");
            rowElement.createChild("div", "timeline-aggregated-category timeline-" + selfCategory.name);
            rowElement.createTextChild(WebInspector.UIString("%s %s (Children)", formatter(value), selfCategory.title));
        }
    }

    // Add other categories.
    for (var categoryName in WebInspector.TimelineUIUtils.categories()) {
        var category = WebInspector.TimelineUIUtils.categories()[categoryName];
         if (category === selfCategory)
             continue;
         var value = aggregatedStats[category.name];
         if (!value)
             continue;
         pieChart.addSlice(value, category.fillColorStop0);
         var rowElement = footerElement.createChild("div");
         rowElement.createChild("div", "timeline-aggregated-category timeline-" + category.name);
         rowElement.createTextChild(WebInspector.UIString("%s %s", formatter(value), category.title));
    }
    return element;
}

WebInspector.TimelineUIUtils.generatePopupContentForFrame = function(frame)
{
    var contentHelper = new WebInspector.TimelinePopupContentHelper(WebInspector.UIString("Frame"));
    var durationInMillis = frame.endTime - frame.startTime;
    var durationText = WebInspector.UIString("%s (at %s)", Number.millisToString(frame.endTime - frame.startTime, true),
        Number.millisToString(frame.startTimeOffset, true));
    contentHelper.appendTextRow(WebInspector.UIString("Duration"), durationText);
    contentHelper.appendTextRow(WebInspector.UIString("FPS"), Math.floor(1000 / durationInMillis));
    contentHelper.appendTextRow(WebInspector.UIString("CPU time"), Number.millisToString(frame.cpuTime, true));
    contentHelper.appendElementRow(WebInspector.UIString("Aggregated Time"),
        WebInspector.TimelineUIUtils._generateAggregatedInfo(frame.timeByCategory));
    if (WebInspector.experimentsSettings.layersPanel.isEnabled() && frame.layerTree) {
        var layerTreeSnapshot = new WebInspector.LayerTreeSnapshot(frame.layerTree);
        contentHelper.appendElementRow(WebInspector.UIString("Layer tree"),
                                       WebInspector.Linkifier.linkifyUsingRevealer(layerTreeSnapshot, WebInspector.UIString("show")));
    }
    return contentHelper.contentTable();
}

/**
 * @param {!WebInspector.FrameStatistics} statistics
 */
WebInspector.TimelineUIUtils.generatePopupContentForFrameStatistics = function(statistics)
{
    /**
     * @param {number} time
     */
    function formatTimeAndFPS(time)
    {
        return WebInspector.UIString("%s (%.0f FPS)", Number.millisToString(time, true), 1 / time);
    }

    var contentHelper = new WebInspector.TimelineDetailsContentHelper(new WebInspector.Linkifier(), false);
    contentHelper.appendTextRow(WebInspector.UIString("Minimum Time"), formatTimeAndFPS(statistics.minDuration));
    contentHelper.appendTextRow(WebInspector.UIString("Average Time"), formatTimeAndFPS(statistics.average));
    contentHelper.appendTextRow(WebInspector.UIString("Maximum Time"), formatTimeAndFPS(statistics.maxDuration));
    contentHelper.appendTextRow(WebInspector.UIString("Standard Deviation"), Number.millisToString(statistics.stddev, true));

    return contentHelper.element;
}

/**
 * @param {!CanvasRenderingContext2D} context
 * @param {number} width
 * @param {number} height
 * @param {string} color0
 * @param {string} color1
 * @param {string} color2
 */
WebInspector.TimelineUIUtils.createFillStyle = function(context, width, height, color0, color1, color2)
{
    var gradient = context.createLinearGradient(0, 0, width, height);
    gradient.addColorStop(0, color0);
    gradient.addColorStop(0.25, color1);
    gradient.addColorStop(0.75, color1);
    gradient.addColorStop(1, color2);
    return gradient;
}

/**
 * @param {!CanvasRenderingContext2D} context
 * @param {number} width
 * @param {number} height
 * @param {!WebInspector.TimelineCategory} category
 */
WebInspector.TimelineUIUtils.createFillStyleForCategory = function(context, width, height, category)
{
    return WebInspector.TimelineUIUtils.createFillStyle(context, width, height, category.fillColorStop0, category.fillColorStop1, category.borderColor);
}

/**
 * @param {!WebInspector.TimelineCategory} category
 */
WebInspector.TimelineUIUtils.createStyleRuleForCategory = function(category)
{
    var selector = ".timeline-category-" + category.name + " .timeline-graph-bar, " +
        ".panel.timeline .timeline-filters-header .filter-checkbox-filter.filter-checkbox-filter-" + category.name + " .checkbox-filter-checkbox, " +
        ".popover .timeline-" + category.name + ", " +
        ".timeline-details-view .timeline-" + category.name + ", " +
        ".timeline-category-" + category.name + " .timeline-tree-icon"

    return selector + " { background-image: linear-gradient(" +
       category.fillColorStop0 + ", " + category.fillColorStop1 + " 25%, " + category.fillColorStop1 + " 25%, " + category.fillColorStop1 + ");" +
       " border-color: " + category.borderColor +
       "}";
}

/**
 * @param {!WebInspector.TimelineModel.Record} record
 * @param {!WebInspector.Linkifier} linkifier
 * @param {function(!DocumentFragment)} callback
 */
WebInspector.TimelineUIUtils.generatePopupContent = function(record, linkifier, callback)
{
    var imageElement = /** @type {?Element} */ (record.getUserObject("TimelineUIUtils::preview-element") || null);
    var relatedNode = /** @type {?WebInspector.DOMNode} */ (record.getUserObject("TimelineUIUtils::related-node") || null);

    var barrier = new CallbackBarrier();
    if (!imageElement && WebInspector.TimelineUIUtils.needsPreviewElement(record.type))
        WebInspector.DOMPresentationUtils.buildImagePreviewContents(record.url, false, barrier.createCallback(saveImage));
    if (!relatedNode && record.relatedBackendNodeId())
        WebInspector.domAgent.pushNodesByBackendIdsToFrontend([record.relatedBackendNodeId()], barrier.createCallback(saveNode));
    barrier.callWhenDone(callbackWrapper);

    /**
     * @param {!Element=} element
     */
    function saveImage(element)
    {
        imageElement = element || null;
        record.setUserObject("TimelineUIUtils::preview-element", element);
    }

    /**
     * @param {?Array.<!DOMAgent.NodeId>} nodeIds
     */
    function saveNode(nodeIds)
    {
        if (nodeIds !== null) {
            relatedNode = WebInspector.domAgent.nodeForId(nodeIds[0]);
            record.setUserObject("TimelineUIUtils::related-node", relatedNode);
        }
    }

    function callbackWrapper()
    {
        callback(WebInspector.TimelineUIUtils._generatePopupContentSynchronously(record, linkifier, imageElement, relatedNode));
    }
}

/**
 * @param {!WebInspector.TimelineModel.Record} record
 * @param {!WebInspector.Linkifier} linkifier
 * @param {?Element} imagePreviewElement
 * @param {?WebInspector.DOMNode} relatedNode
 * @return {!DocumentFragment}
 */
WebInspector.TimelineUIUtils._generatePopupContentSynchronously = function(record, linkifier, imagePreviewElement, relatedNode)
{
    var fragment = document.createDocumentFragment();
    if (record.children.length)
        fragment.appendChild(WebInspector.TimelineUIUtils.generatePieChart(record.aggregatedStats, record.category, record.selfTime));
    else
        fragment.appendChild(WebInspector.TimelineUIUtils.generatePieChart(record.aggregatedStats));

    const recordTypes = WebInspector.TimelineModel.RecordType;

    // The messages may vary per record type;
    var callSiteStackTraceLabel;
    var callStackLabel;
    var relatedNodeLabel;

    var contentHelper = new WebInspector.TimelineDetailsContentHelper(linkifier, true);
    contentHelper.appendTextRow(WebInspector.UIString("Self Time"), Number.millisToString(record.selfTime, true));
    contentHelper.appendTextRow(WebInspector.UIString("Start Time"), Number.millisToString(record.startTimeOffset));

    switch (record.type) {
        case recordTypes.GCEvent:
            contentHelper.appendTextRow(WebInspector.UIString("Collected"), Number.bytesToString(record.data["usedHeapSizeDelta"]));
            break;
        case recordTypes.TimerFire:
            callSiteStackTraceLabel = WebInspector.UIString("Timer installed");
            // Fall-through intended.

        case recordTypes.TimerInstall:
        case recordTypes.TimerRemove:
            contentHelper.appendTextRow(WebInspector.UIString("Timer ID"), record.data["timerId"]);
            if (typeof record.timeout === "number") {
                contentHelper.appendTextRow(WebInspector.UIString("Timeout"), Number.millisToString(record.timeout));
                contentHelper.appendTextRow(WebInspector.UIString("Repeats"), !record.singleShot);
            }
            break;
        case recordTypes.FireAnimationFrame:
            callSiteStackTraceLabel = WebInspector.UIString("Animation frame requested");
            contentHelper.appendTextRow(WebInspector.UIString("Callback ID"), record.data["id"]);
            break;
        case recordTypes.FunctionCall:
            if (record.scriptName)
                contentHelper.appendLocationRow(WebInspector.UIString("Location"), record.scriptName, record.scriptLine);
            break;
        case recordTypes.ScheduleResourceRequest:
        case recordTypes.ResourceSendRequest:
        case recordTypes.ResourceReceiveResponse:
        case recordTypes.ResourceReceivedData:
        case recordTypes.ResourceFinish:
            contentHelper.appendElementRow(WebInspector.UIString("Resource"), WebInspector.linkifyResourceAsNode(record.url));
            if (imagePreviewElement)
                contentHelper.appendElementRow(WebInspector.UIString("Preview"), imagePreviewElement);
            if (record.data["requestMethod"])
                contentHelper.appendTextRow(WebInspector.UIString("Request Method"), record.data["requestMethod"]);
            if (typeof record.data["statusCode"] === "number")
                contentHelper.appendTextRow(WebInspector.UIString("Status Code"), record.data["statusCode"]);
            if (record.data["mimeType"])
                contentHelper.appendTextRow(WebInspector.UIString("MIME Type"), record.data["mimeType"]);
            if (record.data["encodedDataLength"])
                contentHelper.appendTextRow(WebInspector.UIString("Encoded Data Length"), WebInspector.UIString("%d Bytes", record.data["encodedDataLength"]));
            break;
        case recordTypes.EvaluateScript:
            if (record.data && record.url)
                contentHelper.appendLocationRow(WebInspector.UIString("Script"), record.url, record.data["lineNumber"]);
            break;
        case recordTypes.Paint:
            var clip = record.data["clip"];
            if (clip) {
                contentHelper.appendTextRow(WebInspector.UIString("Location"), WebInspector.UIString("(%d, %d)", clip[0], clip[1]));
                var clipWidth = WebInspector.TimelineUIUtils._quadWidth(clip);
                var clipHeight = WebInspector.TimelineUIUtils._quadHeight(clip);
                contentHelper.appendTextRow(WebInspector.UIString("Dimensions"), WebInspector.UIString("%d × %d", clipWidth, clipHeight));
            } else {
                // Backward compatibility: older version used x, y, width, height fields directly in data.
                if (typeof record.data["x"] !== "undefined" && typeof record.data["y"] !== "undefined")
                    contentHelper.appendTextRow(WebInspector.UIString("Location"), WebInspector.UIString("(%d, %d)", record.data["x"], record.data["y"]));
                if (typeof record.data["width"] !== "undefined" && typeof record.data["height"] !== "undefined")
                    contentHelper.appendTextRow(WebInspector.UIString("Dimensions"), WebInspector.UIString("%d\u2009\u00d7\u2009%d", record.data["width"], record.data["height"]));
            }
            // Fall-through intended.

        case recordTypes.PaintSetup:
        case recordTypes.Rasterize:
        case recordTypes.ScrollLayer:
            relatedNodeLabel = WebInspector.UIString("Layer root");
            break;
        case recordTypes.AutosizeText:
            relatedNodeLabel = WebInspector.UIString("Root node");
            break;
        case recordTypes.DecodeImage:
        case recordTypes.ResizeImage:
            relatedNodeLabel = WebInspector.UIString("Image element");
            if (record.url)
                contentHelper.appendElementRow(WebInspector.UIString("Image URL"), WebInspector.linkifyResourceAsNode(record.url));
            break;
        case recordTypes.RecalculateStyles: // We don't want to see default details.
            if (record.data["elementCount"])
                contentHelper.appendTextRow(WebInspector.UIString("Elements affected"), record.data["elementCount"]);
            callStackLabel = WebInspector.UIString("Styles recalculation forced");
            break;
        case recordTypes.Layout:
            if (record.data["dirtyObjects"])
                contentHelper.appendTextRow(WebInspector.UIString("Nodes that need layout"), record.data["dirtyObjects"]);
            if (record.data["totalObjects"])
                contentHelper.appendTextRow(WebInspector.UIString("Layout tree size"), record.data["totalObjects"]);
            if (typeof record.data["partialLayout"] === "boolean") {
                contentHelper.appendTextRow(WebInspector.UIString("Layout scope"),
                   record.data["partialLayout"] ? WebInspector.UIString("Partial") : WebInspector.UIString("Whole document"));
            }
            callSiteStackTraceLabel = WebInspector.UIString("Layout invalidated");
            callStackLabel = WebInspector.UIString("Layout forced");
            relatedNodeLabel = WebInspector.UIString("Layout root");
            break;
        case recordTypes.ConsoleTime:
            contentHelper.appendTextRow(WebInspector.UIString("Message"), record.data["message"]);
            break;
        case recordTypes.WebSocketCreate:
        case recordTypes.WebSocketSendHandshakeRequest:
        case recordTypes.WebSocketReceiveHandshakeResponse:
        case recordTypes.WebSocketDestroy:
            if (typeof record.webSocketURL !== "undefined")
                contentHelper.appendTextRow(WebInspector.UIString("URL"), record.webSocketURL);
            if (typeof record.webSocketProtocol !== "undefined")
                contentHelper.appendTextRow(WebInspector.UIString("WebSocket Protocol"), record.webSocketProtocol);
            if (typeof record.data["message"] !== "undefined")
                contentHelper.appendTextRow(WebInspector.UIString("Message"), record.data["message"]);
            break;
        case recordTypes.EmbedderCallback:
            contentHelper.appendTextRow(WebInspector.UIString("Callback Function"), record.embedderCallbackName);
            break;
        default:
            var detailsNode = WebInspector.TimelineUIUtils.buildDetailsNode(record, linkifier);
            if (detailsNode)
                contentHelper.appendElementRow(WebInspector.UIString("Details"), detailsNode);
            break;
    }

    if (relatedNode)
        contentHelper.appendElementRow(relatedNodeLabel || WebInspector.UIString("Related node"), WebInspector.DOMPresentationUtils.linkifyNodeReference(relatedNode));

    if (record.scriptName && record.type !== recordTypes.FunctionCall)
        contentHelper.appendLocationRow(WebInspector.UIString("Function Call"), record.scriptName, record.scriptLine);

    if (record.jsHeapSizeUsed) {
        if (record.usedHeapSizeDelta) {
            var sign = record.usedHeapSizeDelta > 0 ? "+" : "-";
            contentHelper.appendTextRow(WebInspector.UIString("Used JavaScript Heap Size"),
                WebInspector.UIString("%s (%s%s)", Number.bytesToString(record.jsHeapSizeUsed), sign, Number.bytesToString(Math.abs(record.usedHeapSizeDelta))));
        } else if (record.category === WebInspector.TimelineUIUtils.categories().scripting)
            contentHelper.appendTextRow(WebInspector.UIString("Used JavaScript Heap Size"), Number.bytesToString(record.jsHeapSizeUsed));
    }

    if (record.callSiteStackTrace)
        contentHelper.appendStackTrace(callSiteStackTraceLabel || WebInspector.UIString("Call Site stack"), record.callSiteStackTrace);

    if (record.stackTrace)
        contentHelper.appendStackTrace(callStackLabel || WebInspector.UIString("Call Stack"), record.stackTrace);

    if (record.warnings()) {
        var ul = document.createElement("ul");
        for (var i = 0; i < record.warnings().length; ++i)
            ul.createChild("li").textContent = record.warnings()[i];
        contentHelper.appendElementRow(WebInspector.UIString("Warning"), ul);
    }
    fragment.appendChild(contentHelper.element);
    return fragment;
}

/**
 * @param {!Array.<number>} quad
 * @return {number}
 */
WebInspector.TimelineUIUtils._quadWidth = function(quad)
{
    return Math.round(Math.sqrt(Math.pow(quad[0] - quad[2], 2) + Math.pow(quad[1] - quad[3], 2)));
}

/**
 * @param {!Array.<number>} quad
 * @return {number}
 */
WebInspector.TimelineUIUtils._quadHeight = function(quad)
{
    return Math.round(Math.sqrt(Math.pow(quad[0] - quad[6], 2) + Math.pow(quad[1] - quad[7], 2)));
}

/**
 * @param {!WebInspector.TimelineModel.Record} record
 * @param {!WebInspector.Linkifier} linkifier
 * @return {?Node}
 */
WebInspector.TimelineUIUtils.buildDetailsNode = function(record, linkifier)
{
    var details;
    var detailsText;

    switch (record.type) {
    case WebInspector.TimelineModel.RecordType.GCEvent:
        detailsText = WebInspector.UIString("%s collected", Number.bytesToString(record.data["usedHeapSizeDelta"]));
        break;
    case WebInspector.TimelineModel.RecordType.TimerFire:
        detailsText = record.data["timerId"];
        break;
    case WebInspector.TimelineModel.RecordType.FunctionCall:
        if (record.scriptName)
            details = linkifyLocation(record.scriptName, record.scriptLine, 0);
        break;
    case WebInspector.TimelineModel.RecordType.FireAnimationFrame:
        detailsText = record.data["id"];
        break;
    case WebInspector.TimelineModel.RecordType.EventDispatch:
        detailsText = record.data ? record.data["type"] : null;
        break;
    case WebInspector.TimelineModel.RecordType.Paint:
        var width = record.data.clip ? WebInspector.TimelineUIUtils._quadWidth(record.data.clip) : record.data.width;
        var height = record.data.clip ? WebInspector.TimelineUIUtils._quadHeight(record.data.clip) : record.data.height;
        if (width && height)
            detailsText = WebInspector.UIString("%d\u2009\u00d7\u2009%d", width, height);
        break;
    case WebInspector.TimelineModel.RecordType.TimerInstall:
    case WebInspector.TimelineModel.RecordType.TimerRemove:
        details = linkifyTopCallFrame();
        detailsText = record.data["timerId"];
        break;
    case WebInspector.TimelineModel.RecordType.RequestAnimationFrame:
    case WebInspector.TimelineModel.RecordType.CancelAnimationFrame:
        details = linkifyTopCallFrame();
        detailsText = record.data["id"];
        break;
    case WebInspector.TimelineModel.RecordType.ParseHTML:
    case WebInspector.TimelineModel.RecordType.RecalculateStyles:
        details = linkifyTopCallFrame();
        break;
    case WebInspector.TimelineModel.RecordType.EvaluateScript:
        details = record.url ? linkifyLocation(record.url, record.data["lineNumber"], 0) : null;
        break;
    case WebInspector.TimelineModel.RecordType.XHRReadyStateChange:
    case WebInspector.TimelineModel.RecordType.XHRLoad:
    case WebInspector.TimelineModel.RecordType.ScheduleResourceRequest:
    case WebInspector.TimelineModel.RecordType.ResourceSendRequest:
    case WebInspector.TimelineModel.RecordType.ResourceReceivedData:
    case WebInspector.TimelineModel.RecordType.ResourceReceiveResponse:
    case WebInspector.TimelineModel.RecordType.ResourceFinish:
    case WebInspector.TimelineModel.RecordType.DecodeImage:
    case WebInspector.TimelineModel.RecordType.ResizeImage:
        detailsText = WebInspector.displayNameForURL(record.url);
        break;
    case WebInspector.TimelineModel.RecordType.ConsoleTime:
        detailsText = record.data["message"];
        break;
    case WebInspector.TimelineModel.RecordType.EmbedderCallback:
        detailsText = record.data["callbackName"];
        break;
    default:
        details = record.scriptName ? linkifyLocation(record.scriptName, record.scriptLine, 0) : linkifyTopCallFrame();
        break;
    }

    if (!details && detailsText)
        details = document.createTextNode(detailsText);
    return details;

    /**
     * @param {string} url
     * @param {number} lineNumber
     * @param {number=} columnNumber
     */
    function linkifyLocation(url, lineNumber, columnNumber)
    {
        // FIXME(62725): stack trace line/column numbers are one-based.
        columnNumber = columnNumber ? columnNumber - 1 : 0;
        return linkifier.linkifyLocation(url, lineNumber - 1, columnNumber, "timeline-details");
    }

    /**
     * @param {!ConsoleAgent.CallFrame} callFrame
     */
    function linkifyCallFrame(callFrame)
    {
        return linkifyLocation(callFrame.url, callFrame.lineNumber, callFrame.columnNumber);
    }

    /**
     * @return {?Element}
     */
    function linkifyTopCallFrame()
    {
        if (record.stackTrace)
            return linkifyCallFrame(record.stackTrace[0]);
        if (record.callSiteStackTrace)
            return linkifyCallFrame(record.callSiteStackTrace[0]);
        return null;
    }

    /**
     * @return {?Element}
     */
    function linkifyScriptLocation()
    {
        return record.scriptName ? linkifyLocation(record.scriptName, record.scriptLine, 0) : null;
    }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {string} name
 * @param {string} title
 * @param {number} overviewStripGroupIndex
 * @param {string} borderColor
 * @param {string} backgroundColor
 * @param {string} fillColorStop0
 * @param {string} fillColorStop1
 */
WebInspector.TimelineCategory = function(name, title, overviewStripGroupIndex, borderColor, backgroundColor, fillColorStop0, fillColorStop1)
{
    this.name = name;
    this.title = title;
    this.overviewStripGroupIndex = overviewStripGroupIndex;
    this.borderColor = borderColor;
    this.backgroundColor = backgroundColor;
    this.fillColorStop0 = fillColorStop0;
    this.fillColorStop1 = fillColorStop1;
    this.hidden = false;
}

WebInspector.TimelineCategory.Events = {
    VisibilityChanged: "VisibilityChanged"
};

WebInspector.TimelineCategory.prototype = {
    /**
     * @return {boolean}
     */
    get hidden()
    {
        return this._hidden;
    },

    set hidden(hidden)
    {
        this._hidden = hidden;
        this.dispatchEventToListeners(WebInspector.TimelineCategory.Events.VisibilityChanged, this);
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @param {string} title
 */
WebInspector.TimelinePopupContentHelper = function(title)
{
    this._contentTable = document.createElement("table");
    var titleCell = this._createCell(WebInspector.UIString("%s - Details", title), "timeline-details-title");
    titleCell.colSpan = 2;
    var titleRow = document.createElement("tr");
    titleRow.appendChild(titleCell);
    this._contentTable.appendChild(titleRow);
}

WebInspector.TimelinePopupContentHelper.prototype = {
    /**
     * @return {!Element}
     */
    contentTable: function()
    {
        return this._contentTable;
    },

    /**
     * @param {string=} styleName
     */
    _createCell: function(content, styleName)
    {
        var text = document.createElement("label");
        text.appendChild(document.createTextNode(content));
        var cell = document.createElement("td");
        cell.className = "timeline-details";
        if (styleName)
            cell.className += " " + styleName;
        cell.textContent = content;
        return cell;
    },

    /**
     * @param {string} title
     * @param {string|number|boolean} content
     */
    appendTextRow: function(title, content)
    {
        var row = document.createElement("tr");
        row.appendChild(this._createCell(title, "timeline-details-row-title"));
        row.appendChild(this._createCell(content, "timeline-details-row-data"));
        this._contentTable.appendChild(row);
    },

    /**
     * @param {string} title
     * @param {!Node|string} content
     */
    appendElementRow: function(title, content)
    {
        var row = document.createElement("tr");
        var titleCell = this._createCell(title, "timeline-details-row-title");
        row.appendChild(titleCell);
        var cell = document.createElement("td");
        cell.className = "details";
        if (content instanceof Node)
            cell.appendChild(content);
        else
            cell.createTextChild(content || "");
        row.appendChild(cell);
        this._contentTable.appendChild(row);
    }
}

/**
 * @constructor
 * @param {!WebInspector.Linkifier} linkifier
 * @param {boolean} monospaceValues
 */
WebInspector.TimelineDetailsContentHelper = function(linkifier, monospaceValues)
{
    this._linkifier = linkifier;
    this.element = document.createElement("div");
    this.element.className = "timeline-details-view-block";
    this._monospaceValues = monospaceValues;
}

WebInspector.TimelineDetailsContentHelper.prototype = {
    /**
     * @param {string} title
     * @param {string|number|boolean} value
     */
    appendTextRow: function(title, value)
    {
        var rowElement = this.element.createChild("div", "timeline-details-view-row");
        rowElement.createChild("span", "timeline-details-view-row-title").textContent = WebInspector.UIString("%s: ", title);
        rowElement.createChild("span", "timeline-details-view-row-value" + (this._monospaceValues ? " monospace" : "")).textContent = value;
    },

    /**
     * @param {string} title
     * @param {!Node|string} content
     */
    appendElementRow: function(title, content)
    {
        var rowElement = this.element.createChild("div", "timeline-details-view-row");
        rowElement.createChild("span", "timeline-details-view-row-title").textContent = WebInspector.UIString("%s: ", title);
        var valueElement = rowElement.createChild("span", "timeline-details-view-row-details" + (this._monospaceValues ? " monospace" : ""));
        if (content instanceof Node)
            valueElement.appendChild(content);
        else
            valueElement.createTextChild(content || "");
    },

    /**
     * @param {string} title
     * @param {string} url
     * @param {number} line
     */
    appendLocationRow: function(title, url, line)
    {
        this.appendElementRow(title, this._linkifier.linkifyLocation(url, line - 1) || "");
    },

    /**
     * @param {string} title
     * @param {!Array.<!ConsoleAgent.CallFrame>} stackTrace
     */
    appendStackTrace: function(title, stackTrace)
    {
        var rowElement = this.element.createChild("div", "timeline-details-view-row");
        rowElement.createChild("span", "timeline-details-view-row-title").textContent = WebInspector.UIString("%s: ", title);
        var stackTraceElement = rowElement.createChild("div", "timeline-details-view-row-stack-trace monospace");

        for (var i = 0; i < stackTrace.length; ++i) {
            var stackFrame = stackTrace[i];
            var row = stackTraceElement.createChild("div");
            row.createTextChild(stackFrame.functionName || WebInspector.UIString("(anonymous function)"));
            row.createTextChild(" @ ");
            var urlElement = this._linkifier.linkifyLocation(stackFrame.url, stackFrame.lineNumber - 1);
            row.appendChild(urlElement);
        }
    }
}
