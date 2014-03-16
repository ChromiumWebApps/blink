function test() {
    InspectorTest.addConsoleSniffer(addMessage);

    function addMessage(message) {
        var viewMessages = WebInspector.ConsolePanel._view()._visibleViewMessages;
        for (var i = 0; i < viewMessages.length; ++i) {
            var m = viewMessages[i].consoleMessage();
            InspectorTest.addResult("Message[" + i + "]: " + WebInspector.displayNameForURL(m.url) + ":" + m.line + " " + m.messageText);
            var trace = m.stackTrace;
            if (!trace) {
                InspectorTest.addResult("FAIL: no stack trace attached to message #" + i);
            } else {
                InspectorTest.addResult("Stack Trace:\n");
                InspectorTest.addResult("    url: " + trace[0].url);
                InspectorTest.addResult("    function: " + trace[0].functionName);
                InspectorTest.addResult("    line: " + trace[0].lineNumber);
            }
        }
        InspectorTest.completeTest();
    }
    InspectorTest.evaluateInPage("thisTest()");
}
