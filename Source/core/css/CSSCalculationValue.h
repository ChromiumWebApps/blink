/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#ifndef CSSCalculationValue_h
#define CSSCalculationValue_h

#include "core/css/CSSParserValues.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "platform/CalculationValue.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class CSSParserValueList;
class CSSValueList;
class CalculationValue;
class CalcExpressionNode;
class Length;

enum CalculationCategory {
    CalcNumber = 0,
    CalcLength,
    CalcPercent,
    CalcPercentNumber,
    CalcPercentLength,
    CalcOther
};

class CSSCalcExpressionNode : public RefCountedWillBeGarbageCollected<CSSCalcExpressionNode> {
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(CSSCalcExpressionNode);
public:
    enum Type {
        CssCalcPrimitiveValue = 1,
        CssCalcBinaryOperation
    };

    virtual bool isZero() const = 0;
    virtual PassOwnPtr<CalcExpressionNode> toCalcValue(const CSSToLengthConversionData&) const = 0;
    virtual double doubleValue() const = 0;
    virtual double computeLengthPx(const CSSToLengthConversionData&) const = 0;
    virtual String customCSSText() const = 0;
    virtual bool equals(const CSSCalcExpressionNode& other) const { return m_category == other.m_category && m_isInteger == other.m_isInteger; }
    virtual Type type() const = 0;

    CalculationCategory category() const { return m_category; }
    virtual CSSPrimitiveValue::UnitTypes primitiveType() const = 0;
    bool isInteger() const { return m_isInteger; }

    virtual void trace(Visitor*) { }

protected:
    CSSCalcExpressionNode(CalculationCategory category, bool isInteger)
        : m_category(category)
        , m_isInteger(isInteger)
    {
    }

    CalculationCategory m_category;
    bool m_isInteger;
};

class CSSCalcValue : public CSSValue {
public:
    static PassRefPtrWillBeRawPtr<CSSCalcValue> create(CSSParserString name, CSSParserValueList*, ValueRange);
    static PassRefPtrWillBeRawPtr<CSSCalcValue> create(PassRefPtrWillBeRawPtr<CSSCalcExpressionNode>, ValueRange = ValueRangeAll);
    static PassRefPtrWillBeRawPtr<CSSCalcValue> create(const CalculationValue* value, float zoom) { return adoptRefWillBeRefCountedGarbageCollected(new CSSCalcValue(value, zoom)); }

    static PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> createExpressionNode(PassRefPtrWillBeRawPtr<CSSPrimitiveValue>, bool isInteger = false);
    static PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> createExpressionNode(PassRefPtrWillBeRawPtr<CSSCalcExpressionNode>, PassRefPtrWillBeRawPtr<CSSCalcExpressionNode>, CalcOperator);
    static PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> createExpressionNode(const CalcExpressionNode*, float zoom);
    static PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> createExpressionNode(const Length&, float zoom);

    PassRefPtr<CalculationValue> toCalcValue(const CSSToLengthConversionData& conversionData) const
    {
        return CalculationValue::create(m_expression->toCalcValue(conversionData), m_nonNegative ? ValueRangeNonNegative : ValueRangeAll);
    }
    CalculationCategory category() const { return m_expression->category(); }
    bool isInt() const { return m_expression->isInteger(); }
    double doubleValue() const;
    bool isNegative() const { return m_expression->doubleValue() < 0; }
    ValueRange permittedValueRange() { return m_nonNegative ? ValueRangeNonNegative : ValueRangeAll; }
    double computeLengthPx(const CSSToLengthConversionData&) const;
    CSSCalcExpressionNode* expressionNode() const { return m_expression.get(); }

    String customCSSText() const;
    bool equals(const CSSCalcValue&) const;

    void traceAfterDispatch(Visitor*);

private:
    CSSCalcValue(PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> expression, ValueRange range)
        : CSSValue(CalculationClass)
        , m_expression(expression)
        , m_nonNegative(range == ValueRangeNonNegative)
    {
    }
    CSSCalcValue(const CalculationValue* value, float zoom)
        : CSSValue(CalculationClass)
        , m_expression(createExpressionNode(value->expression(), zoom))
        , m_nonNegative(value->isNonNegative())
    {
    }

    double clampToPermittedRange(double) const;

    const RefPtrWillBeMember<CSSCalcExpressionNode> m_expression;
    const bool m_nonNegative;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCalcValue, isCalcValue());

} // namespace WebCore


#endif // CSSCalculationValue_h
