# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Functions for type handling and type conversion (Blink/C++ <-> V8/JS).

Spec:
http://www.w3.org/TR/WebIDL/#es-type-mapping

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import posixpath
import re

from idl_types import IdlType
from v8_globals import includes
from v8_utilities import strip_suffix


################################################################################
# V8-specific handling of IDL types
################################################################################

NON_WRAPPER_TYPES = frozenset([
    'CompareHow',
    'Dictionary',
    'EventHandler',
    'EventListener',
    'MediaQueryListListener',
    'NodeFilter',
    'SerializedScriptValue',
])
TYPED_ARRAYS = {
    # (cpp_type, v8_type), used by constructor templates
    'ArrayBuffer': None,
    'ArrayBufferView': None,
    'Float32Array': ('float', 'v8::kExternalFloatArray'),
    'Float64Array': ('double', 'v8::kExternalDoubleArray'),
    'Int8Array': ('signed char', 'v8::kExternalByteArray'),
    'Int16Array': ('short', 'v8::kExternalShortArray'),
    'Int32Array': ('int', 'v8::kExternalIntArray'),
    'Uint8Array': ('unsigned char', 'v8::kExternalUnsignedByteArray'),
    'Uint8ClampedArray': ('unsigned char', 'v8::kExternalPixelArray'),
    'Uint16Array': ('unsigned short', 'v8::kExternalUnsignedShortArray'),
    'Uint32Array': ('unsigned int', 'v8::kExternalUnsignedIntArray'),
}


def constructor_type_name(idl_type):
    # FIXME: replace this with a [ConstructorAttribute] extended attribute
    return strip_suffix(idl_type.base_type, 'Constructor')


def is_typed_array_type(idl_type):
    return idl_type.base_type in TYPED_ARRAYS


def is_wrapper_type(idl_type):
    return (idl_type.is_interface_type and
            idl_type.base_type not in NON_WRAPPER_TYPES)


################################################################################
# C++ types
################################################################################

CPP_TYPE_SAME_AS_IDL_TYPE = set([
    'double',
    'float',
    'long long',
    'unsigned long long',
])
CPP_INT_TYPES = set([
    'byte',
    'long',
    'short',
])
CPP_UNSIGNED_TYPES = set([
    'octet',
    'unsigned int',
    'unsigned long',
    'unsigned short',
])
CPP_SPECIAL_CONVERSION_RULES = {
    'CompareHow': 'Range::CompareHow',
    'Date': 'double',
    'Dictionary': 'Dictionary',
    'EventHandler': 'EventListener*',
    'MediaQueryListListener': 'RefPtrWillBeRawPtr<MediaQueryListListener>',
    'Promise': 'ScriptPromise',
    'ScriptValue': 'ScriptValue',
    # FIXME: Eliminate custom bindings for XPathNSResolver  http://crbug.com/345529
    'XPathNSResolver': 'RefPtrWillBeRawPtr<XPathNSResolver>',
    'boolean': 'bool',
}


