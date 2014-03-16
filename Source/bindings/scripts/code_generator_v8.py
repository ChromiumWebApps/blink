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

"""Generate Blink V8 bindings (.h and .cpp files).

If run itself, caches Jinja templates (and creates dummy file for build,
since cache filenames are unpredictable and opaque).

This module is *not* concurrency-safe without care: bytecode caching creates
a race condition on cache *write* (crashes if one process tries to read a
partially-written cache). However, if you pre-cache the templates (by running
the module itself), then you can parallelize compiling individual files, since
cache *reading* is safe.

Input: An object of class IdlDefinitions, containing an IDL interface X
Output: V8X.h and V8X.cpp

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import os
import posixpath
import re
import sys

# Path handling for libraries and templates
# Paths have to be normalized because Jinja uses the exact template path to
# determine the hash used in the cache filename, and we need a pre-caching step
# to be concurrency-safe. Use absolute path because __file__ is absolute if
# module is imported, and relative if executed directly.
# If paths differ between pre-caching and individual file compilation, the cache
# is regenerated, which causes a race condition and breaks concurrent build,
# since some compile processes will try to read the partially written cache.
module_path = os.path.dirname(os.path.realpath(__file__))
third_party_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, os.pardir, os.pardir))
templates_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, 'templates'))

# jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
sys.path.insert(1, third_party_dir)
import jinja2

import idl_types
from idl_types import IdlType
import v8_callback_interface
from v8_globals import includes, interfaces
import v8_interface
import v8_types
from v8_utilities import capitalize, cpp_name, conditional_string, v8_class_name


class CodeGeneratorV8(object):
    def __init__(self, interfaces_info, cache_dir):
        interfaces_info = interfaces_info or {}
        self.interfaces_info = interfaces_info
        self.jinja_env = initialize_jinja_env(cache_dir)

        # Set global type info
        idl_types.set_ancestors(dict(
            (interface_name, interface_info['ancestors'])
            for interface_name, interface_info in interfaces_info.iteritems()
            if interface_info['ancestors']))
        IdlType.set_callback_interfaces(set(
            interface_name
            for interface_name, interface_info in interfaces_info.iteritems()
            if interface_info['is_callback_interface']))
        v8_types.set_implemented_as_interfaces(dict(
            (interface_name, interface_info['implemented_as'])
            for interface_name, interface_info in interfaces_info.iteritems()
            if interface_info['implemented_as']))
        v8_types.set_will_be_garbage_collected_types(set(
            interface_name
            for interface_name, interface_info in interfaces_info.iteritems()
            if 'WillBeGarbageCollected' in interface_info['inherited_extended_attributes']))

    def generate_code(self, definitions, interface_name):
        """Returns .h/.cpp code as (header_text, cpp_text)."""
        try:
            interface = definitions.interfaces[interface_name]
        except KeyError:
            raise Exception('%s not in IDL definitions' % interface_name)

        # Store other interfaces for introspection
        interfaces.update(definitions.interfaces)

        # Set local type info
        IdlType.set_callback_functions(definitions.callback_functions.keys())
        IdlType.set_enums((enum.name, enum.values)
                          for enum in definitions.enumerations.values())

        # Select appropriate Jinja template and contents function
        if interface.is_callback:
            header_template_filename = 'callback_interface.h'
            cpp_template_filename = 'callback_interface.cpp'
            generate_contents = v8_callback_interface.generate_callback_interface
        else:
            header_template_filename = 'interface.h'
            cpp_template_filename = 'interface.cpp'
            generate_contents = v8_interface.generate_interface
        header_template = self.jinja_env.get_template(header_template_filename)
        cpp_template = self.jinja_env.get_template(cpp_template_filename)

        # Generate contents (input parameters for Jinja)
        template_contents = generate_contents(interface)

        # Add includes for interface itself and any dependencies
        interface_info = self.interfaces_info[interface_name]
        template_contents['header_includes'].add(interface_info['include_path'])
        template_contents['header_includes'] = sorted(template_contents['header_includes'])
        includes.update(interface_info.get('dependencies_include_paths', []))
        template_contents['cpp_includes'] = sorted(includes)

        # Render Jinja templates
        header_text = header_template.render(template_contents)
        cpp_text = cpp_template.render(template_contents)
        return header_text, cpp_text


def initialize_jinja_env(cache_dir):
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(templates_dir),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({
        'blink_capitalize': capitalize,
        'conditional': conditional_if_endif,
        'runtime_enabled': runtime_enabled_if,
        })
    return jinja_env


# [Conditional]
def conditional_if_endif(code, conditional_string):
    # Jinja2 filter to generate if/endif directive blocks
    if not conditional_string:
        return code
    return ('#if %s\n' % conditional_string +
            code +
            '#endif // %s\n' % conditional_string)


# [RuntimeEnabled]
def runtime_enabled_if(code, runtime_enabled_function_name):
    if not runtime_enabled_function_name:
        return code
    # Indent if statement to level of original code
    indent = re.match(' *', code).group(0)
    return ('%sif (%s())\n' % (indent, runtime_enabled_function_name) +
            '    %s' % code)


################################################################################

def main(argv):
    # If file itself executed, cache templates
    try:
        cache_dir = argv[1]
        dummy_filename = argv[2]
    except IndexError as err:
        print 'Usage: %s OUTPUT_DIR DUMMY_FILENAME' % argv[0]
        return 1

    # Cache templates
    jinja_env = initialize_jinja_env(cache_dir)
    template_filenames = [filename for filename in os.listdir(templates_dir)
                          # Skip .svn, directories, etc.
                          if filename.endswith(('.cpp', '.h'))]
    for template_filename in template_filenames:
        jinja_env.get_template(template_filename)

    # Create a dummy file as output for the build system,
    # since filenames of individual cache files are unpredictable and opaque
    # (they are hashes of the template path, which varies based on environment)
    with open(dummy_filename, 'w') as dummy_file:
        pass  # |open| creates or touches the file


if __name__ == '__main__':
    sys.exit(main(sys.argv))
