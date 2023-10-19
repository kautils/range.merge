set(${PROJECT_NAME}_m_evacu ${m})
set(m ${PROJECT_NAME})
list(APPEND ${m}_unsetter )


if(NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
    file(DOWNLOAD https://raw.githubusercontent.com/kautils/CMakeKautilHeader/v0.0.1/CMakeKautilHeader.cmake ${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
endif()
include(${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeFetchKautilModule/v0.0.1/CMakeFetchKautilModule.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeLibrarytemplate/v0.0.1/CMakeLibrarytemplate.cmake)


CMakeFetchKautilModule(${m}_kautil_region GIT https://github.com/kautils/region.git       REMOTE origin BRANCH v0.0)
CMakeFetchKautilModule(${m}_kautil_btree GIT https://github.com/kautils/btree_search.git REMOTE origin BRANCH v1.0)


list(APPEND ${m}_unsetter  ${m}_merge_hpp)
file(GLOB ${m}_merge_hpp ${CMAKE_CURRENT_LIST_DIR}/*.hpp)
install(FILES ${${m}_merge_hpp} DESTINATION include/kautil/range/merge )
install(SCRIPT "${${m}_kautil_btree.BUILD_DIR}/cmake_install.cmake")
install(SCRIPT "${${m}_kautil_region.BUILD_DIR}/cmake_install.cmake")


find_package(KautilRegion.0.0.1.interface REQUIRED)
find_package(KautilAlgorithmBtreeSearch.1.0.1.interface REQUIRED)

string(APPEND ${m}_findpkgs
    "if(EXISTS \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilRegion.0.0.1\")\n"
    "\t set(KautilRegion.0.0.1.interface_DIR \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilRegion.0.0.1\")\n"
    "\t find_package(KautilRegion.0.0.1.interface REQUIRED)\n"
    "endif()\n"
        
    "if(EXISTS \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilAlgorithmBtreeSearch.1.0.1\")\n"
    "\t set(KautilAlgorithmBtreeSearch.1.0.1_DIR \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilAlgorithmBtreeSearch.1.0.1\")\n"
    "\t find_package(KautilAlgorithmBtreeSearch.1.0.1.interface REQUIRED)\n"
    "endif()\n"
)


set(module_name merge)
unset(srcs)
file(GLOB srcs ${CMAKE_CURRENT_LIST_DIR}/*.cc)
set(${module_name}_common_pref
    MODULE_PREFIX kautil range
    MODULE_NAME ${module_name}
    INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> $<INSTALL_INTERFACE:include>  
    SOURCES ${srcs}
    LINK_LIBS 
        kautil::region::0.0.1::interface
        kautil::algorithm::btree_search::1.0.1::interface
    EXPORT_NAME_PREFIX ${PROJECT_NAME}
    EXPORT_VERSION ${PROJECT_VERSION}
    EXPORT_VERSION_COMPATIBILITY AnyNewerVersion
        
    DESTINATION_INCLUDE_DIR include/kautil/range/merge
    DESTINATION_CMAKE_DIR cmake
    DESTINATION_LIB_DIR lib
)


CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE interface ${${module_name}_common_pref} )


set(__t ${${module_name}_interface_tmain})
add_executable(${__t})
target_sources(${__t} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/unit_test.cc)
target_link_libraries(${__t} PRIVATE ${${module_name}_static} kautil::algorithm::btree_search::1.0.1::interface kautil::region::0.0.1::interface )
target_compile_definitions(${__t} PRIVATE ${${module_name}_interface_tmain_ppcs})

foreach(__v ${${m}_unsetter})
    unset(${__v})
endforeach()
unset(${m}_unsetter)
set(m ${${PROJECT_NAME}_m_evacu})
