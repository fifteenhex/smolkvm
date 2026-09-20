project('ipl', 'c')

picolibc = subproject('picolibc', default_options: [ 'multilib=false', 'semihost=false', 'picocrt=false', 'format-default=integer', 'test-long-double=false' ] )

picolibc_dep = picolibc.get_variable('picolibc_dep')

inc = include_directories('include')

executable(
    'ipl',
    [ 'start.S', 'main.c', 'stdio.c' ] ,
    link_args: [
        '-T',
        meson.current_source_dir() / 'linker.ld',
    ],
    dependencies: picolibc_dep,
    link_depends: 'linker.ld',
    include_directories : inc,
)
