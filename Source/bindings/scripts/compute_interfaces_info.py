#!/usr/bin/python
#
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

"""Compute global interface information, including public information, dependencies, and inheritance.

Computed data is stored in a global variable, |interfaces_info|, and written as
output (concretely, exported as a pickle). This is then used by the IDL compiler
itself, so it does not need to compute global information itself, and so that
inter-IDL dependencies are clear, since they are all computed here.

The |interfaces_info| pickle is a *global* dependency: any changes cause a full
rebuild. This is to avoid having to compute which public data is visible by
which IDL files on a file-by-file basis, which is very complex for little
benefit.
|interfaces_info| should thus only contain data about an interface that
contains paths or is needed by *other* interfaces, e.g., path data (to abstract
the compiler from OS-specific file paths) or public data (to avoid having to
read other interfaces unnecessarily).
It should *not* contain full information about an interface (e.g., all
extended attributes), as this would cause unnecessary rebuilds.

|interfaces_info| is a dict, keyed by |interface_name|.

Current keys are:
* dependencies:
    'implements_interfaces': targets of 'implements' statements
    'referenced_interfaces': reference interfaces that are introspected
                             (currently just targets of [PutForwards])

* inheritance:
    'ancestors': all ancestor interfaces
    'inherited_extended_attributes': inherited extended attributes
                                     (all controlling memory management)

* public:
    'is_callback_interface': bool, callback interface or not
    'implemented_as': value of [ImplementedAs=...] on interface (C++ class name)

* paths:
    'full_path': path to the IDL file, so can lookup an IDL by interface name
    'include_path': path for use in C++ #include directives
    'dependencies_full_paths': paths to dependencies (for merging into main)
    'dependencies_include_paths': paths for use in C++ #include directives

Note that all of these are stable information, unlikely to change without
moving or deleting files (hence requiring a full rebuild anyway) or significant
code changes (for inherited extended attributes).

Design doc: http://www.chromium.org/developers/design-documents/idl-build
"""

from collections import defaultdict
import optparse
import os
import posixpath
import sys

from utilities import get_file_contents, write_pickle_file, get_interface_extended_attributes_from_idl, is_callback_interface_from_idl, get_partial_interface_name_from_idl, get_implemented_interfaces_from_idl, get_parent_interface, get_put_forward_interfaces_from_idl

module_path = os.path.dirname(__file__)
source_path = os.path.normpath(os.path.join(module_path, os.pardir, os.pardir))

INHERITED_EXTENDED_ATTRIBUTES = set([
    'ActiveDOMObject',
    'DependentLifetime',
    'WillBeGarbageCollected',
])

# Main variable (filled in and exported)
interfaces_info = {}

# Auxiliary variables (not visible to future build steps)
partial_interface_files = defaultdict(lambda: {
    'full_paths': [],
    'include_paths': [],
})
parent_interfaces = {}
inherited_extended_attributes_by_interface = {}  # interface name -> extended attributes


class IdlInterfaceFileNotFoundError(Exception):
    """Raised if the IDL file implementing an interface cannot be found."""
    pass


def parse_options():
    usage = 'Usage: %prog [options] [generated1.idl]...'
    parser = optparse.OptionParser(usage=usage)
    parser.add_option('--idl-files-list', help='file listing IDL files')
    parser.add_option('--interfaces-info-file', help='output pickle file')
    parser.add_option('--write-file-only-if-changed', type='int', help='if true, do not write an output file if it would be identical to the existing one, which avoids unnecessary rebuilds in ninja')

    options, args = parser.parse_args()
    if options.interfaces_info_file is None:
        parser.error('Must specify an output file using --interfaces-info-file.')
    if options.idl_files_list is None:
        parser.error('Must specify a file listing IDL files using --idl-files-list.')
    if options.write_file_only_if_changed is None:
        parser.error('Must specify whether file is only written if changed using --write-file-only-if-changed.')
    options.write_file_only_if_changed = bool(options.write_file_only_if_changed)
    return options, args


################################################################################
# Computations
################################################################################

def include_path(idl_filename, implemented_as=None):
    """Returns relative path to header file in POSIX format; used in includes.

    POSIX format is used for consistency of output, so reference tests are
    platform-independent.
    """
    relative_path_local = os.path.relpath(idl_filename, source_path)
    relative_dir_local = os.path.dirname(relative_path_local)
    relative_dir_posix = relative_dir_local.replace(os.path.sep, posixpath.sep)

    idl_file_basename, _ = os.path.splitext(os.path.basename(idl_filename))
    cpp_class_name = implemented_as or idl_file_basename

    return posixpath.join(relative_dir_posix, cpp_class_name + '.h')


def add_paths_to_partials_dict(partial_interface_name, full_path, this_include_path=None):
    paths_dict = partial_interface_files[partial_interface_name]
    paths_dict['full_paths'].append(full_path)
    if this_include_path:
        paths_dict['include_paths'].append(this_include_path)


