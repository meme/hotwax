project('hotwax', 'c')

cc = meson.get_compiler('c')
source_dir = meson.current_source_dir()
host_os = host_machine.system()

resolv_dep = cc.find_library('resolv')
threads_dep = dependency('threads')

gum_dep = cc.find_library('frida-gum',
    has_headers: ['frida-gum.h'],
    static: true,
    dirs: [source_dir],
    header_args: '-I' + source_dir,
)

gum_dependencies = [resolv_dep, gum_dep]

if host_os == 'linux'
    dl_dep = cc.find_library('dl')
    gum_dependencies += [dl_dep]
endif

if host_os == 'linux'
    lzma_dep = cc.find_library('lzma',
        static: true
    )
    gum_dependencies += [lzma_dep]
endif

gum_dep = declare_dependency(
    dependencies: gum_dependencies,
    include_directories: '.',
)

hotwax_lib = static_library('hotwax',
    sources: ['basic_block.c', 'instr.c'],
)

hotwax_dep = declare_dependency(
    link_with: [hotwax_lib],
    dependencies: [threads_dep, gum_dep],
)

subdir('targets')
