build backend="AUTO":
    cmake -S . -B build \
        -G Ninja \
        -DCMAKE_C_COMPILER=/usr/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
        -DCMAKE_C_COMPILER_LAUNCHER= \
        -DCMAKE_CXX_COMPILER_LAUNCHER= \
        -DOPAL_BACKEND={{backend}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build

clangd backend="AUTO":
    cmake -S . -B build \
        -G Ninja \
        -DCMAKE_C_COMPILER=/usr/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
        -DCMAKE_C_COMPILER_LAUNCHER= \
        -DCMAKE_CXX_COMPILER_LAUNCHER= \
        -DOPAL_BACKEND={{backend}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    ln -sf build/compile_commands.json compile_commands.json