def compute_individual_info(idl_filename):
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)

    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)
    implemented_as = extended_attributes.get('ImplementedAs')
    # FIXME: remove [NoHeader] once switch to Python
    this_include_path = (include_path(idl_filename, implemented_as)
                         if 'NoHeader' not in extended_attributes else None)

    # Handle partial interfaces
    partial_interface_name = get_partial_interface_name_from_idl(idl_file_contents)
    if partial_interface_name:
        add_paths_to_partials_dict(partial_interface_name, full_path, this_include_path)
        return

    # If not a partial interface, the basename is the interface name
    interface_name, _ = os.path.splitext(os.path.basename(idl_filename))

    interfaces_info[interface_name] = {
        'full_path': full_path,
        'implemented_as': implemented_as,
        'implements_interfaces': get_implemented_interfaces_from_idl(idl_file_contents, interface_name),
        'include_path': this_include_path,
        'is_callback_interface': is_callback_interface_from_idl(idl_file_contents),
        # Interfaces that are referenced (used as types) and that we introspect
        # during code generation (beyond interface-level data ([ImplementedAs],
        # is_callback_interface, ancestors, and inherited extended attributes):
        # deep dependencies.
        # These cause rebuilds of referrers, due to the dependency, so these
        # should be minimized; currently only targets of [PutForwards].
        'referenced_interfaces': get_put_forward_interfaces_from_idl(idl_file_contents),
    }

    # Record inheritance information
    inherited_extended_attributes_by_interface[interface_name] = dict(
            (key, value)
            for key, value in extended_attributes.iteritems()
            if key in INHERITED_EXTENDED_ATTRIBUTES)
    parent = get_parent_interface(idl_file_contents)
    if parent:
        parent_interfaces[interface_name] = parent


def compute_inheritance_info(interface_name):
    """Compute inheritance information, namely ancestors and inherited extended attributes."""
    def generate_ancestors(interface_name):
        while interface_name in parent_interfaces:
            interface_name = parent_interfaces[interface_name]
            yield interface_name

    ancestors = list(generate_ancestors(interface_name))
    inherited_extended_attributes = inherited_extended_attributes_by_interface[interface_name]
    for ancestor in ancestors:
        # Ancestors may not be present, notably if an ancestor is a generated
        # IDL file and we are running this script from run-bindings-tests,
        # where we don't generate these files.
        ancestor_extended_attributes = inherited_extended_attributes_by_interface.get(ancestor, {})
        inherited_extended_attributes.update(ancestor_extended_attributes)

    interfaces_info[interface_name].update({
        'ancestors': ancestors,
        'inherited_extended_attributes': inherited_extended_attributes,
    })


def compute_interfaces_info(idl_files):
    """Compute information about IDL files.

    Information is stored in global interfaces_info.
    """
    # Compute information for individual files
    for idl_filename in idl_files:
        compute_individual_info(idl_filename)

    # Once all individual files handled, can compute inheritance information
    # and dependencies

    # Compute inheritance info
    for interface_name in interfaces_info:
        compute_inheritance_info(interface_name)

    # Compute dependencies
    # An IDL file's dependencies are partial interface files that extend it,
    # and files for other interfaces that this interfaces implements.
    for interface_name, interface_info in interfaces_info.iteritems():
        partial_interface_paths = partial_interface_files[interface_name]
        partial_interfaces_full_paths = partial_interface_paths['full_paths']
        partial_interfaces_include_paths = partial_interface_paths['include_paths']

        implemented_interfaces = interface_info['implements_interfaces']
        try:
            implemented_interfaces_info = [
                interfaces_info[interface]
                for interface in implemented_interfaces]
        except KeyError as key_name:
            raise IdlInterfaceFileNotFoundError('Could not find the IDL file where the following implemented interface is defined: %s' % key_name)
        implemented_interfaces_full_paths = [
            implemented_interface_info['full_path']
            for implemented_interface_info in implemented_interfaces_info]
        implemented_interfaces_include_paths = [
            implemented_interface_info['include_path']
            for implemented_interface_info in implemented_interfaces_info
            if implemented_interface_info['include_path']]

        interface_info.update({
            'dependencies_full_paths': (partial_interfaces_full_paths +
                                        implemented_interfaces_full_paths),
            'dependencies_include_paths': (partial_interfaces_include_paths +
                                           implemented_interfaces_include_paths),
        })


################################################################################

def main():
    options, args = parse_options()

    # Static IDL files are passed in a file (generated at GYP time), due to OS
    # command line length limits
    with open(options.idl_files_list) as idl_files_list:
        idl_files = [line.rstrip('\n') for line in idl_files_list]
    # Generated IDL files are passed at the command line, since these are in the
    # build directory, which is determined at build time, not GYP time, so these
    # cannot be included in the file listing static files
    idl_files.extend(args)

    compute_interfaces_info(idl_files)
    write_pickle_file(options.interfaces_info_file,
                      interfaces_info,
                      options.write_file_only_if_changed)


if __name__ == '__main__':
    sys.exit(main())
