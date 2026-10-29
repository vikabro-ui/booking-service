# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/vikabro/back/project1/build/_deps/pqxx-src"
  "/home/vikabro/back/project1/build/_deps/pqxx-build"
  "/home/vikabro/back/project1/build/_deps/pqxx-subbuild/pqxx-populate-prefix"
  "/home/vikabro/back/project1/build/_deps/pqxx-subbuild/pqxx-populate-prefix/tmp"
  "/home/vikabro/back/project1/build/_deps/pqxx-subbuild/pqxx-populate-prefix/src/pqxx-populate-stamp"
  "/home/vikabro/back/project1/build/_deps/pqxx-subbuild/pqxx-populate-prefix/src"
  "/home/vikabro/back/project1/build/_deps/pqxx-subbuild/pqxx-populate-prefix/src/pqxx-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/vikabro/back/project1/build/_deps/pqxx-subbuild/pqxx-populate-prefix/src/pqxx-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/vikabro/back/project1/build/_deps/pqxx-subbuild/pqxx-populate-prefix/src/pqxx-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
