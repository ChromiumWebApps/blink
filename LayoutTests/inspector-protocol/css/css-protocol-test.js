function initialize_cssTest()
{

InspectorTest.requestMainFrameId = function(callback)
{
    InspectorTest.sendCommandOrDie("Page.enable", {}, pageEnabled);

    function pageEnabled()
    {
        InspectorTest.sendCommandOrDie("Page.getResourceTree", {}, resourceTreeLoaded);
    }

    function resourceTreeLoaded(payload)
    {
        callback(payload.frameTree.frame.id);
    }
};

InspectorTest.requestDocumentNodeId = function(callback)
{
    InspectorTest.sendCommandOrDie("DOM.getDocument", {}, onGotDocument);

    function onGotDocument(result)
    {
        callback(result.root.nodeId);
    }
};

InspectorTest.requestNodeId = function(selector, callback)
{
    InspectorTest.requestDocumentNodeId(onGotDocumentNodeId);

    function onGotDocumentNodeId(documentNodeId)
    {
        InspectorTest.sendCommandOrDie("DOM.querySelector", { "nodeId": documentNodeId , "selector": selector }, onGotNode);
    }

    function onGotNode(result)
    {
        callback(result.nodeId);
    }
};

InspectorTest.dumpRuleMatch = function(ruleMatch)
{
    function log(indent, string)
    {
        var indentString = Array(indent+1).join(" ");
        InspectorTest.log(indentString + string);
    }

    var rule = ruleMatch.rule;
    var matchingSelectors = ruleMatch.matchingSelectors;
    var selectorLine = "";
    var selectors = rule.selectorList.selectors;
    for (var i = 0; i < selectors.length; ++i) {
        if (i > 0)
            selectorLine += ", ";
        var matching = matchingSelectors.indexOf(i) !== -1;
        if (matching)
            selectorLine += "*";
        selectorLine += selectors[i].value;
        if (matching)
            selectorLine += "*";
    }
    selectorLine += " {";
    selectorLine += "    " + rule.origin + (rule.sourceURL ? " (" + InspectorTest.displayName(rule.sourceURL) + ")" : "");
    log(0, selectorLine);
    var style = rule.style;
    var cssProperties = style.cssProperties;
    for (var i = 0; i < cssProperties.length; ++i) {
        var cssProperty = cssProperties[i];
        var propertyLine = cssProperty.name + ": " + cssProperty.value + ";";
        log(4, propertyLine);
    }
    log(0, "}");
};

InspectorTest.displayName = function(url)
{
    return url.substr(url.lastIndexOf("/") + 1);
};

InspectorTest.loadAndDumpMatchingRules = function(nodeId, callback)
{
    InspectorTest.requestNodeId(nodeId, nodeIdLoaded);

    function nodeIdLoaded(nodeId)
    {
        InspectorTest.sendCommandOrDie("CSS.getMatchedStylesForNode", { "nodeId": nodeId }, matchingRulesLoaded);
    }

    function matchingRulesLoaded(result)
    {
        InspectorTest.log("Dumping matched rules: ");
        var ruleMatches = result.matchedCSSRules;
        for (var i = 0; i < ruleMatches.length; ++i) {
            var ruleMatch = ruleMatches[i];
            var origin = ruleMatch.rule.origin;
            if (origin !== "inspector" && origin !== "regular")
                continue;
            InspectorTest.dumpRuleMatch(ruleMatch);
        }
        callback();
    }
}

}