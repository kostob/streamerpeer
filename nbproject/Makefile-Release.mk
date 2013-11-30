#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Generic
CND_DLIB_EXT=so
CND_CONF=Release
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/network.o \
	${OBJECTDIR}/streamer.o \
	${OBJECTDIR}/threads.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-L/usr/home/tobias/dev/GRAPES/src -L/usr/local/lib -Wl,-rpath,. -lpthread -lgrapes -lavcodec -lavdevice -lavfilter -lavformat -lavresample -lavutil

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/streamerpeer

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/streamerpeer: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/streamerpeer ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/network.o: nbproject/Makefile-${CND_CONF}.mk network.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -I/usr/home/tobias/dev/GRAPES/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/network.o network.c

${OBJECTDIR}/streamer.o: nbproject/Makefile-${CND_CONF}.mk streamer.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -I/usr/home/tobias/dev/GRAPES/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/streamer.o streamer.c

${OBJECTDIR}/threads.o: nbproject/Makefile-${CND_CONF}.mk threads.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -O2 -I/usr/home/tobias/dev/GRAPES/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/threads.o threads.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/streamerpeer

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
