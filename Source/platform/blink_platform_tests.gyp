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
#
{
  'includes': [
    '../build/win/precompile.gypi',
    'blink_platform.gypi',
  ],
  'targets': [{
    'target_name': 'blink_platform_unittests',
    'type': 'executable',
    'dependencies': [
      '../config.gyp:unittest_config',
      '../wtf/wtf.gyp:wtf',
      '../wtf/wtf_tests.gyp:run_all_tests',
      '../wtf/wtf_tests.gyp:wtf_unittest_helpers',
      'blink_platform.gyp:blink_platform',
      'blink_platform.gyp:blink_common',
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
    'defines': [
      'INSIDE_BLINK',
    ],
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)/blink',
    ],
    'sources': [
      '<@(platform_test_files)',
    ],
    'conditions': [
      # TODO(dmikurube): Kill linux_use_tcmalloc. http://crbug.com/345554
      ['os_posix==1 and OS!="mac" and OS!="android" and OS!="ios" and ((use_allocator!="none" and use_allocator!="see_use_tcmalloc") or (use_allocator=="see_use_tcmalloc" and linux_use_tcmalloc==1))', {
        'dependencies': [
          '<(DEPTH)/base/base.gyp:base',
          '<(DEPTH)/base/allocator/allocator.gyp:allocator',
        ]
      }],
      ['OS=="android" and gtest_target_type == "shared_library"', {
        'type': 'shared_library',
        'dependencies': [
          '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          '<(DEPTH)/tools/android/forwarder2/forwarder.gyp:forwarder2',
        ],
      }],
    ],
  }],
  'conditions': [
    ['OS=="android" and android_webview_build==0 and gtest_target_type == "shared_library"', {
      'targets': [{
        'target_name': 'blink_platform_unittests_apk',
        'type': 'none',
        'dependencies': [
          '<(DEPTH)/base/base.gyp:base_java',
          '<(DEPTH)/net/net.gyp:net_java',
          'blink_platform_unittests',
        ],
        'variables': {
          'test_suite_name': 'blink_platform_unittests',
        },
        'includes': [ '../../../../build/apk_test.gypi' ],
      }],
    }],
  ],
}
