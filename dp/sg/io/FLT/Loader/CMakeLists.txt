find_package( NVfltlib )

if (FLTLIB_FOUND)
  #includes
  include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${FLTLIB_INCLUDE_DIRS}"
  )

  #definitions
  add_definitions(
    "-DFLTLOADER_EXPORTS"
  )

  if(WIN32)
    add_definitions( "/wd4996" )
  endif()

  #source
  set( FLTLOADER_SOURCES
    nvsgInterface.cpp
    textureMaterial.cpp
    TaiManager.cpp
    StackedAtlas.cpp
    FLTLoader.cpp
  )

  set( FLTLOADER_HEADERS
    TaiManager.h
    StackedAtlas.h
    FLTLoader.h
  )

  source_group(source FILES ${FLTLOADER_SOURCES})
  source_group(header FILES ${FLTLOADER_HEADERS})

  #target
  add_library( FLTLoader SHARED
    ${FLTLOADER_HEADERS}
    ${FLTLOADER_SOURCES}
  )

  target_link_libraries( FLTLoader 
    DP
    DPSgCore
    DPSgUI
    DPSgAlgorithm
    DPMath
    DPUtil
    DPFx
    DPSgIO
    ${FLTLIB_LIBRARIES}
    ${OPENGL_glu_LIBRARY}
  )

  set_target_properties( FLTLoader PROPERTIES SUFFIX ".nxm" FOLDER "Loaders")
else()
  message("fltlib not found, skipping FLTLoader.")
endif()