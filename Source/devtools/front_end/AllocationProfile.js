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
 */
WebInspector.AllocationProfile = function(profile, liveObjectStats)
{
    this._strings = profile.strings;
    this._liveObjectStats = liveObjectStats;

    this._nextNodeId = 1;
    this._functionInfos = []
    this._idToNode = {};
    this._collapsedTopNodeIdToFunctionInfo = {};

    this._traceTops = null;

    this._buildAllocationFunctionInfos(profile);
    this._traceTree = this._buildInvertedAllocationTree(profile, liveObjectStats);
}

WebInspector.AllocationProfile.prototype = {
    _buildAllocationFunctionInfos: function(profile)
    {
        var strings = this._strings;

        var functionInfoFields = profile.snapshot.meta.trace_function_info_fields;
        var functionIdOffset = functionInfoFields.indexOf("function_id");
        var functionNameOffset = functionInfoFields.indexOf("name");
        var scriptNameOffset = functionInfoFields.indexOf("script_name");
        var scriptIdOffset = functionInfoFields.indexOf("script_id");
        var lineOffset = functionInfoFields.indexOf("line");
        var columnOffset = functionInfoFields.indexOf("column");
        var functionInfoFieldCount = functionInfoFields.length;

        var rawInfos = profile.trace_function_infos;
        var infoLength = rawInfos.length;
        var functionInfos = this._functionInfos = new Array(infoLength / functionInfoFieldCount);
        var index = 0;
        for (var i = 0; i < infoLength; i += functionInfoFieldCount) {
            functionInfos[index++] = new WebInspector.FunctionAllocationInfo(
                strings[rawInfos[i + functionNameOffset]],
                strings[rawInfos[i + scriptNameOffset]],
                rawInfos[i + scriptIdOffset],
                rawInfos[i + lineOffset],
                rawInfos[i + columnOffset]);
        }
    },

    _buildInvertedAllocationTree: function(profile, liveObjectStats)
    {
        var traceTreeRaw = profile.trace_tree;
        var functionInfos = this._functionInfos;

        var traceNodeFields = profile.snapshot.meta.trace_node_fields;
        var nodeIdOffset = traceNodeFields.indexOf("id");
        var functionInfoIndexOffset = traceNodeFields.indexOf("function_info_index");
        var allocationCountOffset = traceNodeFields.indexOf("count");
        var allocationSizeOffset = traceNodeFields.indexOf("size");
        var childrenOffset = traceNodeFields.indexOf("children");
        var nodeFieldCount = traceNodeFields.length;

        function traverseNode(rawNodeArray, nodeOffset, parent)
        {
            var functionInfo = functionInfos[rawNodeArray[nodeOffset + functionInfoIndexOffset]];
            var id = rawNodeArray[nodeOffset + nodeIdOffset];
            var stats = liveObjectStats[id];
            var liveCount = stats ? stats.count : 0;
            var liveSize = stats ? stats.size : 0;
            var result = new WebInspector.AllocationTraceNode(
                id,
                functionInfo,
                rawNodeArray[nodeOffset + allocationCountOffset],
                rawNodeArray[nodeOffset + allocationSizeOffset],
                liveCount,
                liveSize,
                parent);
            functionInfo.addTraceTopNode(result);

            var rawChildren = rawNodeArray[nodeOffset + childrenOffset];
            for (var i = 0; i < rawChildren.length; i += nodeFieldCount) {
                result.children.push(traverseNode(rawChildren, i, result));
            }
            return result;
        }

        return traverseNode(traceTreeRaw, 0, null);
    },

    /**
     * @return {!Array.<!WebInspector.HeapSnapshotCommon.SerializedAllocationNode>}
     */
    serializeTraceTops: function()
    {
        if (this._traceTops)
            return this._traceTops;
        var result = this._traceTops = [];
        var functionInfos = this._functionInfos;
        for (var i = 0; i < functionInfos.length; i++) {
            var info = functionInfos[i];
            if (info.totalCount === 0)
                continue;
            var nodeId = this._nextNodeId++;
            result.push(this._serializeNode(
                nodeId,
                info,
                info.totalCount,
                info.totalSize,
                info.totalLiveCount,
                info.totalLiveSize,
                true));
            this._collapsedTopNodeIdToFunctionInfo[nodeId] = info;
        }
        result.sort(function(a, b) {
            return b.size - a.size;
        });
        return result;
    },

    /**
     * @param {string} nodeId
     * @return {!WebInspector.HeapSnapshotCommon.AllocationNodeCallers}
     */
    serializeCallers: function(nodeId)
    {
        var node = this._idToNode[nodeId];
        if (!node) {
            var functionInfo = this._collapsedTopNodeIdToFunctionInfo[nodeId];
            node = functionInfo.tracesWithThisTop();
            delete this._collapsedTopNodeIdToFunctionInfo[nodeId];
            this._idToNode[nodeId] = node;
        }

        var nodesWithSingleCaller = [];
        while (node.callers().length === 1) {
            node = node.callers()[0];
            nodesWithSingleCaller.push(this._serializeCaller(node));
        }

        var branchingCallers = [];
        var callers = node.callers();
        for (var i = 0; i < callers.length; i++) {
            branchingCallers.push(this._serializeCaller(callers[i]));
        }
        return new WebInspector.HeapSnapshotCommon.AllocationNodeCallers(nodesWithSingleCaller, branchingCallers);
    },

    _serializeCaller: function(node)
    {
        var callerId = this._nextNodeId++;
        this._idToNode[callerId] = node;
        return this._serializeNode(
            callerId,
            node.functionInfo,
            node.allocationCount,
            node.allocationSize,
            node.liveCount,
            node.liveSize,
            node.hasCallers());
    },

    /**
     * @param {number} nodeId
     * @param {!WebInspector.FunctionAllocationInfo} functionInfo
     * @param {number} count
     * @param {number} size
     * @param {number} liveCount
     * @param {number} liveSize
     * @param {boolean} hasChildren
     * @return {!WebInspector.HeapSnapshotCommon.SerializedAllocationNode}
     */
    _serializeNode: function(nodeId, functionInfo, count, size, liveCount, liveSize, hasChildren)
    {
        return new WebInspector.HeapSnapshotCommon.SerializedAllocationNode(
            nodeId,
            functionInfo.functionName,
            functionInfo.scriptName,
            functionInfo.line,
            functionInfo.column,
            count,
            size,
            liveCount,
            liveSize,
            hasChildren
        );
    }
}


