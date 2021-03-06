# - Try to find libiphone 
# Find libiphone headers, libraries and the answer to all questions.
#
#  LIBIPHONE_FOUND               True if libiphone got found
#  LIBIPHONE_INCLUDE_DIRS        Location of libiphone headers 
#  LIBIPHONE_LIBRARIES           List of libaries to use libiphone 
#
# Copyright (c) 2009 Jonathan Beck <jonabeck@gmail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

INCLUDE( FindPkgConfig )

IF ( LIBIPHONE_FIND_REQUIRED )
	SET( _pkgconfig_REQUIRED "REQUIRED" )
ELSE( LIBIPHONE_FIND_REQUIRED )
	SET( _pkgconfig_REQUIRED "" )	
ENDIF ( LIBIPHONE_FIND_REQUIRED )

IF ( LIBIPHONE_MIN_VERSION )
	PKG_SEARCH_MODULE( LIBIPHONE ${_pkgconfig_REQUIRED} libiphone-1.0>=${LIBIPHONE_MIN_VERSION} )
ELSE ( LIBIPHONE_MIN_VERSION )
	PKG_SEARCH_MODULE( LIBIPHONE ${_pkgconfig_REQUIRED} libiphone-1.0 )
ENDIF ( LIBIPHONE_MIN_VERSION )


IF( NOT LIBIPHONE_FOUND AND NOT PKG_CONFIG_FOUND )
	FIND_PATH( LIBIPHONE_INCLUDE_DIRS libiphone/libiphone.h )
	FIND_LIBRARY( LIBIPHONE_LIBRARIES libiphone)

	# Report results
	IF ( LIBIPHONE_LIBRARIES AND LIBIPHONE_INCLUDE_DIRS )	
		SET( LIBIPHONE_FOUND 1 )
		IF ( NOT LIBIPHONE_FIND_QUIETLY )
			MESSAGE( STATUS "Found libiphone: ${LIBIPHONE_LIBRARIES}" )
		ENDIF ( NOT LIBIPHONE_FIND_QUIETLY )
	ELSE ( LIBIPHONE_LIBRARIES AND LIBIPHONE_INCLUDE_DIRS )	
		IF ( LIBIPHONE_FIND_REQUIRED )
			MESSAGE( SEND_ERROR "Could NOT find libiphone" )
		ELSE ( LIBIPHONE_FIND_REQUIRED )
			IF ( NOT LIBIPHONE_FIND_QUIETLY )
				MESSAGE( STATUS "Could NOT find libiphone" )	
			ENDIF ( NOT LIBIPHONE_FIND_QUIETLY )
		ENDIF ( LIBIPHONE_FIND_REQUIRED )
	ENDIF ( LIBIPHONE_LIBRARIES AND LIBIPHONE_INCLUDE_DIRS )
ENDIF( NOT LIBIPHONE_FOUND AND NOT PKG_CONFIG_FOUND )

IF ( LIBIPHONE_FIND_REQUIRED AND NOT LIBIPHONE_FOUND)
	MESSAGE( FATAL_ERROR "Could NOT find libiphone")
ENDIF ( LIBIPHONE_FIND_REQUIRED AND NOT LIBIPHONE_FOUND)

IF ( LIBIPHONE_FOUND AND NOT LIBIPHONE_FIND_QUIETLY )
	MESSAGE( STATUS "Found libiphone: ${LIBIPHONE_LIBRARIES}" )
ENDIF ( LIBIPHONE_FOUND AND NOT LIBIPHONE_FIND_QUIETLY )

MARK_AS_ADVANCED( LIBIPHONE_LIBRARIES LIBIPHONE_INCLUDE_DIRS )
