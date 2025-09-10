cmake_minimum_required(VERSION 3.5)
project(liboculus)

# == Code common to all builds =======================================
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

if( FIPS_CONFIG AND NOT FIPS_IMPORT )
  get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
  include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

  fips_setup()

  find_package( Boost REQUIRED COMPONENTS system )
  find_package( g3log REQUIRED)

  #== fips-specific section ==========================================
  add_compile_options(-std=c++17)

  ## Set global include paths
  fips_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
  )

  ## Verbose output about ASIO handlers
  #add_definitions( -DBOOST_ASIO_ENABLE_HANDLER_TRACKING )

  fips_add_subdirectory("lib/")

  if( NOT FIPS_IMPORT )
    add_subdirectory("tools/")
  endif()

  if( FIPS_UNITTESTS )
    add_subdirectory( test/ )
  endif()

  fips_finish()

else()

  # ROS build, determine which version...
  if(NOT DEFINED ENV{ROS_VERSION})
    set(ROS_VERSION 2)
  else()
    set(ROS_VERSION $ENV{ROS_VERSION})
  endif()

  set(oculus_SRCS
              lib/DataRx.cpp
              lib/SonarConfiguration.cpp
              lib/SonarStatus.cpp
              lib/StatusRx.cpp
              lib/SonarPlayer.cpp
              lib/OculusMessageHandler.cpp
              lib/SimpleFireMessage.cpp
              lib/IoServiceThread.cpp )


  if(ROS_VERSION EQUAL 2)
    # == ament/ROS2 section =================================

    find_package(ament_cmake REQUIRED)
    find_package(Boost REQUIRED COMPONENTS system)
    find_package(g3log REQUIRED)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    add_compile_options(-std=c++17)

    include_directories(
      include
      thirdparty
    )

    add_library(oculus SHARED ${oculus_SRCS})
    target_link_libraries(oculus
      Boost::system
      g3log
    )
    ament_target_dependencies(oculus
      Boost
      g3log
    )

    install(
      TARGETS oculus
      EXPORT export_${PROJECT_NAME}
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin
    )

    install(
      DIRECTORY include/${PROJECT_NAME}/ thirdparty/
      DESTINATION include
      FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
      PATTERN ".git" EXCLUDE
    )

    ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
    ament_export_include_directories(include)
    ament_export_libraries(oculus)

    ament_package()

  elseif(ROS_VERSION EQUAL 1)

    # Catkin/ROS1 section =====
    find_package( catkin REQUIRED COMPONENTS
                  g3log_ros )
    find_package( Boost REQUIRED COMPONENTS system)

    catkin_package(
        CATKIN_DEPENDS g3log_ros
        INCLUDE_DIRS include thirdparty
        LIBRARIES liboculus
    )

    add_compile_options(-std=c++14)

    add_library(liboculus
                ${oculus_SRCS})

    include_directories(liboculus
      include
      thirdparty
      ${catkin_INCLUDE_DIRS}
    )

    target_link_libraries(liboculus
                ${catkin_LIBRARIES}
    )

    install(TARGETS liboculus
      ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
      LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
      RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
    )

    ## Install headers
    install(DIRECTORY  include/${PROJECT_NAME}/ thirdparty/
    DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
                PATTERN ".git" EXCLUDE)

    if (CATKIN_ENABLE_TESTING)
      add_definitions(-DTEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test/data")
      include_directories( test/data/ )

      file(GLOB oculus_test_SRCS
                test/unit/*cpp )

      catkin_add_gtest(oculus_test ${oculus_test_SRCS})

      target_link_libraries(oculus_test ${catkin_LIBRARIES}
                liboculus
                Boost::system )
    endif()

  else()
    message("Unsure what sort of build to do.  Not in a FIPS, ROS1 or ROS2 environment")
  endif()


endif()