/**
 * @constructor
 */
WebInspector.AllocationTraceNode = function(id, functionInfo, count, size, liveCount, liveSize, parent)
{
    this.id = id;
    this.functionInfo = functionInfo;
    this.allocationCount = count;
    this.allocationSize = size;
    this.liveCount = liveCount;
    this.liveSize = liveSize;
    this.parent = parent;
    this.children = [];
}


/**
 * @constructor
 * @param {!WebInspector.FunctionAllocationInfo} functionInfo
 */
WebInspector.AllocationBackTraceNode = function(functionInfo)
{
    this.functionInfo = functionInfo;
    this.allocationCount = 0;
    this.allocationSize = 0;
    this.liveCount = 0;
    this.liveSize = 0;
    this._callers = [];
}


WebInspector.AllocationBackTraceNode.prototype = {
    /**
     * @param {!WebInspector.AllocationTraceNode} traceNode
     * @return {!WebInspector.AllocationTraceNode}
     */
    addCaller: function(traceNode)
    {
        var functionInfo = traceNode.functionInfo;
        var result;
        for (var i = 0; i < this._callers.length; i++) {
            var caller = this._callers[i];
            if (caller.functionInfo === functionInfo) {
                result = caller;
                break;
            }
        }
        if (!result) {
            result = new WebInspector.AllocationBackTraceNode(functionInfo);
            this._callers.push(result);
        }
        return result;
    },

    /**
     * @return {!Array.<!WebInspector.AllocationBackTraceNode>}
     */
    callers: function()
    {
        return this._callers;
    },

    /**
     * @return {boolean}
     */
    hasCallers: function()
    {
        return this._callers.length > 0;
    }
}


/**
 * @constructor
 */
WebInspector.FunctionAllocationInfo = function(functionName, scriptName, scriptId, line, column)
{
    this.functionName = functionName;
    this.scriptName = scriptName;
    this.scriptId = scriptId;
    this.line = line;
    this.column = column;
    this.totalCount = 0;
    this.totalSize = 0;
    this.totalLiveCount = 0;
    this.totalLiveSize = 0;
    this._traceTops = [];
}

WebInspector.FunctionAllocationInfo.prototype = {
    addTraceTopNode: function(node)
    {
        if (node.allocationCount === 0)
            return;
        this._traceTops.push(node);
        this.totalCount += node.allocationCount;
        this.totalSize += node.allocationSize;
        this.totalLiveCount += node.liveCount;
        this.totalLiveSize += node.liveSize;
    },

    /**
     * @return {?WebInspector.AllocationBackTraceNode}
     */
    tracesWithThisTop: function()
    {
        if (!this._traceTops.length)
            return null;
        if (!this._backTraceTree)
            this._buildAllocationTraceTree();
        return this._backTraceTree;
    },

    _buildAllocationTraceTree: function()
    {
        this._backTraceTree = new WebInspector.AllocationBackTraceNode(this._traceTops[0].functionInfo);

        for (var i = 0; i < this._traceTops.length; i++) {
            var node = this._traceTops[i];
            var backTraceNode = this._backTraceTree;
            var count = node.allocationCount;
            var size = node.allocationSize;
            var liveCount = node.liveCount;
            var liveSize = node.liveSize;
            while (true) {
                backTraceNode.allocationCount += count;
                backTraceNode.allocationSize += size;
                backTraceNode.liveCount += liveCount;
                backTraceNode.liveSize += liveSize;
                node = node.parent;
                if (node === null) {
                    break;
                }
                backTraceNode = backTraceNode.addCaller(node);
            }
        }
    }
}
