build backend="AUTO":
    cmake -S . -B build \
        -G Ninja \
        -DCMAKE_C_COMPILER=/usr/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
        -DCMAKE_C_COMPILER_LAUNCHER= \
        -DCMAKE_CXX_COMPILER_LAUNCHER= \
        -DOPAL_BACKEND={{backend}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build --parallel

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

clang-tidy:
    find include src atlas aurora bezel editor finewave graphite hydra opal photon \
        \( -path '*/extern/*' -o -path '*/third-party/*' -o -path '*/build/*' \) -prune -o \
        \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) \
        -print0 | xargs -0 -P 1 -n1 \
        clang-tidy \
        -p build \
        --extra-arg=-isystem$(xcrun --show-sdk-path)/usr/include \
        --extra-arg=-isystem/opt/homebrew/opt/llvm/include/c++/v1 \
        -checks='-*,clang-diagnostic-*,portability-*,misc-include-cleaner' \
        2>&1 | tee clang-tidy.log