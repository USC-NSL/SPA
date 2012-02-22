#===-- runtime/Runtest/Makefile ----------------------------*- Makefile -*--===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

LEVEL=../..

USEDLIBS = kleaverSolver.a kleaverExpr.a kleeSupport.a kleeBasic.a kleeModule.a kleeRuntest.a
LINK_COMPONENTS = support
LIBRARYNAME=MaxAdversarialModule
SHARED_LIBRARY=1
LINK_LIBS_IN_SHARED = 1
DONT_BUILD_RELINKED = 1
NO_PEDANTIC=1

include $(LEVEL)/Makefile.common
