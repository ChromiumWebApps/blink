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

importScript("LayerTree.js");
importScript("Layers3DView.js");
importScript("LayerDetailsView.js");
importScript("PaintProfilerView.js");
importScript("TransformController.js");

/**
 * @constructor
 * @extends {WebInspector.PanelWithSidebarTree}
 */
WebInspector.LayersPanel = function()
{
    WebInspector.PanelWithSidebarTree.call(this, "layers", 225);
    this.registerRequiredCSS("layersPanel.css");

    this.sidebarElement().classList.add("outline-disclosure");
    this.sidebarTree.element.classList.remove("sidebar-tree");

    this._model = new WebInspector.LayerTreeModel();
    this._model.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, this._onLayerTreeUpdated, this);
    this._currentlySelectedLayer = null;
    this._currentlyHoveredLayer = null;

    this._layerTree = new WebInspector.LayerTree(this._model, this.sidebarTree);
    this._layerTree.addEventListener(WebInspector.LayerTree.Events.LayerSelected, this._onLayerSelected, this);
    this._layerTree.addEventListener(WebInspector.LayerTree.Events.LayerHovered, this._onLayerHovered, this);

    this._rightSplitView = new WebInspector.SplitView(false, true, "layerDetailsSplitViewState");
    this._rightSplitView.show(this.mainElement());

    this._layers3DView = new WebInspector.Layers3DView(this._model);
    this._layers3DView.show(this._rightSplitView.mainElement());
    this._layers3DView.addEventListener(WebInspector.Layers3DView.Events.LayerSelected, this._onLayerSelected, this);
    this._layers3DView.addEventListener(WebInspector.Layers3DView.Events.LayerHovered, this._onLayerHovered, this);
    this._layers3DView.addEventListener(WebInspector.Layers3DView.Events.LayerSnapshotRequested, this._onSnapshotRequested, this);

    this._tabbedPane = new WebInspector.TabbedPane();
    this._tabbedPane.show(this._rightSplitView.sidebarElement());

    this._layerDetailsView = new WebInspector.LayerDetailsView(this._model);
    this._tabbedPane.appendTab(WebInspector.LayersPanel.DetailsViewTabs.Details, WebInspector.UIString("Details"), this._layerDetailsView);
    this._paintProfilerView = new WebInspector.PaintProfilerView(this._model, this._layers3DView);
    this._tabbedPane.appendTab(WebInspector.LayersPanel.DetailsViewTabs.Profiler, WebInspector.UIString("Profiler"), this._paintProfilerView);
}

WebInspector.LayersPanel.DetailsViewTabs = {
    Details: "details",
    Profiler: "profiler"
};

WebInspector.LayersPanel.prototype = {
    wasShown: function()
    {
        WebInspector.Panel.prototype.wasShown.call(this);
        this.sidebarTree.element.focus();
        this._model.enable();
    },

    willHide: function()
    {
        this._model.disable();
        WebInspector.Panel.prototype.willHide.call(this);
    },

    /**
     * @param {!WebInspector.LayerTreeSnapshot} snapshot
     */
    _showSnapshot: function(snapshot)
    {
        this._model.setSnapshot(snapshot);
    },

    _onLayerTreeUpdated: function()
    {
        if (this._currentlySelectedLayer && !this._model.layerById(this._currentlySelectedLayer.id()))
            this._selectLayer(null);
        if (this._currentlyHoveredLayer && !this._model.layerById(this._currentlyHoveredLayer.id()))
            this._hoverLayer(null);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onLayerSelected: function(event)
    {
        var layer = /** @type {!WebInspector.Layer} */ (event.data);
        this._selectLayer(layer);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onLayerHovered: function(event)
    {
        var layer = /** @type WebInspector.Layer */ (event.data);
        this._hoverLayer(layer);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onSnapshotRequested: function(event)
    {
        var layer = /** @type {!WebInspector.Layer} */ (event.data);
        this._tabbedPane.selectTab(WebInspector.LayersPanel.DetailsViewTabs.Profiler);
        this._paintProfilerView.profile(layer);
    },

    /**
     * @param {?WebInspector.Layer} layer
     */
    _selectLayer: function(layer)
    {
        if (this._currentlySelectedLayer === layer)
            return;
        this._currentlySelectedLayer = layer;
        var nodeId = layer && layer.nodeIdForSelfOrAncestor();
        if (nodeId)
            WebInspector.domAgent.highlightDOMNodeForTwoSeconds(nodeId);
        else
            WebInspector.domAgent.hideDOMNodeHighlight();
        this._layerTree.selectLayer(layer);
        this._layers3DView.selectLayer(layer);
        this._layerDetailsView.setLayer(layer);
    },

    /**
     * @param {?WebInspector.Layer} layer
     */
    _hoverLayer: function(layer)
    {
        if (this._currentlyHoveredLayer === layer)
            return;
        this._currentlyHoveredLayer = layer;
        var nodeId = layer && layer.nodeIdForSelfOrAncestor();
        if (nodeId)
            WebInspector.domAgent.highlightDOMNode(nodeId);
        else
            WebInspector.domAgent.hideDOMNodeHighlight();
        this._layerTree.hoverLayer(layer);
        this._layers3DView.hoverLayer(layer);
    },

    __proto__: WebInspector.PanelWithSidebarTree.prototype
}

/**
 * @constructor
 * @implements {WebInspector.Revealer}
 */
WebInspector.LayersPanel.LayerTreeRevealer = function()
{
}

WebInspector.LayersPanel.LayerTreeRevealer.prototype = {
    /**
     * @param {!Object} layerTree
     */
    reveal: function(layerTree)
    {
        if (layerTree instanceof WebInspector.LayerTreeSnapshot)
            /** @type {!WebInspector.LayersPanel} */ (WebInspector.inspectorView.showPanel("layers"))._showSnapshot(layerTree);
    }
}
