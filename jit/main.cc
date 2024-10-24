#include <iostream>

#include <llvm/IR/Mangler.h>

#include "ccompiler.h"
#include "jit.h"

int main() {
    const char code[] =
"#include <vector> // curr\n"
"void std::__libcpp_verbose_abort(char const* format, ...) noexcept {std::abort();}"
"struct S { int a; int b; std::vector<int> c;};\n"
"void init(struct S* s) { s->a = 42; s->b = 1337; s->c.push_back(s->a); }\n";

    auto R = cc::CCompiler().compile(code);
    // Abort if compilation failed.
    auto [C, M] = cantFail(std::move(R));
    // M->print(llvm::errs(), nullptr);

    // -- JIT compiler the IR module.

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    auto JIT = jit::Jit::Create();
    auto TSM = llvm::orc::ThreadSafeModule(std::move(M), std::move(C));
    std::string mangledInit = "_Z4initP1S";


    auto RT = JIT->addModule(std::move(TSM));
    if (auto E = RT.takeError()) {
        llvm::errs() << llvm::toString(std::move(E)) << '\n';
        return 1;
    }

    if (auto ADDR = JIT->lookup(mangledInit)) {
        std::printf("JIT ADDR 0x%lx\n", (*ADDR).getAddress().getValue());

        struct S {
            int a, b;
            std::vector<int> c;
        } state = {0, 0, {}};
        auto JIT_FN = (*ADDR).getAddress().toPtr<void(struct S*)>();

        std::printf("S { a=%d b=%d, c=%zu }\n", state.a, state.b, state.c.size());
        JIT_FN(&state);
        std::printf("S { a=%d b=%d, c=%zu }\n", state.a, state.b, state.c.size());
    }

    // Remove jitted code tracked by this RT.
    cantFail((*RT)->remove());

    if (auto E = JIT->lookup(mangledInit).takeError()) {
        // In ERROR state, as expected, consume the error.
        llvm::consumeError(std::move(E));
    } else {
        // In SUCCESS state, not expected as code was dropped.
        llvm::errs() << "Expected error, we removed code tracked by RT and "
                        "hence 'init' should be "
                        "removed from the JIT!\n";
    }

    return 0;
}

