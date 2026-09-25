# SPDX-License-Identifier: CC0-1.0

foreach algorithm : ['af', 'agc']
    exe = executable('simple-' + algorithm,
                     [algorithm + '.cpp',
                      '../../../src/ipa/simple/algorithms/' + algorithm + '.cpp'],
                     dependencies : [libcamera_private, libipa_dep],
                     implicit_include_directories : false,
                     include_directories : [test_includes_internal,
                                            '../../../src/', '../../../src/ipa/simple/', '.'])
    test('simple-' + algorithm, exe, suite : 'ipa-simple')
endforeach
