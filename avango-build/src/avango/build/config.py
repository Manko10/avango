# -*- Mode:Python -*-

##########################################################################
#                                                                        #
# This file is part of AVANGO.                                           #
#                                                                        #
# Copyright 1997 - 2009 Fraunhofer-Gesellschaft zur Foerderung der       #
# angewandten Forschung (FhG), Munich, Germany.                          #
#                                                                        #
# AVANGO is free software: you can redistribute it and/or modify         #
# it under the terms of the GNU Lesser General Public License as         #
# published by the Free Software Foundation, version 3.                  #
#                                                                        #
# AVANGO is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of         #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           #
# GNU General Public License for more details.                           #
#                                                                        #
# You should have received a copy of the GNU Lesser General Public       #
# License along with AVANGO. If not, see <http://www.gnu.org/licenses/>. #
#                                                                        #
##########################################################################

from environment import *
from configstore import _config_store
import distutils.sysconfig
import SCons.Script as scons
import oshelper

_do_parse_config = True
_do_recurse = True

class Config(object):
    def get_library_path(self):
        return []

    def get_include_path(self):
        return []

    def get_python_path(self):
        return []

    def get_libraries(self):
        return []

class PlainConfig(Config):

    def __init__(self, library_path = [], include_path = [], python_path = [], libraries = [], dependencies = []):
        self._library_path = library_path
        self._include_path = include_path
        self._python_path = python_path
        self._libraries = libraries
        self._dependencies = dependencies

    def _join_dependencies(self, retriever):
        result = []
        for d in self._dependencies:
            result += retriever(d)
        return result

    def get_library_path(self):
        return self._library_path+self._join_dependencies(_config_store.get_library_path)

    def get_include_path(self):
        return self._include_path+self._join_dependencies(_config_store.get_include_path)

    def get_python_path(self):
        return self._python_path+self._join_dependencies(_config_store.get_python_path)

    def get_libraries(self):
        return self._libraries+self._join_dependencies(_config_store.get_libraries)

class PKGConfig(Config):

    def __init__(self, name):
        self._name = name

    def get_library_path(self):
        if not _do_parse_config:
            return []
        return Environment.env.ParseFlags('!pkg-config --libs-only-L %s' % self._name)['LIBPATH']

    def get_include_path(self):
        if not _do_parse_config:
            return []
        return Environment.env.ParseFlags('!pkg-config --cflags-only-I %s' % self._name)['CPPPATH']

    def get_libraries(self):
        if not _do_parse_config:
            return []
        return Environment.env.ParseFlags('!pkg-config --libs-only-l %s' % self._name)['LIBS']

class PythonConfig(Config):

    def get_library_path(self):
        result = distutils.sysconfig.get_config_var('LIBDIR')
        if not result:
            return []
        return [result]

    def get_include_path(self):
        result = distutils.sysconfig.get_python_inc()
        if not result:
            return []
        return [result]

    def get_libraries(self):
        if oshelper.os_is_mac():
            # FIXME find proper way to detect this
            return ['Python']
        env = scons.Environment()
        flags = env.ParseFlags(distutils.sysconfig.get_config_var('BLDLIBRARY'))
        return flags['LIBS']

class BoostConfig(PlainConfig):

    def __init__(self, name, dependencies = []):
        super(BoostConfig, self).__init__(dependencies = dependencies)
        self._name = name

    def get_libraries(self):
        result = self._name
        env = Environment.env
        if env['BOOST_LAYOUT'] == 'versioned':
            result += '-' + env['BOOST_TOOLSET']
        if env['BOOST_LAYOUT'] in ['versioned', 'tagged']:
            result += '-mt'
        if env['BOOST_LAYOUT'] in ['versioned', 'tagged'] and env['BOOST_DEBUG']:
                result += '-d'
        if env['BOOST_LAYOUT'] == 'versioned':
            result += '-'+env['BOOST_LIB_VERSION']
        return [result]+super(BoostConfig, self).get_libraries()
