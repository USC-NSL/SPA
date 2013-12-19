#
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
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
    'variables': {
        'ahem_path': '../../DumpRenderTree/qt/fonts/AHEM____.TTF',
        'tools_dir': '../..',
        'source_dir': '../../../Source',
        'conditions': [
            ['OS=="linux"', {
                'use_custom_freetype%': 1,
            }, {
                'use_custom_freetype%': 0,
            }],
        ],
    },
    'includes': [
        '../DumpRenderTree.gypi',
        '../../../Source/core/features.gypi',
    ],
    'targets': [
        {
            'target_name': 'ImageDiff',
            'type': 'executable',
            'dependencies': [
                '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_support_gfx',
            ],
            'include_dirs': [
                '<(DEPTH)',
            ],
            'sources': [
                '<(tools_dir)/DumpRenderTree/chromium/ImageDiff.cpp',
            ],
            'conditions': [
                ['OS=="android" and android_webview_build==0', {
                    # The Chromium Android port will compare images on host rather
                    # than target (a device or emulator) for performance reasons.
                    'toolsets': ['host'],
                }],
                ['OS=="android" and android_webview_build==1', {
                    'type': 'none',
                }],
            ],
        },
        {
            'target_name': 'TestRunner',
            'type': '<(component)',
            'defines': [
                'WEBTESTRUNNER_IMPLEMENTATION=1',
            ],
            'dependencies': [
                'TestRunner_resources',
                '<(source_dir)/WebKit/chromium/WebKit.gyp:webkit',
                '<(source_dir)/WebKit/chromium/WebKit.gyp:webkit_test_support',
            ],
            'include_dirs': [
                '<(DEPTH)',
                '<(source_dir)/WebKit/chromium/public',
                '<(DEPTH)',
                '../chromium/TestRunner/public',
                '../chromium/TestRunner/src',
                '../../../Source',
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    '../chromium/TestRunner/public',
                    '../../../Source',
                ],
            },
            'sources': [
                '<@(test_runner_files)',
            ],
            'conditions': [
                ['component=="shared_library"', {
                    'defines': [
                        'WEBTESTRUNNER_DLL',
                        'WEBTESTRUNNER_IMPLEMENTATION=1',
                    ],
                    'dependencies': [
                        '<(DEPTH)/base/base.gyp:base',
                        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
                        '<(DEPTH)/skia/skia.gyp:skia',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                    ],
                    'direct_dependent_settings': {
                        'defines': [
                            'WEBTESTRUNNER_DLL',
                        ],
                    },
                    'export_dependent_settings': [
                        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                    ],
                    'msvs_settings': {
                        'VCLinkerTool': {
                            'conditions': [
                                ['incremental_chrome_dll==1', {
                                    'UseLibraryDependencyInputs': 'true',
                                }],
                            ],
                        },
                    },
                }],
                ['toolkit_uses_gtk == 1', {
                    'defines': [
                        'WTF_USE_GTK=1',
                    ],
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:gtk',
                    ],
                    'include_dirs': [
                        '<(source_dir)/WebKit/chromium/public/gtk',
                    ],
                }],
                ['OS!="win"', {
                    'sources/': [
                        ['exclude', 'Win\\.cpp$'],
                    ],
                }],
            ],
            # Disable c4267 warnings until we fix size_t to int truncations. 
            'msvs_disabled_warnings': [ 4267, ],
        },
        {
            'target_name': 'TestRunner_resources',
            'type': 'none',
            'dependencies': [
                'ImageDiff',
                'copy_TestNetscapePlugIn',
            ],
            'conditions': [
                ['OS=="win"', {
                    'dependencies': [
                        'LayoutTestHelper',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': ['<(ahem_path)'],
                    }],
                }],
                ['OS=="mac"', {
                    'dependencies': [
                        'LayoutTestHelper',
                    ],
                    'all_dependent_settings': {
                        'mac_bundle_resources': [
                            '<(ahem_path)',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher100.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher200.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher300.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher400.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher500.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher600.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher700.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher800.ttf',
                            '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher900.ttf',
                            '<(SHARED_INTERMEDIATE_DIR)/webkit/missingImage.png',
                            '<(SHARED_INTERMEDIATE_DIR)/webkit/textAreaResizeCorner.png',
                        ],
                    },
                }],
                # The test plugin relies on X11.
                ['OS=="linux" and use_x11==0', {
                    'dependencies!': [
                        'copy_TestNetscapePlugIn',
                    ],
                }],
                ['use_x11 == 1', {
                    'dependencies': [
                        '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': [
                            '<(ahem_path)',
                            '<(tools_dir)/DumpRenderTree/chromium/fonts.conf',
                        ]
                    }],
                }],
                ['OS=="android"', {
                    'dependencies!': [
                        'ImageDiff',
                        'copy_TestNetscapePlugIn',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': [
                            '<(ahem_path)',
                            '<(tools_dir)/DumpRenderTree/chromium/android_main_fonts.xml',
                            '<(tools_dir)/DumpRenderTree/chromium/android_fallback_fonts.xml',
                        ]
                    }],
                }],
                ['OS=="android" and android_webview_build==0', {
                    'dependencies': [
                        'ImageDiff#host',
                    ],
                }],
            ],
        },
        {
            'target_name': 'DumpRenderTree',
            'type': 'executable',
            'mac_bundle': 1,
            'dependencies': [
                'TestRunner',
                'DumpRenderTree_resources',
                '<(source_dir)/devtools/devtools.gyp:devtools_frontend_resources',
                '<(source_dir)/WebKit/chromium/WebKit.gyp:webkit',
                '<(source_dir)/WebKit/chromium/WebKit.gyp:webkit_wtf_support',
                '<(source_dir)/wtf/wtf.gyp:wtf',
                '<(DEPTH)/base/base.gyp:test_support_base',
                '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/third_party/mesa/mesa.gyp:osmesa',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_support',
            ],
            'include_dirs': [
                '<(DEPTH)',
                '<(source_dir)/WebKit/chromium/public',
                '<(tools_dir)/DumpRenderTree',
                '<(DEPTH)',
            ],
            'defines': [
                # Technically not a unit test but require functions available only to
                # unit tests.
                'UNIT_TEST',
            ],
            'sources': [
                '<@(drt_files)',
            ],
            'conditions': [
                ['OS=="mac" or OS=="win" or toolkit_uses_gtk==1', {
                    # These platforms have their own implementations of
                    # checkLayoutTestSystemDependencies() and openStartupDialog().
                    'sources/': [
                        ['exclude', 'TestShellStub\\.cpp$'],
                    ],
                }],
                ['OS=="win"', {
                    'dependencies': [
                        '<(DEPTH)/third_party/angle/src/build_angle.gyp:libEGL',
                        '<(DEPTH)/third_party/angle/src/build_angle.gyp:libGLESv2',
                    ],
                    'resource_include_dirs': ['<(SHARED_INTERMEDIATE_DIR)/webkit'],
                    'sources': [
                        # FIXME: We should just use the resources in the .pak file.
                        '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
                    ],
                    'configurations': {
                        'Debug_Base': {
                            'msvs_settings': {
                                'VCLinkerTool': {
                                    'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                                },
                            },
                        },
                    },
                },{ # OS!="win"
                    'sources/': [
                        ['exclude', 'Win\\.cpp$'],
                    ],
                }],
                ['OS=="mac"', {
                    'dependencies': [
                        '<(source_dir)/WebKit/chromium/WebKit.gyp:copy_mesa',
                    ],
                },{ # OS!="mac"
                    'sources/': [
                        # .mm is already excluded by common.gypi
                        ['exclude', 'Mac\\.cpp$'],
                    ],
                }],
                ['os_posix!=1 or OS=="mac"', {
                    'sources/': [
                        ['exclude', 'Posix\\.cpp$'],
                    ],
                }],
                ['use_x11 == 1', {
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:fontconfig',
                    ],
                    'variables': {
                        # FIXME: Enable warnings on other platforms.
                        'chromium_code': 1,
                    },
                    'conditions': [
                        ['linux_use_tcmalloc == 1', {
                            'dependencies': [
                                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
                            ],
                        }],
                    ],
                },{ # use_x11 != 1
                    'sources/': [
                        ['exclude', 'X11\\.cpp$'],
                    ]
                }],
                ['toolkit_uses_gtk == 1', {
                    'defines': [
                        'WTF_USE_GTK=1',
                    ],
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:gtk',
                    ],
                    'include_dirs': [
                        '<(source_dir)/WebKit/chromium/public/gtk',
                    ],
                }],
                ['OS=="android"', {
                    'type': 'shared_library',
                    'dependencies': [
                        '<(DEPTH)/base/base.gyp:test_support_base',
                        '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
                        '<(DEPTH)/tools/android/forwarder/forwarder.gyp:forwarder',
                        '<(DEPTH)/tools/android/md5sum/md5sum.gyp:md5sum',
                    ],
                }, { # OS!="android"
                    'sources/': [
                        ['exclude', 'Android\\.cpp$'],
                    ],
                }],
                ['use_custom_freetype==1', {
                   'dependencies': [
                       '<(DEPTH)/third_party/freetype2/freetype2.gyp:freetype2',
                   ],
                }],
            ],
        },
        {
            'target_name': 'DumpRenderTree_resources',
            'type': 'none',
            'dependencies': [
                '<(DEPTH)/net/net.gyp:net_resources',
                '<(DEPTH)/ui/ui.gyp:ui_resources',
                '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
                '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
            ],
            'actions': [{
                'action_name': 'repack_local',
                'variables': {
                    'repack_path': '<(DEPTH)/tools/grit/grit/format/repack.py',
                    'pak_inputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources_100_percent.pak',
                ]},
                'inputs': [
                    '<(repack_path)',
                    '<@(pak_inputs)',
                ],
                'outputs': [
                    '<(PRODUCT_DIR)/DumpRenderTree.pak',
                ],
                'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
            }],
            'conditions': [
                ['OS=="mac"', {
                    'all_dependent_settings': {
                        'mac_bundle_resources': [
                            '<(PRODUCT_DIR)/DumpRenderTree.pak',
                        ],
                    },
                }],
            ]
        },
        {
            'target_name': 'TestNetscapePlugIn',
            'type': 'loadable_module',
            'sources': [ '<@(test_plugin_files)' ],
            'dependencies': [
                '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
            ],
            'include_dirs': [
                '<(DEPTH)',
                '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn',
                '<(tools_dir)/DumpRenderTree/chromium/TestNetscapePlugIn/ForwardingHeaders',
            ],
            'conditions': [
                ['OS=="mac"', {
                    'mac_bundle': 1,
                    'product_extension': 'plugin',
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
                            '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
                            '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
                        ]
                    },
                    'xcode_settings': {
                        'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
                        'INFOPLIST_FILE': '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn/mac/Info.plist',
                    },
                }],
                ['os_posix == 1 and OS != "mac"', {
                    'cflags': [
                        '-fvisibility=default',
                    ],
                }],
                ['OS=="win"', {
                    'defines': [
                        # This seems like a hack, but this is what Safari Win does.
                        'snprintf=_snprintf',
                    ],
                    'sources': [
                        '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn/win/TestNetscapePlugin.def',
                        '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn/win/TestNetscapePlugin.rc',
                    ],
                    # The .rc file requires that the name of the dll is npTestNetscapePlugIn.dll.
                    'product_name': 'npTestNetscapePlugIn',
                    # Disable c4267 warnings until we fix size_t to int truncations. 
                    'msvs_disabled_warnings': [ 4267, ],
                }],
            ],
        },
        {
            'target_name': 'copy_TestNetscapePlugIn',
            'type': 'none',
            'dependencies': [
                'TestNetscapePlugIn',
            ],
            'conditions': [
                ['OS=="win"', {
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins',
                        'files': ['<(PRODUCT_DIR)/npTestNetscapePlugIn.dll'],
                    }],
                }],
                ['OS=="mac"', {
                    'dependencies': ['TestNetscapePlugIn'],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins/',
                        'files': ['<(PRODUCT_DIR)/TestNetscapePlugIn.plugin/'],
                    }],
                }],
                ['os_posix == 1 and OS != "mac"', {
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins',
                        'files': ['<(PRODUCT_DIR)/libTestNetscapePlugIn.so'],
                    }],
                }],
            ],
        },
    ], # targets
    'conditions': [
        ['OS=="win"', {
            'targets': [{
                'target_name': 'LayoutTestHelper',
                'type': 'executable',
                'sources': ['<(tools_dir)/DumpRenderTree/chromium/LayoutTestHelperWin.cpp'],
            }],
        }],
        ['OS=="mac"', {
            'targets': [{
                'target_name': 'LayoutTestHelper',
                'type': 'executable',
                'sources': ['<(tools_dir)/DumpRenderTree/chromium/LayoutTestHelper.mm'],
                'link_settings': {
                    'libraries': [
                        '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
                    ],
                },
            }],
        }],
        ['gcc_version>=46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
        ['OS=="android"', {
            # Wrap libDumpRenderTree.so into an android apk for execution.
            'targets': [{
                'target_name': 'DumpRenderTree_apk',
                'type': 'none',
                'dependencies': [
                    '<(DEPTH)/base/base.gyp:base_java',
                    '<(DEPTH)/media/media.gyp:media_java',
                    '<(DEPTH)/net/net.gyp:net_java',
                    'DumpRenderTree',
                ],
                'variables': {
                    'test_suite_name': 'DumpRenderTree',
                    'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)DumpRenderTree<(SHARED_LIB_SUFFIX)',
                },
                'includes': [ '../../../../../build/apk_test.gypi' ],
            }],
        }],
        ['clang==1', {
            'target_defaults': {
                # FIXME: Add -Wglobal-constructors after fixing existing bugs.
                'cflags': ['-Wunused-parameter'],
                'xcode_settings': {
                    'WARNING_CFLAGS': ['-Wunused-parameter'],
                },
            },
        }],
    ], # conditions
}
