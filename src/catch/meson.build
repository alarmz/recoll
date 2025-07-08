catchtests_incdirs = [librecoll_incdir, '..', '../utils',]

catchtests_sources = [
    'catchtests.cpp',
    'testmime.cpp',
]
catchtests = executable(
    'catchtests',
    catchtests_sources,
    include_directories: catchtests_incdirs,
    link_with: librecoll,
    install: false,
)
