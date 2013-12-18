# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../ppapi/ppapi.gypi',
  ],
  'variables': {
    'chromium_code': 1,
    'conditions': [
      # Pepper demos that are compiled as shared libraries need to be compiled
      # with -fPIC flag. All static libraries that these demos depend on must
      # also be compiled with -fPIC flag. Setting GYP_DEFINES="linux_fpic=1"
      # compiles everything with -fPIC. Disable pepper demos on linux/x64
      # unless linux_fpic is 1.
      ['os_posix == 1 and OS != "mac" and (target_arch=="x64" or target_arch=="arm") and linux_fpic!=1', {
        'enable_pepper_demos%': 0,
      }, {
        'enable_pepper_demos%': 1,
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'gpu_demo_framework',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'framework/demo.cc',
        'framework/demo.h',
        'framework/demo_factory.h',
      ],
    },
    {
      'target_name': 'gpu_demo_framework_exe',
      'type': 'static_library',
      'dependencies': [
        'gpu_demo_framework',
        '../gpu.gyp:command_buffer_client',
        '../gpu.gyp:command_buffer_service',
        '../gpu.gyp:gles2_implementation',
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../ui/gl/gl.gyp:gl',
        '../../ui/ui.gyp:ui',
      ],
      'sources': [
        'framework/main_exe.cc',
        'framework/window.cc',
        'framework/window_linux.cc',
        'framework/window_mac.mm',
        'framework/window_win.cc',
        'framework/window.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
      ],
    },
    {
      'target_name': 'gpu_demo_framework_ppapi',
      'suppress_wildcard': 1,  # So that 'all' doesn't end up being a bundle.
      'type': 'static_library',
      'dependencies': [
        'gpu_demo_framework',
        '../../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../../ppapi/ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        '../..',
        '../../ppapi/lib/gl/include',
        '../../third_party/gles2_book/Common/Include',
      ],
      'sources': [
        'framework/pepper.cc',
        '../../third_party/gles2_book/Common/Include/esUtil.h',
        '../../third_party/gles2_book/Common/Source/esShader.c',
        '../../third_party/gles2_book/Common/Source/esShapes.c',
        '../../third_party/gles2_book/Common/Source/esTransform.c',
        '../../third_party/gles2_book/Common/Source/esUtil.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../..',
          '../../ppapi/lib/gl/include',
          '../../third_party/gles2_book/Common/Include',
        ],
        'run_as': {
          'conditions': [
            ['OS=="mac"', {
              'action': [
                '<(PRODUCT_DIR)/Chromium.app/Contents/MacOS/Chromium',
                '--register-pepper-plugins='
                  '<(PRODUCT_DIR)/$(PRODUCT_NAME).plugin;'
                  'pepper-application/x-gpu-demo',
                'file://$(SOURCE_ROOT)/pepper_gpu_demo.html',
              ],
             }, { # OS != "mac"
              'action': [
                '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
                '--register-pepper-plugins=$(TargetPath);'
                  'pepper-application/x-gpu-demo',
                'file://$(ProjectDir)pepper_gpu_demo.html',
              ],
            }],
          ],
        },
        'conditions': [
          ['os_posix == 1 and OS != "mac"', {
            # -gstabs, used in the official builds, causes an ICE. Remove it.
            'cflags!': ['-gstabs'],
          }],
          ['OS=="mac"', {
            'mac_bundle': 1,
            'product_extension': 'plugin',
          }],
        ],
      },
    },
    {
      'target_name': 'hello_triangle_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:hello_triangle',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/demo_hello_triangle.cc',
      ],
    },
    {
      'target_name': 'mip_map_2d_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:mip_map_2d',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/demo_mip_map_2d.cc',
      ],
    },
    {
      'target_name': 'simple_texture_2d_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:simple_texture_2d',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/demo_simple_texture_2d.cc',
      ],
    },
    {
      'target_name': 'simple_texture_cubemap_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:simple_texture_cubemap',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/demo_simple_texture_cubemap.cc',
      ],
    },
    {
      'target_name': 'simple_vertex_shader_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:simple_vertex_shader',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/demo_simple_vertex_shader.cc',
      ],
    },
    {
      'target_name': 'stencil_test_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:stencil_test',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/demo_stencil_test.cc',
      ],
    },
    {
      'target_name': 'texture_wrap_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:texture_wrap',
      ],
      'sources': [
        'gles2_book/example.h',
        'gles2_book/demo_texture_wrap.cc',
      ],
    },
    {
      'target_name': 'compressed_textures_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:es_util',
      ],
      'sources': [
        'compressed_textures/compressed_textures.cc',
      ],
    },
    {
      'target_name': 'occlusion_query_exe',
      'type': 'executable',
      'dependencies': [
        'gpu_demo_framework_exe',
        '../../third_party/gles2_book/gles2_book.gyp:es_util',
      ],
      'defines': [
        'GL_GLEXT_PROTOTYPES',
      ],
      'sources': [
        'occlusion_query/occlusion_query.cc',
      ],
    },
  ],
  'conditions': [
    ['enable_pepper_demos==1', {
      'targets': [
        {
          'target_name': 'hello_triangle_ppapi',
          'type': 'loadable_module',
          'variables': { 'chromium_code': 0, },
          'dependencies': [ 'gpu_demo_framework_ppapi', ],
          'sources': [
            '<@(ppp_entrypoints_sources)',
            'gles2_book/example.h',
            'gles2_book/demo_hello_triangle.cc',
            '../../third_party/gles2_book/Chapter_2/Hello_Triangle/Hello_Triangle.c',
            '../../third_party/gles2_book/Chapter_2/Hello_Triangle/Hello_Triangle.h',
          ],
        },
        {
          'target_name': 'mip_map_2d_ppapi',
          'type': 'loadable_module',
          'variables': { 'chromium_code': 0, },
          'dependencies': [ 'gpu_demo_framework_ppapi', ],
          'sources': [
            '<@(ppp_entrypoints_sources)',
            'gles2_book/example.h',
            'gles2_book/demo_mip_map_2d.cc',
            '../../third_party/gles2_book/Chapter_9/MipMap2D/MipMap2D.c',
            '../../third_party/gles2_book/Chapter_9/MipMap2D/MipMap2D.h',
          ],
        },
        {
          'target_name': 'simple_texture_2d_ppapi',
          'type': 'loadable_module',
          'variables': { 'chromium_code': 0, },
          'dependencies': [ 'gpu_demo_framework_ppapi', ],
          'sources': [
            '<@(ppp_entrypoints_sources)',
            'gles2_book/example.h',
            'gles2_book/demo_simple_texture_2d.cc',
            '../../third_party/gles2_book/Chapter_9/Simple_Texture2D/Simple_Texture2D.c',
            '../../third_party/gles2_book/Chapter_9/Simple_Texture2D/Simple_Texture2D.h',
          ],
        },
        {
          'target_name': 'simple_texture_cubemap_ppapi',
          'type': 'loadable_module',
          'variables': { 'chromium_code': 0, },
          'dependencies': [ 'gpu_demo_framework_ppapi', ],
          'sources': [
            '<@(ppp_entrypoints_sources)',
            'gles2_book/example.h',
            'gles2_book/demo_simple_texture_cubemap.cc',
            '../../third_party/gles2_book/Chapter_9/Simple_TextureCubemap/Simple_TextureCubemap.c',
            '../../third_party/gles2_book/Chapter_9/Simple_TextureCubemap/Simple_TextureCubemap.h',
          ],
        },
        {
          'target_name': 'simple_vertex_shader_ppapi',
          'type': 'loadable_module',
          'variables': { 'chromium_code': 0, },
          'dependencies': [ 'gpu_demo_framework_ppapi', ],
          'sources': [
            '<@(ppp_entrypoints_sources)',
            'gles2_book/example.h',
            'gles2_book/demo_simple_vertex_shader.cc',
            '../../third_party/gles2_book/Chapter_8/Simple_VertexShader/Simple_VertexShader.c',
            '../../third_party/gles2_book/Chapter_8/Simple_VertexShader/Simple_VertexShader.h',
          ],
        },
        {
          'target_name': 'stencil_test_ppapi',
          'type': 'loadable_module',
          'variables': { 'chromium_code': 0, },
          'dependencies': [ 'gpu_demo_framework_ppapi', ],
          'sources': [
            '<@(ppp_entrypoints_sources)',
            'gles2_book/example.h',
            'gles2_book/demo_stencil_test.cc',
            '../../third_party/gles2_book/Chapter_11/Stencil_Test/Stencil_Test.c',
            '../../third_party/gles2_book/Chapter_11/Stencil_Test/Stencil_Test.h',
          ],
        },
        {
          'target_name': 'texture_wrap_ppapi',
          'type': 'loadable_module',
          'variables': { 'chromium_code': 0, },
          'dependencies': [ 'gpu_demo_framework_ppapi', ],
          'sources': [
            '<@(ppp_entrypoints_sources)',
            'gles2_book/example.h',
            'gles2_book/demo_texture_wrap.cc',
            '../../third_party/gles2_book/Chapter_9/TextureWrap/TextureWrap.c',
            '../../third_party/gles2_book/Chapter_9/TextureWrap/TextureWrap.h',
          ],
        },
      ],
    }],
  ],
}
