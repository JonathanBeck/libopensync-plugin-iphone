PROJECT( libopensync-plugin-iphone C )

SET( VERSION "0.38" )

# Look for 3rd Party CMake modules
SET( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules" )

CMAKE_MINIMUM_REQUIRED(VERSION 2.4.4)

FIND_PACKAGE( OpenSync REQUIRED )
FIND_PACKAGE( libiphone REQUIRED )
FIND_PACKAGE( libplist REQUIRED )
FIND_PACKAGE( LibXml2 REQUIRED )
FIND_PACKAGE( LibXslt REQUIRED )

INCLUDE( OpenSyncInternal )

ADD_SUBDIRECTORY( src )

## Packaging

OPENSYNC_PACKAGE( ${PROJECT_NAME} ${VERSION} )

