줄랭 TMI
=

줄랭과 관련된 여러 TMI를 설명해드립니다.

## 줄랭 로고

<img src="imgs/줄랭_메인로고.png" width="40%" height="40%" style="margin:0 5%"  alt="줄랭_메인로고">
<img src="imgs/줄랭_js스타일_로고.png" width="40%" height="40%" alt="줄랭_js로고">

줄랭 로고는 자유롭게 사용하실 수 있습니다. 

언젠가 줄랭 컴파일러를 완전한 프로그램으로 완성하게 되면, 왼쪽 로고는 줄랭 관련 프로그램에, 오른쪽 로고는 줄랭 파일에 사용할 예정입니다.

## 줄랭 세팅 방법

이 글은 [Building LLVM with CMake](https://llvm.org/docs/CMake.html)의 내용을 기반으로 합니다. LLVM cmake에 대한 설명을 더 자세히 보고 싶다면 해당 링크로 들어가서 보는 것을 추천드립니다.
아래에서는 제가 직접 LLVM을 세팅하면서 겪은 문제를 포함해 설명해 놓았습니다.

1. [LLVM github](https://github.com/llvm/llvm-project)를 클론합니다. 용량이 꽤 크기 때문에 --depth 옵션을 붙이는 것이 좋습니다.
    > git clone --depth 1 https://github.com/llvm/llvm-project.git 
   
2. 빌드 내용물이 들어갈 빈 디렉토리를 원하는 위치에 만들고, 해당 위치에서 터미널을 켜줍니다. llvm 디렉토리 안에 만드는 것을 추천합니다.


3. `llvm-project/llvm` 디렉토리의 절대 경로를 얻어옵니다. 이 부분이 꽤 헷갈리는데, LLVM 레포지토리 루트 디렉토리가 아니라, 
    그 하위에 있는 `llvm` 디렉토리가 빌드 대상입니다.


4. cmake 명령으로 빌드 시스템을 빌드합니다. 윈도우 기준으로는 ninja, clang 조합으로 빌드하는 것이 좋습니다. gcc, clang, visual studio의 선택지가 있는데 clang을 권장하는 이유는 아래와 같습니다.

   - windows gcc는 보통 mingw기반으로 동작해서 target이 잘못 설정되기 때문에 사용하기가 어렵습니다.
   - visual studio는 빌드 시간이 너무 길고, 빌드 용량이 너무 크기 때문에 추천하지 않습니다. 
   디버그 기준으로 LLVM을 빌드하는데 1시간이 걸리고, 빌드 파일은 61GB가 나옵니다.

아래와 같은 명령을 사용하면 cmake가 ninja 구성을 생성해줍니다. 반드시 빌드 내용물이 쓰여질 빈 디렉토리에서 
해당 명령을 실행해야 하고, ninja와 clang이 환경 변수에 등록되어 있어야 합니다.
> `cmake <llvm-project/llvm 경로> -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_C_COMPILER=clang++ -DLLVM_HOST_TRIPLE=x86_64-pc-windows-msvc`

-G는 제네레이터를 Ninja로 설정한 것이고, -D는 cmake 변수를 정의할 때 사용하는 옵션입니다. 
해당 옵션으로 CMAKE_BUILD_TYPE, CMAKE_C_COMPILER 등 변수들을 적절하게 설정하였습니다.

마지막의 LLVM_HOST_TRIPLE은 LLVM에서 사용하는 변수인데, 원래는 LLVM의 cmake스크립트가 돌아가면서 자동으로 타겟을 찾아 주지만, clang으로 빌드할 때는 이를 찾지 못하는 버그가 있었습니다.
단순히 이 변수를 직접 정의해줌으로써 해결할 수 있습니다. 참고로 "x86_64-pc-windows-msvc" 와 같은 문자열을 target triple이라고 하는데, 이는 clang --version 명령을 사용하여 쉽게 얻을 수 있습니다.

윈도우가 아닌 플랫폼에서는 LLVM_HOST_TRIPLE를 정의하지 않거나, 해당 플랫폼의 target triple로 설정하여 빌드하면 됩니다.

5. 이제 cmake가 ninja 구성을 생성했으므로, ninja를 실행하면 됩니다. `cmake --build .` 또는 `ninja`를 실행합니다.


6. 빌드가 끝나면 `<build 디렉토리 위치>/lib/cmake/llvm` 경로를 복사해줍니다. 이제 LLVM은 준비가 완료되었습니다.


7. 줄랭 레포지토리를 원하는 위치에 클론하고, 줄랭의 빌드 파일이 들어갈 빌드 디렉토리도 만들어줍니다.


8. 아까처럼 빌드 디렉토리로 들어가서, 아래 명령을 실행합니다.
> `cmake <줄랭 레포지토리 루트 경로> -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_C_COMPILER=clang++ -DLLVM_DIR=<위에서 복사한 경로>`

-DLLVM_DIR을 꼭 넣어주어야 합니다. 만약 릴리즈와 디버그 두 개의 빌드를 하려고 한다면, 릴리즈로 빌드할 때는 DLLVM_DIR에 릴리즈로 빌드한 
LLVM의 경로를, 디버그로 빌드할때는 디버그로 빌드한 LLVM의 경로를 넣어주면 됩니다.

이렇게 하면 모든 세팅이 끝나게 됩니다. LLVM을 빌드하는 것은 직접 명령어를 실행해서 진행하고, 줄랭 자체는 CLion같은 IDE를 통해 
빌드 설정을 하는 것을 추천드립니다.

## 줄랭 컴파일러 구조

곧 추가될 예정입니다.