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

"""Generate template values for methods.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import idl_types
from v8_globals import includes
import v8_types
import v8_utilities
from v8_utilities import has_extended_attribute_value


def generate_method(interface, method):
    arguments = method.arguments
    extended_attributes = method.extended_attributes
    idl_type = method.idl_type
    is_static = method.is_static
    name = method.name

    v8_types.add_includes_for_type(idl_type)
    this_cpp_value = cpp_value(interface, method, len(arguments))

    def function_template():
        if is_static:
            return 'functionTemplate'
        if 'Unforgeable' in extended_attributes:
            return 'instanceTemplate'
        return 'prototypeTemplate'

    is_call_with_script_arguments = has_extended_attribute_value(method, 'CallWith', 'ScriptArguments')
    if is_call_with_script_arguments:
        includes.update(['bindings/v8/ScriptCallStackFactory.h',
                         'core/inspector/ScriptArguments.h'])
    is_call_with_script_state = has_extended_attribute_value(method, 'CallWith', 'ScriptState')
    if is_call_with_script_state:
        includes.add('bindings/v8/ScriptState.h')
    is_check_security_for_node = 'CheckSecurity' in extended_attributes
    if is_check_security_for_node:
        includes.add('bindings/v8/BindingSecurity.h')
    is_custom_element_callbacks = 'CustomElementCallbacks' in extended_attributes
    if is_custom_element_callbacks:
        includes.add('core/dom/custom/CustomElementCallbackDispatcher.h')

    is_check_security_for_frame = (
        'CheckSecurity' in interface.extended_attributes and
        'DoNotCheckSecurity' not in extended_attributes)
    is_raises_exception = 'RaisesException' in extended_attributes

    return {
        'activity_logging_world_list': v8_utilities.activity_logging_world_list(method),  # [ActivityLogging]
        'arguments': [generate_argument(interface, method, argument, index)
                      for index, argument in enumerate(arguments)],
        'conditional_string': v8_utilities.conditional_string(method),
        'cpp_type': v8_types.cpp_type(idl_type),
        'cpp_value': this_cpp_value,
        'deprecate_as': v8_utilities.deprecate_as(method),  # [DeprecateAs]
        'do_not_check_signature': not(is_static or
            v8_utilities.has_extended_attribute(method,
                ['DoNotCheckSecurity', 'DoNotCheckSignature', 'NotEnumerable',
                 'ReadOnly', 'RuntimeEnabled', 'Unforgeable'])),
        'function_template': function_template(),
        'idl_type': idl_type.base_type,
        'has_exception_state':
            is_raises_exception or
            is_check_security_for_frame or
            any(argument for argument in arguments
                if argument.idl_type.name == 'SerializedScriptValue' or
                   argument.idl_type.is_integer_type) or
            name in ['addEventListener', 'removeEventListener', 'dispatchEvent'],
        'is_call_with_execution_context': has_extended_attribute_value(method, 'CallWith', 'ExecutionContext'),
        'is_call_with_script_arguments': is_call_with_script_arguments,
        'is_call_with_script_state': is_call_with_script_state,
        'is_check_security_for_frame': is_check_security_for_frame,
        'is_check_security_for_node': is_check_security_for_node,
        'is_custom': 'Custom' in extended_attributes,
        'is_custom_element_callbacks': is_custom_element_callbacks,
        'is_do_not_check_security': 'DoNotCheckSecurity' in extended_attributes,
        'is_do_not_check_signature': 'DoNotCheckSignature' in extended_attributes,
        'is_implemented_by': 'ImplementedBy' in extended_attributes,
        'is_per_world_bindings': 'PerWorldBindings' in extended_attributes,
        'is_raises_exception': is_raises_exception,
        'is_read_only': 'ReadOnly' in extended_attributes,
        'is_static': is_static,
        'is_strict_type_checking':
            'StrictTypeChecking' in extended_attributes or
            'StrictTypeChecking' in interface.extended_attributes,
        'is_variadic': arguments and arguments[-1].is_variadic,
        'measure_as': v8_utilities.measure_as(method),  # [MeasureAs]
        'name': name,
        'number_of_arguments': len(arguments),
        'number_of_required_arguments': len([
            argument for argument in arguments
            if not (argument.is_optional or argument.is_variadic)]),
        'number_of_required_or_variadic_arguments': len([
            argument for argument in arguments
            if not argument.is_optional]),
        'per_context_enabled_function': v8_utilities.per_context_enabled_function_name(method),  # [PerContextEnabled]
        'property_attributes': property_attributes(method),
        'runtime_enabled_function': v8_utilities.runtime_enabled_function_name(method),  # [RuntimeEnabled]
        'signature': 'v8::Local<v8::Signature>()' if is_static or 'DoNotCheckSignature' in extended_attributes else 'defaultSignature',
        'union_arguments': union_arguments(idl_type),
        'v8_set_return_value_for_main_world': v8_set_return_value(interface.name, method, this_cpp_value, for_main_world=True),
        'v8_set_return_value': v8_set_return_value(interface.name, method, this_cpp_value),
        'world_suffixes': ['', 'ForMainWorld'] if 'PerWorldBindings' in extended_attributes else [''],  # [PerWorldBindings]
    }


def generate_argument(interface, method, argument, index):
    extended_attributes = argument.extended_attributes
    idl_type = argument.idl_type
    this_cpp_value = cpp_value(interface, method, index)
    is_variadic_wrapper_type = argument.is_variadic and v8_types.is_wrapper_type(idl_type)
    use_heap_vector_type = is_variadic_wrapper_type and v8_types.is_will_be_garbage_collected(idl_type)
    return {
        'cpp_type': v8_types.cpp_type(idl_type, will_be_in_heap_object=use_heap_vector_type),
        'cpp_value': this_cpp_value,
        'enum_validation_expression': v8_utilities.enum_validation_expression(idl_type),
        'has_default': 'Default' in extended_attributes,
        'idl_type_object': idl_type,
        # Dictionary is special-cased, but arrays and sequences shouldn't be
        'idl_type': not idl_type.array_or_sequence_type and idl_type.base_type,
        'index': index,
        'is_clamp': 'Clamp' in extended_attributes,
        'is_callback_interface': idl_type.is_callback_interface,
        'is_nullable': idl_type.is_nullable,
        'is_optional': argument.is_optional,
        'is_strict_type_checking': 'StrictTypeChecking' in extended_attributes,
        'is_variadic_wrapper_type': is_variadic_wrapper_type,
        'vector_type': 'WillBeHeapVector' if use_heap_vector_type else 'Vector',
        'is_wrapper_type': v8_types.is_wrapper_type(idl_type),
        'name': argument.name,
        'v8_set_return_value_for_main_world': v8_set_return_value(interface.name, method, this_cpp_value, for_main_world=True),
        'v8_set_return_value': v8_set_return_value(interface.name, method, this_cpp_value),
        'v8_value_to_local_cpp_value': v8_value_to_local_cpp_value(argument, index),
    }


################################################################################
# Value handling
################################################################################

def cpp_value(interface, method, number_of_arguments):
    def cpp_argument(argument):
        idl_type = argument.idl_type
        if (idl_type.is_callback_interface or
            idl_type.name in ['NodeFilter', 'XPathNSResolver']):
            # FIXME: remove this special case
            return '%s.release()' % argument.name
        return argument.name

    # Truncate omitted optional arguments
    arguments = method.arguments[:number_of_arguments]
    cpp_arguments = v8_utilities.call_with_arguments(method)
    if ('ImplementedBy' in method.extended_attributes and
        not method.is_static):
        cpp_arguments.append('*imp')
    cpp_arguments.extend(cpp_argument(argument) for argument in arguments)
    this_union_arguments = union_arguments(method.idl_type)
    if this_union_arguments:
        cpp_arguments.extend(this_union_arguments)

    if 'RaisesException' in method.extended_attributes:
        cpp_arguments.append('exceptionState')

    cpp_method_name = v8_utilities.scoped_name(interface, method, v8_utilities.cpp_name(method))
    return '%s(%s)' % (cpp_method_name, ', '.join(cpp_arguments))


def v8_set_return_value(interface_name, method, cpp_value, for_main_world=False):
    idl_type = method.idl_type
    extended_attributes = method.extended_attributes
    if idl_type.name == 'void':
        return None
    is_union_type = idl_type.is_union_type

    release = False
    # [CallWith=ScriptState], [RaisesException]
    if (has_extended_attribute_value(method, 'CallWith', 'ScriptState') or
        'RaisesException' in extended_attributes or
        is_union_type):
        # use local variable for value
        cpp_value = 'result'

        if is_union_type:
            release = [member_type.is_interface_type
                       for member_type in idl_type.member_types]
        else:
            release = idl_type.is_interface_type

    script_wrappable = 'imp' if idl_types.inherits_interface(interface_name, 'Node') else ''
    return v8_types.v8_set_return_value(idl_type, cpp_value, extended_attributes, script_wrappable=script_wrappable, release=release, for_main_world=for_main_world)


def v8_value_to_local_cpp_value(argument, index):
    extended_attributes = argument.extended_attributes
    idl_type = argument.idl_type
    name = argument.name
    if argument.is_variadic:
        vector_type = 'WillBeHeapVector' if v8_types.is_will_be_garbage_collected(idl_type) else 'Vector'
        return 'V8TRYCATCH_VOID({vector_type}<{cpp_type}>, {name}, toNativeArguments<{cpp_type}>(info, {index}))'.format(
                cpp_type=v8_types.cpp_type(idl_type), name=name, index=index, vector_type=vector_type)
    # [Default=NullString]
    if (argument.is_optional and idl_type.name == 'String' and
        extended_attributes.get('Default') == 'NullString'):
        v8_value = 'argumentOrNull(info, %s)' % index
    else:
        v8_value = 'info[%s]' % index
    return v8_types.v8_value_to_local_cpp_value(
        idl_type, argument.extended_attributes, v8_value, name, index=index)


################################################################################
# Auxiliary functions
################################################################################

# [NotEnumerable]
def property_attributes(method):
    extended_attributes = method.extended_attributes
    property_attributes_list = []
    if 'NotEnumerable' in extended_attributes:
        property_attributes_list.append('v8::DontEnum')
    if 'ReadOnly' in extended_attributes:
        property_attributes_list.append('v8::ReadOnly')
    if property_attributes_list:
        property_attributes_list.insert(0, 'v8::DontDelete')
    return property_attributes_list


def union_arguments(idl_type):
    """Return list of ['result0Enabled', 'result0', 'result1Enabled', ...] for union types, for use in setting return value"""
    if not idl_type.is_union_type:
        return None
    return [arg
            for i in range(len(idl_type.member_types))
            for arg in ['result%sEnabled' % i, 'result%s' % i]]
