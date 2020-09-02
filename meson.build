project('hotwax', 'c')

cc = meson.get_compiler('c')
source_dir = meson.current_source_dir()

resolv_dep = cc.find_library('resolv')
sqlite3_dep = cc.find_library('sqlite3')
threads_dep = dependency('threads')

gum_dep = cc.find_library('frida-gum',
    has_headers: ['frida-gum.h'],
    static: true,
    dirs: [source_dir],
    header_args: '-I' + source_dir,
)

gum_dep = declare_dependency(
    dependencies: [resolv_dep, gum_dep],
    include_directories: '.',
)

hotwax_lib = static_library('hotwax',
    sources: ['basic_block.c', 'instr.c'],
)

hotwax_dep = declare_dependency(
    link_with: [hotwax_lib],
    dependencies: [threads_dep, sqlite3_dep, gum_dep],
)

subdir('targets')