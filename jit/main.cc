#include "ccompiler.h"
#include "jit.h"

int main() {
    const char code[] =
        "extern void libc_puts(const char*);"
        "struct S { int a; int b; };"
        "static void init_a(struct S* s) { s->a = 1111; }"
        "static void init_b(struct S* s) { s->b = 2222; }"
        "void init(struct S* s) {"
        "init_a(s); init_b(s);"
        "libc_puts(\"libc_puts()\"); }";

    auto R = cc::CCompiler().compile(code);
    // Abort if compilation failed.
    auto [C, M] = cantFail(std::move(R));
    // M->print(llvm::errs(), nullptr);

    // -- JIT compiler the IR module.

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    auto JIT = jit::Jit::Create();
    auto TSM = llvm::orc::ThreadSafeModule(std::move(M), std::move(C));

    auto RT = JIT->addModule(std::move(TSM));
    if (auto E = RT.takeError()) {
        llvm::errs() << llvm::toString(std::move(E)) << '\n';
        return 1;
    }

    if (auto ADDR = JIT->lookup("init")) {
        std::printf("JIT ADDR 0x%lx\n", (*ADDR).getAddress().getValue());

        struct S {
            int a, b;
        } state = {0, 0};
        auto JIT_FN = (*ADDR).getAddress().toPtr<void(struct S*)>();

        std::printf("S { a=%d b=%d }\n", state.a, state.b);
        JIT_FN(&state);
        std::printf("S { a=%d b=%d }\n", state.a, state.b);
    }

    // Remove jitted code tracked by this RT.
    cantFail((*RT)->remove());

    if (auto E = JIT->lookup("init").takeError()) {
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

