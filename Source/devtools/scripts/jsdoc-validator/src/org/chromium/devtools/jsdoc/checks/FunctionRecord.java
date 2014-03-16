package org.chromium.devtools.jsdoc.checks;

import com.google.javascript.rhino.head.ast.FunctionNode;

public class FunctionRecord {
    final FunctionNode functionNode;
    final String name;
    final boolean isConstructor;
    final String returnType;
    final TypeRecord enclosingType;
    final FunctionRecord enclosingFunctionRecord;

    public FunctionRecord(FunctionNode functionNode, String name, boolean isConstructor,
            String returnType, TypeRecord parentType, FunctionRecord enclosingFunctionRecord) {
        this.functionNode = functionNode;
        this.name = name;
        this.isConstructor = isConstructor;
        this.returnType = returnType;
        this.enclosingType = parentType;
        this.enclosingFunctionRecord = enclosingFunctionRecord;
    }

    public boolean isTopLevelFunction() {
        return enclosingFunctionRecord == null;
    }

    public boolean hasReturnAnnotation() {
        return returnType != null;
    }
}
