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

"""Blink IDL Intermediate Representation (IR) classes.

Classes are primarily constructors, which build an IdlDefinitions object
(and various contained objects) from an AST (produced by blink_idl_parser).

This is in two steps:
* Constructors walk the AST, creating objects.
* Typedef resolution.

Typedefs are all resolved here, and not stored in IR.

Typedef resolution uses some auxiliary classes and OOP techniques to make this
a generic call, via the resolve_typedefs() method.

Class hierarchy (mostly containment, '<' for inheritance):

IdlDefinitions
    IdlCallbackFunction < TypedObject
    IdlEnum :: FIXME: remove, just use a dict for enums
    IdlInterface
        IdlAttribute < TypedObject
        IdlConstant < TypedObject
        IdlOperation < TypedObject
            IdlArgument < TypedObject
    IdlException < IdlInterface
        (same contents as IdlInterface)

TypedObject :: mixin for typedef resolution

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import abc

from idl_types import IdlType, IdlUnionType

SPECIAL_KEYWORD_LIST = ['GETTER', 'SETTER', 'DELETER']
STANDARD_TYPEDEFS = {
    # http://www.w3.org/TR/WebIDL/#common-DOMTimeStamp
    'DOMTimeStamp': 'unsigned long long',
}


################################################################################
# TypedObject (mixin for typedef resolution)
################################################################################

class TypedObject(object):
    """Object with a type, such as an Attribute or Operation (return value).

    The type can be an actual type, or can be a typedef, which must be resolved
    before passing data to the code generator.
    """
    __metaclass__ = abc.ABCMeta
    idl_type = None

    def resolve_typedefs(self, typedefs):
        """Resolve typedefs to actual types in the object."""
        # Constructors don't have their own return type, because it's the
        # interface itself.
        if not self.idl_type:
            return
        # Need to re-assign self.idl_type, not just mutate idl_type,
        # since type(idl_type) may change.
        self.idl_type = self.idl_type.resolve_typedefs(typedefs)


################################################################################
# Definitions (main container class)
################################################################################

class IdlDefinitions(object):
    def __init__(self, node):
        """Args: node: AST root node, class == 'File'"""
        self.callback_functions = {}
        self.enumerations = {}
        self.interfaces = {}

        node_class = node.GetClass()
        if node_class != 'File':
            raise ValueError('Unrecognized node class: %s' % node_class)

        typedefs = dict((typedef_name, IdlType(type_name))
                        for typedef_name, type_name in
                        STANDARD_TYPEDEFS.iteritems())

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Interface':
                interface = IdlInterface(child)
                self.interfaces[interface.name] = interface
            elif child_class == 'Exception':
                exception = IdlException(child)
                # For simplicity, treat exceptions as interfaces
                self.interfaces[exception.name] = exception
            elif child_class == 'Typedef':
                type_name = child.GetName()
                typedefs[type_name] = typedef_node_to_type(child)
            elif child_class == 'Enum':
                enumeration = IdlEnum(child)
                self.enumerations[enumeration.name] = enumeration
            elif child_class == 'Callback':
                callback_function = IdlCallbackFunction(child)
                self.callback_functions[callback_function.name] = callback_function
            elif child_class == 'Implements':
                # Implements is handled at the interface merging step
                pass
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

        # Typedefs are not stored in IR:
        # Resolve typedefs with the actual types and then discard the Typedefs.
        # http://www.w3.org/TR/WebIDL/#idl-typedefs
        self.resolve_typedefs(typedefs)

    def resolve_typedefs(self, typedefs):
        for callback_function in self.callback_functions.itervalues():
            callback_function.resolve_typedefs(typedefs)
        for interface in self.interfaces.itervalues():
            interface.resolve_typedefs(typedefs)


################################################################################
# Callback Functions
################################################################################

class IdlCallbackFunction(TypedObject):
    def __init__(self, node):
        children = node.GetChildren()
        num_children = len(children)
        if num_children != 2:
            raise ValueError('Expected 2 children, got %s' % num_children)
        type_node, arguments_node = children
        arguments_node_class = arguments_node.GetClass()
        if arguments_node_class != 'Arguments':
            raise ValueError('Expected Arguments node, got %s' % arguments_node_class)

        self.name = node.GetName()
        self.idl_type = type_node_to_type(type_node)
        self.arguments = arguments_node_to_arguments(arguments_node)

    def resolve_typedefs(self, typedefs):
        TypedObject.resolve_typedefs(self, typedefs)
        for argument in self.arguments:
            argument.resolve_typedefs(typedefs)


################################################################################
# Enumerations
################################################################################

class IdlEnum(object):
    # FIXME: remove, just treat enums as a dictionary
    def __init__(self, node):
        self.name = node.GetName()
        self.values = []
        for child in node.GetChildren():
            self.values.append(child.GetName())


################################################################################
# Interfaces and Exceptions
################################################################################

class IdlInterface(object):
    def __init__(self, node=None):
        self.attributes = []
        self.constants = []
        self.constructors = []
        self.custom_constructors = []
        self.extended_attributes = {}
        self.operations = []
        self.parent = None
        if not node:  # Early exit for IdlException.__init__
            return

        self.is_callback = node.GetProperty('CALLBACK') or False
        self.is_exception = False
        # FIXME: uppercase 'Partial' => 'PARTIAL' in base IDL parser
        self.is_partial = node.GetProperty('Partial') or False
        self.name = node.GetName()

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Attribute':
                self.attributes.append(IdlAttribute(child))
            elif child_class == 'Const':
                self.constants.append(IdlConstant(child))
            elif child_class == 'ExtAttributes':
                extended_attributes = ext_attributes_node_to_extended_attributes(child)
                self.constructors, self.custom_constructors = (
                    extended_attributes_to_constructors(extended_attributes))
                clear_constructor_attributes(extended_attributes)
                self.extended_attributes = extended_attributes
            elif child_class == 'Operation':
                self.operations.append(IdlOperation(child))
            elif child_class == 'Inherit':
                self.parent = child.GetName()
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

    def resolve_typedefs(self, typedefs):
        for attribute in self.attributes:
            attribute.resolve_typedefs(typedefs)
        for constant in self.constants:
            constant.resolve_typedefs(typedefs)
        for constructor in self.constructors:
            constructor.resolve_typedefs(typedefs)
        for custom_constructor in self.custom_constructors:
            custom_constructor.resolve_typedefs(typedefs)
        for operation in self.operations:
            operation.resolve_typedefs(typedefs)


class IdlException(IdlInterface):
    # Properly exceptions and interfaces are distinct, and thus should inherit a
    # common base class (say, "IdlExceptionOrInterface").
    # However, there is only one exception (DOMException), and new exceptions
    # are not expected. Thus it is easier to implement exceptions as a
    # restricted subclass of interfaces.
    # http://www.w3.org/TR/WebIDL/#idl-exceptions
    def __init__(self, node):
        # Exceptions are similar to Interfaces, but simpler
        IdlInterface.__init__(self)
        self.is_callback = False
        self.is_exception = True
        self.is_partial = False
        self.name = node.GetName()

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Attribute':
                attribute = IdlAttribute(child)
                self.attributes.append(attribute)
            elif child_class == 'Const':
                self.constants.append(IdlConstant(child))
            elif child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            elif child_class == 'ExceptionOperation':
                self.operations.append(IdlOperation.from_exception_operation_node(child))
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)


################################################################################
# Attributes
################################################################################

class IdlAttribute(TypedObject):
    def __init__(self, node):
        self.is_read_only = node.GetProperty('READONLY') or False
        self.is_static = node.GetProperty('STATIC') or False
        self.name = node.GetName()
        # Defaults, overridden below
        self.idl_type = None
        self.extended_attributes = {}

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Type':
                self.idl_type = type_node_to_type(child)
            elif child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)


################################################################################
# Constants
################################################################################

class IdlConstant(TypedObject):
    def __init__(self, node):
        children = node.GetChildren()
        num_children = len(children)
        if num_children < 2 or num_children > 3:
            raise ValueError('Expected 2 or 3 children, got %s' % num_children)
        type_node = children[0]
        value_node = children[1]
        value_node_class = value_node.GetClass()
        if value_node_class != 'Value':
            raise ValueError('Expected Value node, got %s' % value_node_class)

        self.name = node.GetName()
        # ConstType is more limited than Type, so subtree is smaller and
        # we don't use the full type_node_to_type function.
        self.idl_type = type_node_inner_to_type(type_node)
        self.value = value_node.GetName()

        if num_children == 3:
            ext_attributes_node = children[2]
            self.extended_attributes = ext_attributes_node_to_extended_attributes(ext_attributes_node)
        else:
            self.extended_attributes = {}


################################################################################
# Operations
################################################################################

class IdlOperation(TypedObject):
    def __init__(self, node=None):
        self.arguments = []
        self.extended_attributes = {}
        self.specials = []

        if not node:
            self.is_static = False
            return
        self.name = node.GetName()  # FIXME: should just be: or ''
        # FIXME: AST should use None internally
        if self.name == '_unnamed_':
            self.name = ''

        self.is_static = node.GetProperty('STATIC') or False
        property_dictionary = node.GetProperties()
        for special_keyword in SPECIAL_KEYWORD_LIST:
            if special_keyword in property_dictionary:
                self.specials.append(special_keyword.lower())

        self.idl_type = None
        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Arguments':
                self.arguments = arguments_node_to_arguments(child)
            elif child_class == 'Type':
                self.idl_type = type_node_to_type(child)
            elif child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

    @classmethod
    def from_exception_operation_node(cls, node):
        # Needed to handle one case in DOMException.idl:
        # // Override in a Mozilla compatible format
        # [NotEnumerable] DOMString toString();
        # FIXME: can we remove this? replace with a stringifier?
        operation = cls()
        operation.name = node.GetName()
        children = node.GetChildren()
        if len(children) < 1 or len(children) > 2:
            raise ValueError('ExceptionOperation node with %s children, expected 1 or 2' % len(children))

        type_node = children[0]
        operation.idl_type = type_node_to_type(type_node)

        if len(children) > 1:
            ext_attributes_node = children[1]
            operation.extended_attributes = ext_attributes_node_to_extended_attributes(ext_attributes_node)

        return operation

    @classmethod
    def constructor_from_arguments_node(cls, name, arguments_node):
        constructor = cls()
        constructor.name = name
        constructor.arguments = arguments_node_to_arguments(arguments_node)
        return constructor

    def resolve_typedefs(self, typedefs):
        TypedObject.resolve_typedefs(self, typedefs)
        for argument in self.arguments:
            argument.resolve_typedefs(typedefs)


################################################################################
# Arguments
################################################################################

class IdlArgument(TypedObject):
    def __init__(self, node):
        self.extended_attributes = {}
        self.idl_type = None
        self.is_optional = node.GetProperty('OPTIONAL')  # syntax: (optional T)
        self.is_variadic = False  # syntax: (T...)
        self.name = node.GetName()

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Type':
                self.idl_type = type_node_to_type(child)
            elif child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            elif child_class == 'Argument':
                child_name = child.GetName()
                if child_name != '...':
                    raise ValueError('Unrecognized Argument node; expected "...", got "%s"' % child_name)
                self.is_variadic = child.GetProperty('ELLIPSIS') or False
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)


def arguments_node_to_arguments(node):
    # [Constructor] and [CustomConstructor] without arguments (the bare form)
    # have None instead of an arguments node, but have the same meaning as using
    # an empty argument list, [Constructor()], so special-case this.
    # http://www.w3.org/TR/WebIDL/#Constructor
    if node is None:
        return []
    return [IdlArgument(argument_node)
            for argument_node in node.GetChildren()]


################################################################################
# Extended attributes
################################################################################

def ext_attributes_node_to_extended_attributes(node):
    """
    Returns:
      Dictionary of {ExtAttributeName: ExtAttributeValue}.
      Value is usually a string, with three exceptions:
      Constructors: value is a list of Arguments nodes, corresponding to
        possible signatures of the constructor.
      CustomConstructors: value is a list of Arguments nodes, corresponding to
        possible signatures of the custom constructor.
      NamedConstructor: value is a Call node, corresponding to the single
        signature of the named constructor.
    """
    # Primarily just make a dictionary from the children.
    # The only complexity is handling various types of constructors:
    # Constructors and Custom Constructors can have duplicate entries due to
    # overloading, and thus are stored in temporary lists.
    # However, Named Constructors cannot be overloaded, and thus do not have
    # a list.
    # FIXME: move Constructor logic into separate function, instead of modifying
    #        extended attributes in-place.
    constructors = []
    custom_constructors = []
    extended_attributes = {}

    def child_node(extended_attribute_node):
        children = extended_attribute_node.GetChildren()
        if not children:
            return None
        if len(children) > 1:
            raise ValueError('ExtAttributes node with %s children, expected at most 1' % len(children))
        return children[0]

    extended_attribute_node_list = node.GetChildren()
    for extended_attribute_node in extended_attribute_node_list:
        name = extended_attribute_node.GetName()
        child = child_node(extended_attribute_node)
        child_class = child and child.GetClass()
        if name == 'Constructor':
            if child_class and child_class != 'Arguments':
                raise ValueError('Constructor only supports Arguments as child, but has child of class: %s' % child_class)
            constructors.append(child)
        elif name == 'CustomConstructor':
            if child_class and child_class != 'Arguments':
                raise ValueError('[CustomConstructor] only supports Arguments as child, but has child of class: %s' % child_class)
            custom_constructors.append(child)
        elif name == 'NamedConstructor':
            if child_class and child_class != 'Call':
                raise ValueError('[NamedConstructor] only supports Call as child, but has child of class: %s' % child_class)
            extended_attributes[name] = child
        elif name == 'SetWrapperReferenceTo':
            if not child:
                raise ValueError('[SetWrapperReferenceTo] requires a child, but has none.')
            if child_class != 'Arguments':
                raise ValueError('[SetWrapperReferenceTo] only supports Arguments as child, but has child of class: %s' % child_class)
            extended_attributes[name] = arguments_node_to_arguments(child)
        elif child:
            raise ValueError('ExtAttributes node with unexpected children: %s' % name)
        else:
            value = extended_attribute_node.GetProperty('VALUE')
            extended_attributes[name] = value

    # Store constructors and custom constructors in special list attributes,
    # which are deleted later. Note plural in key.
    if constructors:
        extended_attributes['Constructors'] = constructors
    if custom_constructors:
        extended_attributes['CustomConstructors'] = custom_constructors

    return extended_attributes


def extended_attributes_to_constructors(extended_attributes):
    """Returns constructors and custom_constructors (lists of IdlOperations).

    Auxiliary function for IdlInterface.__init__.
    """

    constructor_list = extended_attributes.get('Constructors', [])
    constructors = [
        IdlOperation.constructor_from_arguments_node('Constructor', arguments_node)
        for arguments_node in constructor_list]

    custom_constructor_list = extended_attributes.get('CustomConstructors', [])
    custom_constructors = [
        IdlOperation.constructor_from_arguments_node('CustomConstructor', arguments_node)
        for arguments_node in custom_constructor_list]

    if 'NamedConstructor' in extended_attributes:
        # FIXME: support overloaded named constructors, and make homogeneous
        name = 'NamedConstructor'
        call_node = extended_attributes['NamedConstructor']
        extended_attributes['NamedConstructor'] = call_node.GetName()
        children = call_node.GetChildren()
        if len(children) != 1:
            raise ValueError('NamedConstructor node expects 1 child, got %s.' % len(children))
        arguments_node = children[0]
        named_constructor = IdlOperation.constructor_from_arguments_node('NamedConstructor', arguments_node)
        # FIXME: should return named_constructor separately; appended for Perl
        constructors.append(named_constructor)

    return constructors, custom_constructors


def clear_constructor_attributes(extended_attributes):
    # Deletes Constructor*s* (plural), sets Constructor (singular)
    if 'Constructors' in extended_attributes:
        del extended_attributes['Constructors']
        extended_attributes['Constructor'] = None
    if 'CustomConstructors' in extended_attributes:
        del extended_attributes['CustomConstructors']
        extended_attributes['CustomConstructor'] = None


################################################################################
# Types
################################################################################

def type_node_to_type(node):
    children = node.GetChildren()
    if len(children) < 1 or len(children) > 2:
        raise ValueError('Type node expects 1 or 2 children (type + optional array []), got %s (multi-dimensional arrays are not supported).' % len(children))

    type_node_child = children[0]

    if len(children) == 2:
        array_node = children[1]
        array_node_class = array_node.GetClass()
        if array_node_class != 'Array':
            raise ValueError('Expected Array node as TypeSuffix, got %s node.' % array_node_class)
        # FIXME: use IdlArrayType instead of is_array, once have that
        is_array = True
    else:
        is_array = False

    is_nullable = node.GetProperty('NULLABLE') or False  # syntax: T?

    return type_node_inner_to_type(type_node_child, is_array=is_array, is_nullable=is_nullable)


def type_node_inner_to_type(node, is_array=False, is_nullable=False):
    # FIXME: remove is_array and is_nullable once have IdlArrayType and IdlNullableType
    node_class = node.GetClass()
    # Note Type*r*ef, not Typedef, meaning the type is an identifier, thus
    # either a typedef shorthand (but not a Typedef declaration itself) or an
    # interface type. We do not distinguish these, and just use the type name.
    if node_class in ['PrimitiveType', 'Typeref']:
        return IdlType(node.GetName(), is_array=is_array, is_nullable=is_nullable)
    elif node_class == 'Any':
        return IdlType('any', is_array=is_array, is_nullable=is_nullable)
    elif node_class == 'Sequence':
        if is_array:
            raise ValueError('Arrays of sequences are not supported')
        return sequence_node_to_type(node, is_nullable=is_nullable)
    elif node_class == 'UnionType':
        if is_array:
            raise ValueError('Arrays of unions are not supported')
        return union_type_node_to_idl_union_type(node, is_nullable=is_nullable)
    raise ValueError('Unrecognized node class: %s' % node_class)


def sequence_node_to_type(node, is_nullable=False):
    children = node.GetChildren()
    if len(children) != 1:
        raise ValueError('Sequence node expects exactly 1 child, got %s' % len(children))
    sequence_child = children[0]
    sequence_child_class = sequence_child.GetClass()
    if sequence_child_class != 'Type':
        raise ValueError('Unrecognized node class: %s' % sequence_child_class)
    element_type = type_node_to_type(sequence_child).base_type
    return IdlType(element_type, is_sequence=True, is_nullable=is_nullable)


def typedef_node_to_type(node):
    children = node.GetChildren()
    if len(children) != 1:
        raise ValueError('Typedef node with %s children, expected 1' % len(children))
    child = children[0]
    child_class = child.GetClass()
    if child_class != 'Type':
        raise ValueError('Unrecognized node class: %s' % child_class)
    return type_node_to_type(child)


def union_type_node_to_idl_union_type(node, is_nullable=False):
    member_types = [type_node_to_type(member_type_node)
                    for member_type_node in node.GetChildren()]
    return IdlUnionType(member_types, is_nullable=is_nullable)
