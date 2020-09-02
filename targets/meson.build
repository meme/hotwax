target_lib = static_library('target', 'target.c')

fork_exe = executable('target_fork',
    sources: ['target_fork.c'],
    link_with: [target_lib],
    dependencies: [hotwax_dep],
)

persistent_exe = executable('target_persistent',
    sources: ['target_persistent.c'],
    link_with: [target_lib],
    dependencies: [hotwax_dep],
)