# tests/이름.zul 하나가 ctest 테스트 하나가 된다. 비교 방식은 run_test.cmake에 적혀 있다.
# CONFIGURE_DEPENDS: .zul 파일을 추가하거나 지우면 다음 빌드 때 cmake가 알아서 다시 돈다.
file(GLOB zul_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.zul")

foreach(source IN LISTS zul_test_sources)
    get_filename_component(name "${source}" NAME_WE)
    add_test(NAME ${name}
            COMMAND ${CMAKE_COMMAND}
            -DZUL=$<TARGET_FILE:zul>
            -DNAME=${name}
            -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
endforeach()
