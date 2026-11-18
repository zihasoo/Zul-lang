# 골든 테스트 하나를 돌린다. tests/CMakeLists.txt가 ctest에 등록할 때 이 스크립트를 쓴다.
#
#   cmake -DZUL=<zul 실행 파일> -DNAME=<이름> -DOUT_DIR=<임시 폴더> -P run_test.cmake
#
#   이름.zul       테스트할 소스. tests 폴더에서 `zul 이름.zul`로 실행한다.
#   이름.expected  기대하는 결과. 표준 출력, 표준 에러, "종료 코드: N" 순서로 이어 붙인 것이다.
#   이름.in        (선택) 표준 입력으로 넣을 내용.
#   이름.ir        (선택) `zul -S`가 만든 IR에 그대로 들어 있어야 하는 줄들.
#
# 환경 변수 ZUL_BLESS=1을 주면 비교하지 않고 실제 결과로 이름.expected를 덮어쓴다.
cmake_minimum_required(VERSION 3.26)

set(dir "${CMAKE_CURRENT_LIST_DIR}")

if(NOT EXISTS "${dir}/${NAME}.zul")
    message(FATAL_ERROR "${NAME}.zul 파일이 없습니다")
endif()

# 표준 입력을 터미널에서 물려받으면 `입`을 쓰는 테스트가 입력을 기다리며 멈춘다.
if(EXISTS "${dir}/${NAME}.in")
    set(stdin_file "${dir}/${NAME}.in")
elseif(CMAKE_HOST_WIN32)
    set(stdin_file "NUL")
else()
    set(stdin_file "/dev/null")
endif()

# 표준 출력과 표준 에러를 따로 받는다. 한 파이프로 받으면 버퍼링 때문에 순서가 매번 달라질 수 있다.
execute_process(
        COMMAND "${ZUL}" "${NAME}.zul"
        WORKING_DIRECTORY "${dir}"
        INPUT_FILE "${stdin_file}"
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        RESULT_VARIABLE code
        TIMEOUT 30
        ENCODING NONE)

set(actual "")
foreach(text IN ITEMS out err)
    if(NOT "${${text}}" STREQUAL "")
        string(APPEND actual "${${text}}")
        if(NOT actual MATCHES "\n$")
            string(APPEND actual "\n")
        endif()
    endif()
endforeach()
string(APPEND actual "종료 코드: ${code}\n")
# 줄 끝이 CRLF든 LF든 같은 결과로 본다.
string(REPLACE "\r" "" actual "${actual}")

if("$ENV{ZUL_BLESS}")
    file(WRITE "${dir}/${NAME}.expected" "${actual}")
    message("${NAME}.expected를 실제 결과로 덮어썼습니다")
    return()
endif()

set(failed FALSE)

if(NOT EXISTS "${dir}/${NAME}.expected")
    message("${NAME}.expected 파일이 없습니다. 실제 결과:\n${actual}")
    set(failed TRUE)
else()
    file(READ "${dir}/${NAME}.expected" expected)
    string(REPLACE "\r" "" expected "${expected}")
    if(NOT actual STREQUAL expected)
        message("---- 기대한 결과 (${NAME}.expected)\n${expected}---- 실제 결과\n${actual}----")
        set(failed TRUE)
    endif()
endif()

if(EXISTS "${dir}/${NAME}.ir")
    set(ll_file "${OUT_DIR}/${NAME}.ll")
    file(MAKE_DIRECTORY "${OUT_DIR}")
    execute_process(
            COMMAND "${ZUL}" -S -o "${ll_file}" "${NAME}.zul"
            WORKING_DIRECTORY "${dir}"
            OUTPUT_VARIABLE ir_out
            ERROR_VARIABLE ir_err
            RESULT_VARIABLE ir_code
            TIMEOUT 30
            ENCODING NONE)
    if(NOT ir_code STREQUAL "0")
        message("`zul -S`가 실패했습니다 (종료 코드: ${ir_code})\n${ir_out}${ir_err}")
        set(failed TRUE)
    else()
        file(READ "${ll_file}" ir)
        file(STRINGS "${dir}/${NAME}.ir" wanted_lines)
        foreach(line IN LISTS wanted_lines)
            string(FIND "${ir}" "${line}" pos)
            if(pos EQUAL -1)
                message("IR에 이 줄이 없습니다 (${NAME}.ir): ${line}")
                set(failed TRUE)
            endif()
        endforeach()
        if(failed)
            message("생성된 IR: ${ll_file}")
        endif()
    endif()
endif()

if(failed)
    message(FATAL_ERROR "${NAME} 실패")
endif()