def cpp_type(idl_type, extended_attributes=None, used_as_argument=False, will_be_in_heap_object=False):
    """Returns C++ type corresponding to IDL type.

    |idl_type| argument is of type IdlType, while return value is a string

    Args:
        idl_type:
            IdlType
        used_as_argument:
            bool, True if idl_type's raw/primitive C++ type should be returned.
        will_be_in_heap_object:
            bool, True if idl_type will be part of a possibly heap allocated
            object (e.g., appears as an element of a C++ heap vector type.)
            The C++ type of an interface type changes, if so.
    """
    def string_mode():
        # FIXME: the Web IDL spec requires 'EmptyString', not 'NullString',
        # but we use NullString for performance.
        if extended_attributes.get('TreatNullAs') != 'NullString':
            return ''
        if extended_attributes.get('TreatUndefinedAs') != 'NullString':
            return 'WithNullCheck'
        return 'WithUndefinedOrNullCheck'

    extended_attributes = extended_attributes or {}
    idl_type = preprocess_idl_type(idl_type)

    # Composite types
    array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        will_be_garbage_collected = is_will_be_garbage_collected(array_or_sequence_type)
        vector_type = 'WillBeHeapVector' if will_be_garbage_collected else 'Vector'
        return cpp_template_type(vector_type, cpp_type(array_or_sequence_type, will_be_in_heap_object=will_be_garbage_collected))

    if idl_type.is_union_type:
        return (cpp_type(member_type) for member_type in idl_type.member_types)

    # Simple types
    base_idl_type = idl_type.base_type

    if base_idl_type in CPP_TYPE_SAME_AS_IDL_TYPE:
        return base_idl_type
    if base_idl_type in CPP_INT_TYPES:
        return 'int'
    if base_idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if base_idl_type in CPP_SPECIAL_CONVERSION_RULES:
        return CPP_SPECIAL_CONVERSION_RULES[base_idl_type]

    if base_idl_type in NON_WRAPPER_TYPES:
        return 'RefPtr<%s>' % base_idl_type
    if base_idl_type == 'DOMString':
        if not used_as_argument:
            return 'String'
        return 'V8StringResource<%s>' % string_mode()

    if is_typed_array_type(idl_type) and used_as_argument:
        return base_idl_type + '*'
    if idl_type.is_interface_type:
        implemented_as_class = implemented_as(idl_type)
        if used_as_argument:
            return implemented_as_class + '*'
        if is_will_be_garbage_collected(idl_type):
            ref_ptr_type = 'RefPtrWillBeMember' if will_be_in_heap_object else 'RefPtrWillBeRawPtr'
            return cpp_template_type(ref_ptr_type, implemented_as_class)
        return cpp_template_type('RefPtr', implemented_as_class)
    # Default, assume native type is a pointer with same type name as idl type
    return base_idl_type + '*'


def cpp_template_type(template, inner_type):
    """Returns C++ template specialized to type, with space added if needed."""
    if inner_type.endswith('>'):
        format_string = '{template}<{inner_type} >'
    else:
        format_string = '{template}<{inner_type}>'
    return format_string.format(template=template, inner_type=inner_type)


def v8_type(interface_name):
    return 'V8' + interface_name


# [ImplementedAs]
# This handles [ImplementedAs] on interface types, not [ImplementedAs] in the
# interface being generated. e.g., given:
#   Foo.idl: interface Foo {attribute Bar bar};
#   Bar.idl: [ImplementedAs=Zork] interface Bar {};
# when generating bindings for Foo, the [ImplementedAs] on Bar is needed.
# This data is external to Foo.idl, and hence computed as global information in
# compute_dependencies.py to avoid having to parse IDLs of all used interfaces.
implemented_as_interfaces = {}


def implemented_as(idl_type):
    base_idl_type = idl_type.base_type
    if base_idl_type in implemented_as_interfaces:
        return implemented_as_interfaces[base_idl_type]
    return base_idl_type


def set_implemented_as_interfaces(new_implemented_as_interfaces):
    implemented_as_interfaces.update(new_implemented_as_interfaces)


# [WillBeGarbageCollected]
will_be_garbage_collected_types = set()


def is_will_be_garbage_collected(idl_type):
    return idl_type.base_type in will_be_garbage_collected_types


def set_will_be_garbage_collected_types(new_will_be_garbage_collected_types):
    will_be_garbage_collected_types.update(new_will_be_garbage_collected_types)


################################################################################
# Includes
################################################################################

def includes_for_cpp_class(class_name, relative_dir_posix):
    return set([posixpath.join('bindings', relative_dir_posix, class_name + '.h')])


