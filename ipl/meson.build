project('ipl', 'c')

picolibc = subproject('picolibc', default_options: [ 'multilib=false', 'semihost=false'] )

picolibc_dep = picolibc.get_variable('picolibc_dep')

executable(
    'ipl',
    [ 'start.S', 'main.c' ] ,
    c_args: ['-ffreestanding', '-fno-pie'],
    link_args: [
        '-static',
        '-nolibc',
        '-nostartfiles',
        '-T',
        meson.current_source_dir() / 'linker.ld',
    ],
    link_depends: 'linker.ld',
)
