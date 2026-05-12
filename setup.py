import sys
from setuptools import setup, Extension

class get_pybind_include(object):
    def __str__(self):
        import pybind11
        return pybind11.get_include()


compile_args = ['-std=c++17', '-O3', '-Wall']
if sys.platform == 'win32':
    compile_args = ['/O2', '/std:c++17'] 

ext_modules = [
    Extension(
        'c_eclat',
        ['eclat.cpp'],
        include_dirs=[get_pybind_include()],
        language='c++',
        extra_compile_args=compile_args,
    ),
]

setup(
    name='c_eclat',
    ext_modules=ext_modules,
)