INCLUDES_FOR_TYPE = {
    'object': set(),
    'CompareHow': set(),
    'Dictionary': set(['bindings/v8/Dictionary.h']),
    'EventHandler': set(['bindings/v8/V8AbstractEventListener.h',
                         'bindings/v8/V8EventListenerList.h']),
    'EventListener': set(['bindings/v8/BindingSecurity.h',
                          'bindings/v8/V8EventListenerList.h',
                          'core/frame/DOMWindow.h']),
    'MediaQueryListListener': set(['core/css/MediaQueryListListener.h']),
    'Promise': set(['bindings/v8/ScriptPromise.h']),
    'SerializedScriptValue': set(['bindings/v8/SerializedScriptValue.h']),
    'ScriptValue': set(['bindings/v8/ScriptValue.h']),
}


def includes_for_type(idl_type):
    idl_type = preprocess_idl_type(idl_type)

    # Composite types
    array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        return includes_for_type(array_or_sequence_type)

    if idl_type.is_union_type:
        return set.union(*[includes_for_type(member_type)
                           for member_type in idl_type.member_types])

    # Simple types
    base_idl_type = idl_type.base_type
    if base_idl_type in INCLUDES_FOR_TYPE:
        return INCLUDES_FOR_TYPE[base_idl_type]
    if idl_type.is_basic_type:
        return set()
    if is_typed_array_type(idl_type):
        return set(['bindings/v8/custom/V8%sCustom.h' % base_idl_type])
    if base_idl_type.endswith('ConstructorConstructor'):
        # FIXME: rename to NamedConstructor
        # FIXME: replace with a [NamedConstructorAttribute] extended attribute
        # Ending with 'ConstructorConstructor' indicates a named constructor,
        # and these do not have header files, as they are part of the generated
        # bindings for the interface
        return set()
    if base_idl_type.endswith('Constructor'):
        # FIXME: replace with a [ConstructorAttribute] extended attribute
        base_idl_type = constructor_type_name(idl_type)
    return set(['V8%s.h' % base_idl_type])


def add_includes_for_type(idl_type):
    # FIXME: make an add_includes_for_interface function so don't need to wrap
    # interface name in an IdlType
    includes.update(includes_for_type(idl_type))


################################################################################
# V8 -> C++
################################################################################

V8_VALUE_TO_CPP_VALUE = {
    # Basic
    'Date': 'toCoreDate({v8_value})',
    'DOMString': '{v8_value}',
    'boolean': '{v8_value}->BooleanValue()',
    'float': 'static_cast<float>({v8_value}->NumberValue())',
    'double': 'static_cast<double>({v8_value}->NumberValue())',
    'byte': 'toInt8({arguments})',
    'octet': 'toUInt8({arguments})',
    'short': 'toInt16({arguments})',
    'unsigned short': 'toUInt16({arguments})',
    'long': 'toInt32({arguments})',
    'unsigned long': 'toUInt32({arguments})',
    'long long': 'toInt64({arguments})',
    'unsigned long long': 'toUInt64({arguments})',
    # Interface types
    'CompareHow': 'static_cast<Range::CompareHow>({v8_value}->Int32Value())',
    'Dictionary': 'Dictionary({v8_value}, info.GetIsolate())',
    'EventTarget': 'V8DOMWrapper::isDOMWrapper({v8_value}) ? toWrapperTypeInfo(v8::Handle<v8::Object>::Cast({v8_value}))->toEventTarget(v8::Handle<v8::Object>::Cast({v8_value})) : 0',
    'MediaQueryListListener': 'MediaQueryListListener::create(ScriptValue({v8_value}, info.GetIsolate()))',
    'NodeFilter': 'toNodeFilter({v8_value}, info.GetIsolate())',
    'Promise': 'ScriptPromise({v8_value}, info.GetIsolate())',
    'SerializedScriptValue': 'SerializedScriptValue::create({v8_value}, info.GetIsolate())',
    'ScriptValue': 'ScriptValue({v8_value}, info.GetIsolate())',
    'Window': 'toDOMWindow({v8_value}, info.GetIsolate())',
    'XPathNSResolver': 'toXPathNSResolver({v8_value}, info.GetIsolate())',
}


