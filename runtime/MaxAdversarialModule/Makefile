#===-- runtime/Runtest/Makefile ----------------------------*- Makefile -*--===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

LEVEL=../..

STP_LIBS := stp_c_interface.a stp_AST.a stp_bitvec.a stp_constantbv.a stp_sat.a \
        stp_simplifier.a

USEDLIBS = spaRuntime.a kleaverSolver.a kleaverExpr.a kleeSupport.a kleeBasic.a kleeModule.a cloud9instrum.a
USEDLIBS += $(STP_LIBS)

LINK_COMPONENTS = jit engine bitreader bitwriter linker ipo

LIBS += -lboost_system-mt -lboost_thread-mt -lprotobuf
LINK_COMPONENTS = support
LIBRARYNAME=MaxAdversarialModule
SHARED_LIBRARY=1
LINK_LIBS_IN_SHARED = 1
DONT_BUILD_RELINKED = 1
NO_PEDANTIC=1

include $(LEVEL)/Makefile.common

LIBS += -lboost_system-mt -lboost_thread-mt -lprotobuf
