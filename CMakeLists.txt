CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(mmdbridge) # .sln

# for UTF8 source codes
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# shared compiler options
SET(CMAKE_CXX_FLAGS "/EHsc /wd4267"
    )
# embeded pdb 
SET(CMAKE_CXX_FLAGS_DEBUG
    /Z7
    )

# path settings
SET(PYTHON_ROOT
    "C:/Program Files/Python313"
    CACHE PATH "Python 3.13 root directory")
SET(PYTHON_INCLUDE_DIRS
    ${PYTHON_ROOT}/Include)
SET(PYTHON_LIBRARIES
    ${PYTHON_ROOT}/libs/python313.lib)
SET(PYTHON_DLL
    ${PYTHON_ROOT}/python313.dll)
SET(PYTHON_STDLIB_DIR
    ${PYTHON_ROOT}/Lib)

#FIND_PACKAGE(pybind11 REQUIRED)
SET(pybind11_INCLUDE_DIRS
    ${CMAKE_INSTALL_PREFIX}/include
    ${PYTHON_INCLUDE_DIRS}
    )

if(DEFINED ENV{MMD_DIR})
    SET(MMD_DIR
        $ENV{MMD_DIR}
        )
else()
    GET_FILENAME_COMPONENT(MMD_DIR
        ${CMAKE_CURRENT_LIST_DIR}/MikuMikuDance_x64
        ABSOLUTE
        )
endif()

#FIND_PACKAGE(ilmbase REQUIRED)
SET(ilmbase_INCLUDE_DIRS 
    ${CMAKE_INSTALL_PREFIX}/include/Imath)
#FIND_PACKAGE(Alembic REQUIRED)
SET(ALEMBIC_INCLUDE_DIRS
    ${CMAKE_INSTALL_PREFIX}/include)
FILE(GLOB IMATH_LIBRARIES
    ${CMAKE_INSTALL_PREFIX}/lib/Imath-*.lib)
FILE(GLOB IMATH_DLLS
    ${CMAKE_INSTALL_PREFIX}/bin/Imath-*.dll)
SET(ALEMBIC_LIBRARIES
    ${CMAKE_INSTALL_PREFIX}/lib/Alembic.lib
    ${CMAKE_INSTALL_PREFIX}/lib/hdf5.lib
    ${IMATH_LIBRARIES}
    ${CMAKE_INSTALL_PREFIX}/lib/szip.lib)
if(EXISTS ${CMAKE_INSTALL_PREFIX}/lib/hdf5_hl.lib)
    LIST(APPEND ALEMBIC_LIBRARIES
        ${CMAKE_INSTALL_PREFIX}/lib/hdf5_hl.lib)
endif()
if(EXISTS ${CMAKE_INSTALL_PREFIX}/lib/zlib.lib)
    LIST(APPEND ALEMBIC_LIBRARIES
        ${CMAKE_INSTALL_PREFIX}/lib/zlib.lib)
endif()

# projects
ADD_SUBDIRECTORY(src)

# additional install files
INSTALL(FILES 
    ${PYTHON_DLL}
    ${CMAKE_INSTALL_PREFIX}/bin/Alembic.dll
    ${CMAKE_INSTALL_PREFIX}/bin/hdf5.dll
    ${IMATH_DLLS}
    ${CMAKE_INSTALL_PREFIX}/bin/szip.dll
    DESTINATION ${MMD_DIR}
    )
if(EXISTS ${CMAKE_INSTALL_PREFIX}/bin/hdf5_hl.dll)
    INSTALL(FILES ${CMAKE_INSTALL_PREFIX}/bin/hdf5_hl.dll
        DESTINATION ${MMD_DIR}
        )
endif()
if(EXISTS ${CMAKE_INSTALL_PREFIX}/bin/z.dll)
    INSTALL(FILES ${CMAKE_INSTALL_PREFIX}/bin/z.dll
        DESTINATION ${MMD_DIR}
        )
elseif(EXISTS ${CMAKE_INSTALL_PREFIX}/bin/zlib1.dll)
    INSTALL(FILES ${CMAKE_INSTALL_PREFIX}/bin/zlib1.dll
        DESTINATION ${MMD_DIR}
        )
endif()
FILE(GLOB python_sciprts Release/x64/*.py
    )
INSTALL(FILES ${python_sciprts}
    DESTINATION ${MMD_DIR}
    )

FILE(INSTALL ${PYTHON_STDLIB_DIR} DESTINATION ${MMD_DIR})

INSTALL(DIRECTORY Release/x64/alembic_assign_scripts 
    DESTINATION ${MMD_DIR}
    )
install(DIRECTORY 
    DESTINATION ${MMD_DIR}/out)