def v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, index):
    # Composite types
    array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        return v8_value_to_cpp_value_array_or_sequence(array_or_sequence_type, v8_value, index)

    # Simple types
    idl_type = preprocess_idl_type(idl_type)
    add_includes_for_type(idl_type)
    base_idl_type = idl_type.base_type

    if 'EnforceRange' in extended_attributes:
        arguments = ', '.join([v8_value, 'EnforceRange', 'exceptionState'])
    elif idl_type.is_integer_type:  # NormalConversion
        arguments = ', '.join([v8_value, 'exceptionState'])
    else:
        arguments = v8_value

    if base_idl_type in V8_VALUE_TO_CPP_VALUE:
        cpp_expression_format = V8_VALUE_TO_CPP_VALUE[base_idl_type]
    elif is_typed_array_type(idl_type):
        cpp_expression_format = (
            '{v8_value}->Is{idl_type}() ? '
            'V8{idl_type}::toNative(v8::Handle<v8::{idl_type}>::Cast({v8_value})) : 0')
    else:
        cpp_expression_format = (
            'V8{idl_type}::toNativeWithTypeCheck(info.GetIsolate(), {v8_value})')

    return cpp_expression_format.format(arguments=arguments, idl_type=base_idl_type, v8_value=v8_value)


def v8_value_to_cpp_value_array_or_sequence(array_or_sequence_type, v8_value, index):
    # Index is None for setters, index (starting at 0) for method arguments,
    # and is used to provide a human-readable exception message
    if index is None:
        index = 0  # special case, meaning "setter"
    else:
        index += 1  # human-readable index
    if (array_or_sequence_type.is_interface_type and
        array_or_sequence_type.name != 'Dictionary'):
        this_cpp_type = None
        ref_ptr_type = 'Member' if is_will_be_garbage_collected(array_or_sequence_type) else 'RefPtr'
        expression_format = '(to{ref_ptr_type}NativeArray<{array_or_sequence_type}, V8{array_or_sequence_type}>({v8_value}, {index}, info.GetIsolate()))'
        add_includes_for_type(array_or_sequence_type)
    else:
        ref_ptr_type = None
        this_cpp_type = cpp_type(array_or_sequence_type)
        expression_format = 'toNativeArray<{cpp_type}>({v8_value}, {index}, info.GetIsolate())'
    expression = expression_format.format(array_or_sequence_type=array_or_sequence_type.name, cpp_type=this_cpp_type, index=index, ref_ptr_type=ref_ptr_type, v8_value=v8_value)
    return expression


def v8_value_to_local_cpp_value(idl_type, extended_attributes, v8_value, variable_name, index=None):
    """Returns an expression that converts a V8 value to a C++ value and stores it as a local value."""
    this_cpp_type = cpp_type(idl_type, extended_attributes=extended_attributes, used_as_argument=True)

    idl_type = preprocess_idl_type(idl_type)
    if idl_type.base_type == 'DOMString' and not idl_type.array_or_sequence_type:
        format_string = 'V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID({cpp_type}, {variable_name}, {cpp_value})'
    elif idl_type.is_integer_type:
        format_string = 'V8TRYCATCH_EXCEPTION_VOID({cpp_type}, {variable_name}, {cpp_value}, exceptionState)'
    else:
        format_string = 'V8TRYCATCH_VOID({cpp_type}, {variable_name}, {cpp_value})'

    cpp_value = v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, index)
    return format_string.format(cpp_type=this_cpp_type, cpp_value=cpp_value, variable_name=variable_name)


################################################################################
# C++ -> V8
################################################################################

def preprocess_idl_type(idl_type):
    if idl_type.is_enum:
        # Enumerations are internally DOMStrings
        return IdlType('DOMString')
    if (idl_type.name == 'Any' or idl_type.is_callback_function):
        return IdlType('ScriptValue')
    return idl_type


def preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes):
    """Returns IDL type and value, with preliminary type conversions applied."""
    idl_type = preprocess_idl_type(idl_type)
    if idl_type.name == 'Promise':
        idl_type = IdlType('ScriptValue')
    if idl_type.base_type in ['long long', 'unsigned long long']:
        # long long and unsigned long long are not representable in ECMAScript;
        # we represent them as doubles.
        idl_type = IdlType('double', is_nullable=idl_type.is_nullable)
        cpp_value = 'static_cast<double>(%s)' % cpp_value
    # HTML5 says that unsigned reflected attributes should be in the range
    # [0, 2^31). When a value isn't in this range, a default value (or 0)
    # should be returned instead.
    extended_attributes = extended_attributes or {}
    if ('Reflect' in extended_attributes and
        idl_type.base_type in ['unsigned long', 'unsigned short']):
        cpp_value = cpp_value.replace('getUnsignedIntegralAttribute',
                                      'getIntegralAttribute')
        cpp_value = 'std::max(0, %s)' % cpp_value
    return idl_type, cpp_value


def v8_conversion_type(idl_type, extended_attributes):
    """Returns V8 conversion type, adding any additional includes.

    The V8 conversion type is used to select the C++ -> V8 conversion function
    or v8SetReturnValue* function; it can be an idl_type, a cpp_type, or a
    separate name for the type of conversion (e.g., 'DOMWrapper').
    """
    extended_attributes = extended_attributes or {}

    # Composite types
    array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        if array_or_sequence_type.is_interface_type:
            add_includes_for_type(array_or_sequence_type)
        return 'array'

    # Simple types
    base_idl_type = idl_type.base_type
    # Basic types, without additional includes
    if base_idl_type in CPP_INT_TYPES:
        return 'int'
    if base_idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if base_idl_type == 'DOMString':
        if 'TreatReturnedNullStringAs' not in extended_attributes:
            return 'DOMString'
        treat_returned_null_string_as = extended_attributes['TreatReturnedNullStringAs']
        if treat_returned_null_string_as == 'Null':
            return 'StringOrNull'
        if treat_returned_null_string_as == 'Undefined':
            return 'StringOrUndefined'
        raise 'Unrecognized TreatReturnNullStringAs value: "%s"' % treat_returned_null_string_as
    if idl_type.is_basic_type or base_idl_type == 'ScriptValue':
        return base_idl_type

    # Data type with potential additional includes
    add_includes_for_type(idl_type)
    if base_idl_type in V8_SET_RETURN_VALUE:  # Special v8SetReturnValue treatment
        return base_idl_type

    # Pointer type
    return 'DOMWrapper'


V8_SET_RETURN_VALUE = {
    'boolean': 'v8SetReturnValueBool(info, {cpp_value})',
    'int': 'v8SetReturnValueInt(info, {cpp_value})',
    'unsigned': 'v8SetReturnValueUnsigned(info, {cpp_value})',
    'DOMString': 'v8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    # [TreatNullReturnValueAs]
    'StringOrNull': 'v8SetReturnValueStringOrNull(info, {cpp_value}, info.GetIsolate())',
    'StringOrUndefined': 'v8SetReturnValueStringOrUndefined(info, {cpp_value}, info.GetIsolate())',
    'void': '',
    # No special v8SetReturnValue* function (set value directly)
    'float': 'v8SetReturnValue(info, {cpp_value})',
    'double': 'v8SetReturnValue(info, {cpp_value})',
    # No special v8SetReturnValue* function, but instead convert value to V8
    # and then use general v8SetReturnValue.
    'array': 'v8SetReturnValue(info, {cpp_value})',
    'Date': 'v8SetReturnValue(info, {cpp_value})',
    'EventHandler': 'v8SetReturnValue(info, {cpp_value})',
    'ScriptValue': 'v8SetReturnValue(info, {cpp_value})',
    'SerializedScriptValue': 'v8SetReturnValue(info, {cpp_value})',
    # DOMWrapper
    'DOMWrapperForMainWorld': 'v8SetReturnValueForMainWorld(info, WTF::getPtr({cpp_value}))',
    'DOMWrapperFast': 'v8SetReturnValueFast(info, WTF::getPtr({cpp_value}), {script_wrappable})',
    'DOMWrapperDefault': 'v8SetReturnValue(info, {cpp_value})',
}


