#!/usr/bin/env python3
from distutils.core import setup, Extension
from distutils.command.build import build

VERSION='0.3'

class build_and_make_exec(build):
    def run(self):
        from distutils.ccompiler import new_compiler
        comp = new_compiler()
        comp.compile(['pfa/pfa.c'], extra_preargs=['-Wall',
            '-fno-omit-frame-pointer', '-Os'])
        comp.link_executable(['pfa/pfa.o'], 'pfa/pfa')
        comp.link_executable(['pfa/pfa.o'], 'pfa/pfai')
        build.run(self)

setup(name='pfa', packages=['pfa',], version=VERSION,

    author = "M Stoeckl",
    author_email = "mstoeckl@u.rochester.edu",
    maintainer = "M Stoeckl",
    maintainer_email = "mstoeckl@u.rochester.edu",

    description = "Very fast and consistent (if ugly) autoformatting for Python",
    url = "https://github.com/mstoeckl/python-fast-autoformat",
    download_url = "https://github.com/mstoeckl/python-fast-autoformat/archive/"+VERSION+".tar.gz",

    long_description = open("README.md").read(),

    keywords = ['python', 'autoformat', 'c', 'fast', 'format', 'formatter'],

    cmdclass = {'build':build_and_make_exec},

    data_files = [('bin/', ['pfa/pfa', 'pfa/pfai'])],

    classifiers = ["License :: OSI Approved :: MIT License",
        "Intended Audience :: Developers",
        "Programming Language :: C",
        "Programming Language :: Python :: 3",
        "Operating System :: POSIX :: Linux", "Topic :: Utilities"],
)
