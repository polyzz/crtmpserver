FIND_PATH(CURL_INCLUDE_PATH 
	NAMES
		curl/curl.h
  PATHS
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
  NO_DEFAULT_PATH)

FIND_LIBRARY(CURL_LIBRARY_PATH
	NAMES
		curl
	PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
  NO_DEFAULT_PATH)

IF(CURL_INCLUDE_PATH)
	SET(CURL_FOUND 1)
	MESSAGE(STATUS "Looking for curl headers - found")
ELSE(CURL_INCLUDE_PATH)
	SET(CURL_FOUND 0)
	MESSAGE(STATUS "Looking for curl headers - not found")
ENDIF(CURL_INCLUDE_PATH)

IF(CURL_LIBRARY_PATH)
  SET(CURL_FOUND 1)
  MESSAGE(STATUS "Looking for curl library - found")
ELSE(CURL_LIBRARY_PATH)
  SET(CURL_FOUND 0)
  MESSAGE(STATUS "Looking for curl library - not found")
ENDIF(CURL_LIBRARY_PATH)

MARK_AS_ADVANCED(CURL_FOUND)