def v8_set_return_value(idl_type, cpp_value, extended_attributes=None, script_wrappable='', release=False, for_main_world=False):
    """Returns a statement that converts a C++ value to a V8 value and sets it as a return value.

    release: for union types, can be either False (False for all member types)
             or a sequence (list or tuple) of booleans (if specified
             individually).
    """
    def dom_wrapper_conversion_type():
        if not script_wrappable:
            return 'DOMWrapperDefault'
        if for_main_world:
            return 'DOMWrapperForMainWorld'
        return 'DOMWrapperFast'

    if idl_type.is_union_type:
        return [
            v8_set_return_value(member_type,
                                cpp_value + str(i),
                                extended_attributes,
                                script_wrappable,
                                release and release[i])
                for i, member_type in
                enumerate(idl_type.member_types)]
    idl_type, cpp_value = preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = v8_conversion_type(idl_type, extended_attributes)
    # SetReturn-specific overrides
    if this_v8_conversion_type in ['Date', 'EventHandler', 'ScriptValue', 'SerializedScriptValue', 'array']:
        # Convert value to V8 and then use general v8SetReturnValue
        cpp_value = cpp_value_to_v8_value(idl_type, cpp_value, extended_attributes=extended_attributes)
    if this_v8_conversion_type == 'DOMWrapper':
        this_v8_conversion_type = dom_wrapper_conversion_type()

    format_string = V8_SET_RETURN_VALUE[this_v8_conversion_type]
    if release:
        cpp_value = '%s.release()' % cpp_value
    statement = format_string.format(cpp_value=cpp_value, script_wrappable=script_wrappable)
    return statement


CPP_VALUE_TO_V8_VALUE = {
    # Built-in types
    'Date': 'v8DateOrNull({cpp_value}, {isolate})',
    'DOMString': 'v8String({isolate}, {cpp_value})',
    'boolean': 'v8Boolean({cpp_value}, {isolate})',
    'int': 'v8::Integer::New({isolate}, {cpp_value})',
    'unsigned': 'v8::Integer::NewFromUnsigned({isolate}, {cpp_value})',
    'float': 'v8::Number::New({isolate}, {cpp_value})',
    'double': 'v8::Number::New({isolate}, {cpp_value})',
    'void': 'v8Undefined()',
    # Special cases
    'EventHandler': '{cpp_value} ? v8::Handle<v8::Value>(V8AbstractEventListener::cast({cpp_value})->getListenerObject(imp->executionContext())) : v8::Handle<v8::Value>(v8::Null({isolate}))',
    'ScriptValue': '{cpp_value}.v8Value()',
    'SerializedScriptValue': '{cpp_value} ? {cpp_value}->deserialize() : v8::Handle<v8::Value>(v8::Null({isolate}))',
    # General
    'array': 'v8Array({cpp_value}, {isolate})',
    'DOMWrapper': 'toV8({cpp_value}, {creation_context}, {isolate})',
}


def cpp_value_to_v8_value(idl_type, cpp_value, isolate='info.GetIsolate()', creation_context='', extended_attributes=None):
    """Returns an expression that converts a C++ value to a V8 value."""
    # the isolate parameter is needed for callback interfaces
    idl_type, cpp_value = preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = v8_conversion_type(idl_type, extended_attributes)
    format_string = CPP_VALUE_TO_V8_VALUE[this_v8_conversion_type]
    statement = format_string.format(cpp_value=cpp_value, isolate=isolate, creation_context=creation_context)
    return statement
