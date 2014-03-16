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
 * @extends {WebInspector.SplitView}
 * @param {!WebInspector.TimelineModeViewDelegate} delegate
 * @param {!WebInspector.TimelineModel} model
 */
WebInspector.MemoryStatistics = function(delegate, model)
{
    WebInspector.SplitView.call(this, true, false);

    this.element.id = "memory-graphs-container";

    this._delegate = delegate;
    this._model = model;
    this._calculator = new WebInspector.TimelineCalculator(this._model);

    this._graphsContainer = this.mainElement();
    this._createCurrentValuesBar();
    this._canvasView = new WebInspector.VBoxWithResizeCallback(this._resize.bind(this));
    this._canvasView.show(this._graphsContainer);
    this._canvasContainer = this._canvasView.element;
    this._canvasContainer.id = "memory-graphs-canvas-container";
    this._canvas = this._canvasContainer.createChild("canvas");
    this._canvas.id = "memory-counters-graph";

    this._canvasContainer.addEventListener("mouseover", this._onMouseMove.bind(this), true);
    this._canvasContainer.addEventListener("mousemove", this._onMouseMove.bind(this), true);
    this._canvasContainer.addEventListener("mouseout", this._onMouseOut.bind(this), true);
    this._canvasContainer.addEventListener("click", this._onClick.bind(this), true);
    // We create extra timeline grid here to reuse its event dividers.
    this._timelineGrid = new WebInspector.TimelineGrid();
    this._canvasContainer.appendChild(this._timelineGrid.dividersElement);

    // Populate sidebar
    this.sidebarElement().createChild("div", "sidebar-tree sidebar-tree-section").textContent = WebInspector.UIString("COUNTERS");
    this.createAllCounters();
}

/**
 * @constructor
 * @param {string} counterName
 */
WebInspector.MemoryStatistics.Counter = function(counterName)
{
    this.counterName = counterName;
    this.times = [];
    this.values = [];
}

