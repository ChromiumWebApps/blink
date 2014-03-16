# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions (file reading, simple IDL parsing by regexes) for IDL build.

Design doc: http://www.chromium.org/developers/design-documents/idl-build
"""

import os
import cPickle as pickle
import re
import string


class IdlBadFilenameError(Exception):
    """Raised if an IDL filename disagrees with the interface name in the file."""
    pass


################################################################################
# Basic file reading/writing
################################################################################

def get_file_contents(filename):
    with open(filename) as f:
        return f.read()


def write_file(new_text, destination_filename, only_if_changed):
    if only_if_changed and os.path.isfile(destination_filename):
        with open(destination_filename) as destination_file:
            if destination_file.read() == new_text:
                return
    with open(destination_filename, 'w') as destination_file:
        destination_file.write(new_text)


def write_pickle_file(pickle_filename, data, only_if_changed):
    if only_if_changed and os.path.isfile(pickle_filename):
        with open(pickle_filename) as pickle_file:
            if pickle.load(pickle_file) == data:
                return
    with open(pickle_filename, 'w') as pickle_file:
        pickle.dump(data, pickle_file)


################################################################################
# IDL parsing
#
# We use regular expressions for parsing; this is incorrect (Web IDL is not a
# regular language), but simple and sufficient in practice.
# Leading and trailing context (e.g. following '{') used to avoid false matches.
################################################################################

def get_partial_interface_name_from_idl(file_contents):
    match = re.search(r'partial\s+interface\s+(\w+)\s*{', file_contents)
    return match and match.group(1)


def get_implemented_interfaces_from_idl(file_contents, interface_name):
    # Rule is: identifier-A implements identifier-B;
    # http://www.w3.org/TR/WebIDL/#idl-implements-statements
    def get_implemented(left_identifier, right_identifier):
        # identifier-A must be the current interface
        if left_identifier != interface_name:
            raise IdlBadFilenameError("Identifier on the left of the 'implements' statement should be %s in %s.idl, but found %s" % (interface_name, interface_name, left_identifier))
        return right_identifier

    implements_re = (r'^\s*'
                     r'(\w+)\s+'
                     r'implements\s+'
                     r'(\w+)\s*'
                     r';')
    implements_matches = re.finditer(implements_re, file_contents, re.MULTILINE)
    implements_pairs = [(match.group(1), match.group(2))
                        for match in implements_matches]
    return [get_implemented(left, right) for left, right in implements_pairs]


def is_callback_interface_from_idl(file_contents):
    match = re.search(r'callback\s+interface\s+\w+\s*{', file_contents)
    return bool(match)


def get_parent_interface(file_contents):
    match = re.search(r'interface\s+'
                      r'\w+\s*'
                      r':\s*(\w+)\s*'
                      r'{',
                      file_contents)
    return match and match.group(1)


def get_interface_extended_attributes_from_idl(file_contents):
    match = re.search(r'\[(.*)\]\s*'
                      r'((callback|partial)\s+)?'
                      r'(interface|exception)\s+'
                      r'\w+\s*'
                      r'(:\s*\w+\s*)?'
                      r'{',
                      file_contents, flags=re.DOTALL)
    if not match:
        return {}
    # Strip comments
    # re.compile needed b/c Python 2.6 doesn't support flags in re.sub
    single_line_comment_re = re.compile(r'//.*$', flags=re.MULTILINE)
    block_comment_re = re.compile(r'/\*.*?\*/', flags=re.MULTILINE | re.DOTALL)
    extended_attributes_string = re.sub(single_line_comment_re, '', match.group(1))
    extended_attributes_string = re.sub(block_comment_re, '', extended_attributes_string)
    extended_attributes = {}
    # FIXME: this splitting is WRONG: it fails on ExtendedAttributeArgList like
    # 'NamedConstructor=Foo(a, b)'
    parts = [extended_attribute.strip()
             for extended_attribute in extended_attributes_string.split(',')
             # Discard empty parts, which may exist due to trailing comma
             if extended_attribute.strip()]
    for part in parts:
        name, _, value = map(string.strip, part.partition('='))
        extended_attributes[name] = value
    return extended_attributes


def get_put_forward_interfaces_from_idl(file_contents):
    put_forwards_pattern = (r'\[[^\]]*PutForwards=[^\]]*\]\s+'
                            r'readonly\s+'
                            r'attribute\s+'
                            r'(\w+)')
    return sorted(set(match.group(1)
                      for match in re.finditer(put_forwards_pattern,
                                               file_contents,
                                               flags=re.DOTALL)))
