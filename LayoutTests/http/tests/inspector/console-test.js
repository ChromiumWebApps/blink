var initialize_ConsoleTest = function() {


InspectorTest.showConsolePanel = function()
{
    WebInspector.inspectorView.showPanel("console");
}

InspectorTest.prepareConsoleMessageText = function(messageElement)
{
    var messageText = messageElement.textContent.replace(/\u200b/g, "");
    // Replace scriptIds with generic scriptId string to avoid flakiness.
    messageText = messageText.replace(/VM\d+/g, "VM");
    // Strip out InjectedScript from stack traces to avoid rebaselining each time InjectedScriptSource is edited.
    messageText = messageText.replace(/InjectedScript[\.a-zA-Z_]+ VM:\d+/g, "");
    // The message might be extremely long in case of dumping stack overflow message.
    messageText = messageText.substring(0, 1024);
    return messageText;
}

InspectorTest.dumpConsoleMessages = function(printOriginatingCommand, dumpClassNames)
{
    WebInspector.inspectorView.panel("console");
    var result = [];
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var message = messageViews[i].consoleMessage();
        var element = messageViews[i].toMessageElement();

        if (dumpClassNames) {
            var classNames = [];
            for (var node = element.firstChild; node; node = node.traverseNextNode(element)) {
                if (node.nodeType === Node.ELEMENT_NODE && node.className)
                    classNames.push(node.className);
            }
        }

        if (InspectorTest.dumpConsoleTableMessage(message)) {
            if (dumpClassNames)
                InspectorTest.addResult(classNames.join(" > "));
        } else {
            var messageText = InspectorTest.prepareConsoleMessageText(element)
            InspectorTest.addResult(messageText + (dumpClassNames ? " " + classNames.join(" > ") : ""));
        }

        var uiMessage = messageViews[i];
        if (printOriginatingCommand && uiMessage.originatingCommand) {
            var originatingElement = uiMessage.originatingCommand.toMessageElement();
            InspectorTest.addResult("Originating from: " + originatingElement.textContent.replace(/\u200b/g, ""));
        }
    }
    return result;
}

InspectorTest.dumpConsoleTableMessage = function(message)
{
    var table = InspectorTest.toViewMessage(message).toMessageElement();
    var headers = table.querySelectorAll("th div");
    if (!headers.length)
        return false;

    var headerLine = "";
    for (var i = 0; i < headers.length; i++)
        headerLine += headers[i].textContent + " | ";

    InspectorTest.addResult("HEADER " + headerLine);

    var rows = table.querySelectorAll(".data-container tr");

    for (var i = 0; i < rows.length; i++) {
        var row = rows[i];
        var rowLine = "";
        var items = row.querySelectorAll("td > div > span");
        for (var j = 0; j < items.length; j++)
            rowLine += items[j].textContent + " | ";

        if (rowLine.trim())
            InspectorTest.addResult("ROW " + rowLine);
    }
    return true;
}

InspectorTest.dumpConsoleMessagesWithStyles = function(sortMessages)
{
    WebInspector.inspectorView.panel("console");
    var result = [];
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].toMessageElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element)
        InspectorTest.addResult(messageText);
        var spans = element.querySelectorAll(".console-message-text > span > span");
        for (var j = 0; j < spans.length; j++)
            InspectorTest.addResult("Styled text #" + j + ": " + (spans[j].style.cssText || "NO STYLES DEFINED"));
    }
}

InspectorTest.dumpConsoleMessagesWithClasses = function(sortMessages) {
    WebInspector.inspectorView.panel("console");
    var result = [];
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].toMessageElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element)
        result.push(messageText + " " + element.getAttribute("class"));
    }
    if (sortMessages)
        result.sort();
    for (var i = 0; i < result.length; ++i)
        InspectorTest.addResult(result[i]);
}

InspectorTest.expandConsoleMessages = function(callback)
{
    WebInspector.inspectorView.panel("console");
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var message = messageViews[i].consoleMessage();
        var element = messageViews[i].toMessageElement();
        var node = element;
        while (node) {
            if (node.treeElementForTest)
                node.treeElementForTest.expand();
            if (node._section) {
                message.section = node._section;
                node._section.expanded = true;
            }
            node = node.traverseNextNode(element);
        }
    }
    if (callback)
        InspectorTest.runAfterPendingDispatches(callback);
}

InspectorTest.checkConsoleMessagesDontHaveParameters = function()
{
    WebInspector.inspectorView.panel("console");
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var m = messageViews[i].consoleMessage();
        InspectorTest.addResult("Message[" + i + "]:");
        InspectorTest.addResult("Message: " + WebInspector.displayNameForURL(m.url) + ":" + m.line + " " + m.message);
        if ("_parameters" in m) {
            if (m._parameters)
                InspectorTest.addResult("FAILED: message parameters list is not empty: " + m.parameters);
            else
                InspectorTest.addResult("SUCCESS: message parameters list is empty. ");
        } else {
            InspectorTest.addResult("FAILED: didn't find _parameters field in the message.");
        }
    }
}

InspectorTest.waitUntilMessageReceived = function(callback)
{
    InspectorTest.addSniffer(WebInspector.console, "addMessage", callback, false);
}

}
