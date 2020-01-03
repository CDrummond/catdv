# - Try to find LibDv
# Once done this will define
#
#  LIBDV_FOUND - system has LibDv
#  LIBDV_INCLUDE_DIR - the LibDv include directory
#  LIBDV_LIBRARIES - the libraries needed to use LibDv
#  LIBDV_DEFINITIONS - Compiler switches required for using LibDv

# IF (LIBDV_INCLUDE_DIR AND LIBDV_LIBRARIES)
   # in cache already
#   SET(LibDv_FIND_QUIETLY TRUE)
# ELSE (LIBDV_INCLUDE_DIR AND LIBDV_LIBRARIES)
    # use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    INCLUDE(UsePkgConfig)
    PKGCONFIG(libdv _LibDvIncDir _LibDvLinkDir _LibDvLinkFlags _LibDvCflags)
    SET(LIBDV_DEFINITIONS ${_LibDvCflags})

    FIND_PATH(LIBDV_INCLUDE_DIR dv.h
                PATHS
                ${_LibDvIncDir}
                PATH_SUFFIXES libdv
                )

    FIND_LIBRARY(LIBDV_LIBRARIES NAMES dv
                    PATHS
                    ${_LibDvLinkDir}
                    )

    IF (LIBDV_INCLUDE_DIR AND LIBDV_LIBRARIES)
        SET(LIBDV_FOUND TRUE)
    ELSE (LIBDV_INCLUDE_DIR AND LIBDV_LIBRARIES)
        SET(LIBDV_FOUND FALSE)
    ENDIF (LIBDV_INCLUDE_DIR AND LIBDV_LIBRARIES)

    IF (LIBDV_FOUND)
        IF (NOT LibDv_FIND_QUIETLY)
            MESSAGE(STATUS "Found LibDv: ${LIBDV_LIBRARIES}")
        ENDIF (NOT LibDv_FIND_QUIETLY)
    ELSE (LIBDV_FOUND)
        IF (LibDv_FIND_REQUIRED)
            MESSAGE(SEND_ERROR "Could NOT find LibDv")
        ENDIF (LibDv_FIND_REQUIRED)
    ENDIF (LIBDV_FOUND)

    # MARK_AS_ADVANCED(LIBDV_INCLUDE_DIR LIBDV_LIBRARIES)
# ENDIF (LIBDV_INCLUDE_DIR AND LIBDV_LIBRARIES)
