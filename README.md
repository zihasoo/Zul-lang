The Zul Programming Language
=

![](imgs/줄랭_한글배너.png)

한글 줄임말로 된 프로그래밍 언어 "줄랭" (줄임말 랭귀지)

줄랭은 간단한 문법으로 컴팩트하고 빠른 프로그램을 만들기 위해 고안된 언어입니다.  
C/C++과 비슷한 내부 구조를 가지지만, 정적 타입 추론을 적극 활용해 불필요한 명세를 줄여 더 빠르고 편하게 코드를 작성할 수 있습니다.   
LLVM을 기반으로 동작해 LLVM Infrastructure의 강력한 도구들을 활용할 수 있고, C/C++과 쉽게 링킹이 가능합니다.

**주의사항**:

- **반드시 터미널 인코딩을 UTF-8로 설정하고 실행해야 합니다. 아래 [알려진 문제](#알려진-문제)를 참고하세요**
- 줄랭은 알파 단계에 있습니다. 컴파일러에 대한 깊은 지식 없이 단기간에 개발되었고, 개발자도 알지 못하는 버그와 미스 컴파일링 가능성이 난무합니다.
- 정확한 문법 지원 현황은 아래 [지원 현황](#문법-지원-현황) 에서 확인할 수 있습니다.

## 목차

1. [시작하기](#시작하기)
2. [줄랭의 문법](#줄랭의-문법)
3. [코드 예시](#코드-예시)
4. [알려진 문제](#알려진-문제)
5. [줄랭 컴파일러](#줄랭-컴파일러)
6. [문법 지원 현황](#문법-지원-현황)
7. [참고 자료](#참고-자료)

## 시작하기

빌드된 줄랭 컴파일러는 [다운로드 페이지](https://github.com/zihasoo/Zul-lang/releases) 를 참고하세요.

- 윈도우에서 줄랭 컴파일러를 실행했을 때 `msvcp140.dll이 없어 코드 실행을 진행할 수 없습니다` 
와 같은 오류가 발생한다면 `microsoft visual c++ 2015-2019 redistributable`가 없는 것입니다.   
대부분의 윈도우 프로그램이 해당 패키지를 요구하기 때문에 설치하는 것이 좋습니다.
[Microsoft Visual C++ 재배포 가능 패키지 다운로드](https://learn.microsoft.com/ko-kr/cpp/windows/latest-supported-vc-redist?view=msvc-170) 
에서 설치 가능합니다.

줄랭 컴파일러의 코드를 수정하고, 빌드하고 싶다면 LLVM을 설치해야 합니다. 또한 cmake, C++ 빌드 시스템, C/C++ 컴파일러가 필요합니다.
자세한 내용은 [줄랭 세팅 방법](./zullang_TMI.md#줄랭-세팅-방법)을 참고하세요.

## 줄랭의 문법
줄랭의 문법에 대한 설명은 [줄랭 Documentation](./zullang_documentation.md) 에서 확인하실 수 있습니다.

## 코드 예시:

줄랭으로 구현한 퀵 정렬 알고리즘입니다. 기본적으로 C라이브러리와 링킹이 되기 때문에,
아래의 clock과 rand처럼 선언만 해준다면 C언어의 함수를 쉽게 사용할 수 있습니다.

줄랭의 주석은 //를 사용합니다. 다른 언어와 비슷한 줄 주석으로, //부터 줄 끝까지가 무시됩니다.

``` 

ㅎㅇ clock() 수
ㅎㅇ rand() 수

//정수 1천만개 정렬 시간: 672ms
배열크기 = 10000000
배열: 수[10000000]

ㅎㅇ 치환(ㄱ: 수, ㄴ: 수):
    임시 = 배열[ㄱ]
    배열[ㄱ] = 배열[ㄴ]
    배열[ㄴ] = 임시

ㅎㅇ 파티셔닝(피봇: 수, 왼쪽: 수, 오른쪽: 수) 수:
    ㄱㄱ :
        ㄱㄱ :
            왼쪽 += 1
            ㅇㅈ? 배열[왼쪽] >= 배열[피봇]:
                ㅅㄱ
        ㄱㄱ :
            오른쪽 -= 1
            ㅇㅈ? 배열[오른쪽] <= 배열[피봇]:
                ㅅㄱ
        ㅇㅈ? 오른쪽 <= 왼쪽:
            치환(오른쪽, 피봇)
            ㅈㅈ 오른쪽
        치환(오른쪽, 왼쪽)


ㅎㅇ 퀵정렬(시작: 수, 끝: 수) :
    ㅇㅈ? 시작 + 1 >= 끝:
        ㅈㅈ

    피봇 = 파티셔닝(시작, 시작, 끝)

    퀵정렬(시작, 피봇)
    퀵정렬(피봇 + 1, 끝)

ㅎㅇ 시작() 수:
    ㄱㄱ 번호 = 0; 번호 < 배열크기; 번호 += 1:
        배열[번호] = rand()

    시작시간 = clock()

    퀵정렬(0, 배열크기)

    종료시간 = clock()

    ㄱㄱ 번호 = 0; 번호 < 배열크기; 번호 += 1:
        출(배열[번호])

    출((종료시간 - 시작시간), "ms")
```

## 알려진 문제:

**윈도우 CP949 지원 문제**

윈도우 명령 프롬프트의 한글 인코딩(CP949)과 일반적인 파일 인코딩(UTF-8)이 맞지 않아
파일 경로 탐색, 에러 메시지 출력, 구문 분석의 인코딩이 모두 달라지는 문제가 있습니다.

*줄랭 컴파일러는 모든 인코딩을 UTF-8로 강제합니다.*

따라서 실행 전에 터미널에서 "chcp 65001" 을 실행해 터미널 인코딩을 UTF-8로 바꿔주어야 합니다. 
또는 윈도우 시스템 로켈을 UTF-8로 설정해 인코딩을 UTF-8로 통일해주어야지 정상적으로 작동합니다.

IDE를 사용 중이라면 IDE 콘솔 인코딩을 UTF-8로 바꾸면 됩니다.

**배열 및 포인터 지원 문제**

줄랭 컴파일러의 타입 시스템은 굉장히 단순하게 이루어져 있고, 구조상 배열과 포인터를 처리하기가 매우 어렵습니다.
또한 원시 포인터를 그대로 도입하지 않고, 포인터를 안전하게 사용할 수 있는 도구들을 추가하려 하기 때문에, 배열 및 포인터에 대한 지원은 꽤 오랜 시간 후에 가능할 것 같습니다.


**에러 회복 문제**

컴파일 과정에서 한 번 틀린 문법이 오게 되면, 토큰 처리 순서가 깨지면서 그 뒤의 모든 구문을 에러로 처리하는 문제가 있습니다.
보통은 한 줄 내에 회복이 되지만, 운이 나쁘다면 함수 전체 또는 파일 전체가 에러로 뒤덮힐 수도 있습니다.

에러가 생긴 순간 컴파일을 중단해버리면 에러 메시지가 너무 많이 출력되는 문제는 해결되지만, 에러를 보기 위해 여러 번 컴파일하고 고쳐야 한다는 문제가 다시 생깁니다.

줄랭 컴파일러의 파서는 재귀 하향식으로 구현되어 있는데, 정보를 모두 함수 리턴값으로 전달하다 보니 전달할 정보를 추가하기가 쉽지 않은 상황입니다.

만약 에러 메시지가 너무 많이 출력된다면, 맨 위의 에러가 그 원인이므로 첫 번째 에러부터 고쳐주세요.

## 줄랭 컴파일러

줄랭 컴파일러는 줄랭을 LLVM IR코드로 (zul to ll) 변환하는 컴파일러 프론트엔드입니다. JIT기능을 내장하고 있어
코드를 즉시 실행할 수 있고, LLVM 관련 도구의 도움을 받으면 네이티브 바이너리로도 컴파일이 가능합니다.

만약 바이너리 실행 파일을 얻고 싶다면 줄랭 컴파일러에 적절한 옵션을 넣어 IR 코드 또는 비트코드로 컴파일하고, 이를 clang에 넘겨주면 됩니다.
gcc, msvc는 LLVM IR 코드를 해석할 수 없기 때문에, 오로지 clang만 가능합니다.

정석적인 방법은 llc와 lld를 사용하는 것이지만, clang에는 이러한 도구가 모두 연결되어 있기 때문에 clang을 사용하는 것이 가장 편리합니다.

아직 JIT Optimization이 지원되지 않기 때문에, 정말 빠른 프로그램을 원한다면 clang의 최적화 기능을 이용해주세요.
clang에 -O3 옵션을 넣어주면 됩니다. (JIT도 충분히 빠르긴 합니다)

컴파일러 옵션은 아래와 같습니다. (아무 옵션도 넣지 않으면 JIT로 실행합니다)

- zul [옵션] <줄랭 소스파일>


- --help : 커맨드 라인 옵션 도움말
- --version : 줄랭 컴파일러 버전
- -S : IR코드로 컴파일 (.ll 파일로 컴파일)
- -c : bitcode로 컴파일 (.bc로 컴파일)
- -o : 아웃풋 파일 이름 (-S 또는 -c 옵션을 주었을 때)

컴파일러의 자세한 동작 원리와 구조는 [줄랭 컴파일러 구조](./zullang_TMI.md#줄랭-컴파일러-구조)를 참고하세요

## 문법 지원 현황

| 문법                    | 상태          |
|-----------------------|-------------|
| 산술 연산                 | 지원          |
| 논리 연산                 | 지원          |
| 지역 변수                 | 지원          |
| 전역 변수                 | 지원          |
| 단락 평가 (short-circuit) | 지원          |
| ㅈㅈ문                   | 지원          |
| ㄱㄱ문                   | 지원          |
| ㅌㅌ문                   | 지원          |
| ㅅㄱ문                   | 지원          |
| ㅇㅈ문                   | 지원          |
| 전역 변수 초기화             | 지원          |
| 암시적 형변환               | 지원          |
| 명시적 형변환               | 지원 예정       |
| 함수                    | 지원          |
| 함수 반환 타입 추론           | 지원 예정       |
| 배열                    | 지원 (전역 변수만) |
| 함수 오버로딩               | 지원 예정       |
| 연산자 오버로딩              | 지원 예정       |
| 참조자                   | 지원 예정       |
| 포인터                   | 미정          |
| 클래스                   | 지원 예정       |
| 동적 할당                 | 미정          |
| 삼항 연산자                | 미정          |
| static, const         | 미정          |
| 리터럴 배열                | 지원 예정       |
| 리터럴 셋                 | 지원 예정       |
| 리터럴 맵                 | 지원 예정       |
| C 링킹                  | 지원          |
| C++ 링킹                | 지원 예정       |

## 참고 자료

[My First Language Frontend with LLVM Tutorial](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html)

[A Complete Guide to LLVM for Programming Language Creators](https://mukulrathi.com/create-your-own-programming-language/llvm-ir-cpp-api-tutorial/)

