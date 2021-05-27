set(VDDS_HPP
	${PROJECT_SOURCE_DIR}/include/vdds/data.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/domain.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/topic.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/notifier.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/sub-queue.hpp 
	${PROJECT_SOURCE_DIR}/include/vdds/pub-handle.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/pub.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/sub.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/query.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/strcache.hpp
	${PROJECT_SOURCE_DIR}/include/vdds/utils.hpp)

add_library(vdds SHARED ${VDDS_HPP}
	topic.cc
	domain.cc
	query.cc
	pub-handle.cc
	sub-queue.cc
	strcache.cc
	utils.cc)

target_include_directories(vdds PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(vdds PUBLIC hogl)

set_target_properties(vdds PROPERTIES VERSION ${VDDS_VERSION})

install(TARGETS vdds DESTINATION lib COMPONENT dev)
install(FILES ${VDDS_HPP} DESTINATION include/vdds COMPONENT dev)