WebInspector.MemoryStatistics.Counter.prototype = {
    /**
     * @param {number} time
     * @param {!TimelineAgent.Counters} counters
     */
    appendSample: function(time, counters)
    {
        var value = counters[this.counterName];
        if (value === undefined)
            return;
        if (this.values.length && this.values.peekLast() === value)
            return;
        this.times.push(time);
        this.values.push(value);
    },

    reset: function()
    {
        this.times = [];
        this.values = [];
    },

    /**
     * @param {!WebInspector.TimelineCalculator} calculator
     */
    _calculateVisibleIndexes: function(calculator)
    {
        var start = calculator.minimumBoundary();
        var end = calculator.maximumBoundary();

        // Maximum index of element whose time <= start.
        this._minimumIndex = Number.constrain(this.times.upperBound(start) - 1, 0, this.times.length - 1);

        // Minimum index of element whose time >= end.
        this._maximumIndex = Number.constrain(this.times.lowerBound(end), 0, this.times.length - 1);

        // Current window bounds.
        this._minTime = start;
        this._maxTime = end;
    },

    /**
     * @param {number} width
     */
    _calculateXValues: function(width)
    {
        if (!this.values.length)
            return;

        var xFactor = width / (this._maxTime - this._minTime);

        this.x = new Array(this.values.length);
        this.x[this._minimumIndex] = 0;
        for (var i = this._minimumIndex + 1; i < this._maximumIndex; i++)
             this.x[i] = xFactor * (this.times[i] - this._minTime);
        this.x[this._maximumIndex] = width;
    }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.SwatchCheckbox = function(title, color)
{
    this.element = document.createElement("div");
    this._swatch = this.element.createChild("div", "swatch");
    this.element.createChild("span", "title").textContent = title;
    this._color = color;
    this.checked = true;

    this.element.addEventListener("click", this._toggleCheckbox.bind(this), true);
}

WebInspector.SwatchCheckbox.Events = {
    Changed: "Changed"
}

WebInspector.SwatchCheckbox.prototype = {
    get checked()
    {
        return this._checked;
    },

    set checked(v)
    {
        this._checked = v;
        if (this._checked)
            this._swatch.style.backgroundColor = this._color;
        else
            this._swatch.style.backgroundColor = "";
    },

    _toggleCheckbox: function(event)
    {
        this.checked = !this.checked;
        this.dispatchEventToListeners(WebInspector.SwatchCheckbox.Events.Changed);
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @param {!WebInspector.MemoryStatistics} memoryCountersPane
 * @param {string} title
 * @param {string} graphColor
 * @param {!WebInspector.MemoryStatistics.Counter} counter
 */
WebInspector.CounterUIBase = function(memoryCountersPane, title, graphColor, counter)
{
    this._memoryCountersPane = memoryCountersPane;
    this.counter = counter;
    var container = memoryCountersPane.sidebarElement().createChild("div", "memory-counter-sidebar-info");
    var swatchColor = graphColor;
    this._swatch = new WebInspector.SwatchCheckbox(WebInspector.UIString(title), swatchColor);
    this._swatch.addEventListener(WebInspector.SwatchCheckbox.Events.Changed, this._toggleCounterGraph.bind(this));
    container.appendChild(this._swatch.element);

    this._value = null;
    this.graphColor = graphColor;
    this.strokeColor = graphColor;
    this.graphYValues = [];
}

WebInspector.CounterUIBase.prototype = {
    _toggleCounterGraph: function(event)
    {
        this._value.classList.toggle("hidden", !this._swatch.checked);
        this._memoryCountersPane.refresh();
    },

    /**
     * @param {number} x
     * @return {number}
     */
    _recordIndexAt: function(x)
    {
        return this.counter.x.upperBound(x, null, this.counter._minimumIndex + 1, this.counter._maximumIndex + 1) - 1;
    },

    /**
     * @param {number} x
     */
    updateCurrentValue: function(x)
    {
        if (!this.visible || !this.counter.values.length)
            return;
        var index = this._recordIndexAt(x);
        this._value.textContent = WebInspector.UIString(this._currentValueLabel, this.counter.values[index]);
        var y = this.graphYValues[index];
        this._marker.style.left = x + "px";
        this._marker.style.top = y + "px";
        this._marker.classList.remove("hidden");
    },

    clearCurrentValueAndMarker: function()
    {
        this._value.textContent = "";
        this._marker.classList.add("hidden");
    },

    get visible()
    {
        return this._swatch.checked;
    },
}

WebInspector.MemoryStatistics.prototype = {
    _createCurrentValuesBar: function()
    {
        throw new Error("Not implemented");
    },

    createAllCounters: function()
    {
        throw new Error("Not implemented");
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     */
    addRecord: function(record)
    {
        throw new Error("Not implemented");
    },

    reset: function()
    {
        for (var i = 0; i < this._counters.length; ++i)
            this._counters[i].reset();

        for (var i = 0; i < this._counterUI.length; ++i)
            this._counterUI[i].reset();

        this.refresh();
    },

    _resize: function()
    {
        var parentElement = this._canvas.parentElement;
        this._canvas.width = parentElement.clientWidth;
        this._canvas.height = parentElement.clientHeight;
        var timelinePaddingLeft = 15;
        this._calculator.setDisplayWindow(timelinePaddingLeft, this._canvas.width);
        this.refresh();
    },

    /**
     * @param {number} startTime
     * @param {number} endTime
     */
    setWindowTimes: function(startTime, endTime)
    {
        this._calculator.setWindow(startTime, endTime);
        this.scheduleRefresh();
    },

    scheduleRefresh: function()
    {
        if (this._refreshTimer)
            return;
        this._refreshTimer = setTimeout(this.refresh.bind(this), 300);
    },

    draw: function()
    {
        for (var i = 0; i < this._counters.length; ++i) {
            this._counters[i]._calculateVisibleIndexes(this._calculator);
            this._counters[i]._calculateXValues(this._canvas.width);
        }
        this._clear();
        this._setVerticalClip(10, this._canvas.height - 20);
    },

    /**
     * @param {?Event} event
     */
    _onClick: function(event)
    {
        var x = event.x - this._canvasContainer.totalOffsetLeft();
        var minDistance = Infinity;
        var bestTime;
        for (var i = 0; i < this._counterUI.length; ++i) {
            var counterUI = this._counterUI[i];
            if (!counterUI.counter.times.length)
                continue;
            var index = counterUI._recordIndexAt(x);
            var distance = Math.abs(x - counterUI.counter.x[index]);
            if (distance < minDistance) {
                minDistance = distance;
                bestTime = counterUI.counter.times[index];
            }
        }
        if (bestTime !== undefined)
            this._revealRecordAt(bestTime);
    },

    /**
     * @param {number} time
     */
    _revealRecordAt: function(time)
    {
        var recordToReveal;
        function findRecordToReveal(record)
        {
            if (record.startTime <= time && time <= record.endTime) {
                recordToReveal = record;
                return true;
            }
            // If there is no record containing the time than use the latest one before that time.
            if (!recordToReveal || record.endTime < time && recordToReveal.endTime < record.endTime)
                recordToReveal = record;
            return false;
        }
        this._model.forAllRecords(null, findRecordToReveal);
        this._delegate.selectRecord(recordToReveal);
    },

    /**
     * @param {?Event} event
     */
    _onMouseOut: function(event)
    {
        delete this._markerXPosition;
        this._clearCurrentValueAndMarker();
    },

    _clearCurrentValueAndMarker: function()
    {
        for (var i = 0; i < this._counterUI.length; i++)
            this._counterUI[i].clearCurrentValueAndMarker();
    },

    /**
     * @param {?Event} event
     */
    _onMouseMove: function(event)
    {
        var x = event.x - this._canvasContainer.totalOffsetLeft();
        this._markerXPosition = x;
        this._refreshCurrentValues();
    },

    _refreshCurrentValues: function()
    {
        if (this._markerXPosition === undefined)
            return;
        for (var i = 0; i < this._counterUI.length; ++i)
            this._counterUI[i].updateCurrentValue(this._markerXPosition);
    },

    refresh: function()
    {
        delete this._refreshTimer;
        this._timelineGrid.updateDividers(this._calculator);
        this.draw();
        this._refreshCurrentValues();
    },

    refreshRecords: function()
    {
        this.reset();
        var records = this._model.records();
        for (var i = 0; i < records.length; ++i)
            this.addRecord(records[i]);
    },

    /**
     * @param {number} originY
     * @param {number} height
     */
    _setVerticalClip: function(originY, height)
    {
        this._originY = originY;
        this._clippedHeight = height;
    },

    _clear: function()
    {
        var ctx = this._canvas.getContext("2d");
        ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    },

    /**
     * @param {?WebInspector.TimelineModel.Record} record
     * @param {string=} regex
     * @param {boolean=} selectRecord
     */
    highlightSearchResult: function(record, regex, selectRecord)
    {
    },

    /**
     * @param {?WebInspector.TimelineModel.Record} record
     */
    setSelectedRecord: function(record)
    {
    },

    __proto__: WebInspector.SplitView.prototype
}
