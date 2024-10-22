#include <clang/Basic/DiagnosticOptions.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <llvm/IR/Module.h>
// #include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <iostream>

using clang::CompilerInstance;
using clang::CompilerInvocation;
using clang::DiagnosticConsumer;
using clang::DiagnosticOptions;
using clang::DiagnosticsEngine;
using clang::EmitLLVMOnlyAction;
using clang::TextDiagnosticPrinter;

using llvm::ArrayRef;
using llvm::IntrusiveRefCntPtr;
using llvm::MemoryBuffer;

int main() {
    const char* code_fname = "jit.cpp";
    const char* code_input =
        "#include <vector> // curr\n"
        "struct S { int a; int b; std::vector<int> c;};\n"
        "void init(struct S* s) { s->a = 42; s->b = 1337; s->c.push_back(s->a); }\n";

    // Setup custom diagnostic options.
    IntrusiveRefCntPtr<DiagnosticOptions> diag_opts(new DiagnosticOptions());
    diag_opts->ShowColors = 1;

    // Setup custom diagnostic consumer.
    //
    // We configure the consumer with our custom diagnostic options and set it
    // up that diagnostic messages are printed to stderr.
    std::unique_ptr<DiagnosticConsumer> diag_print =
        std::make_unique<TextDiagnosticPrinter>(llvm::errs(), diag_opts.get());

    // Create custom diagnostics engine.
    //
    // The engine will NOT take ownership of the DiagnosticConsumer object.
    auto diag_eng =
        std::make_unique<DiagnosticsEngine>(nullptr /* DiagnosticIDs */, diag_opts,
                                            diag_print.get(), false /* own DiagnosticConsumer */);

    // Create compiler instance.
    CompilerInstance cc;

    // Setup compiler invocation.
    //
    // We are only passing a single argument, which is the pseudo file name for
    // our code `code_fname`. We will be remapping this pseudo file name to an
    // in-memory buffer via the preprocessor options below.
    //
    // The CompilerInvocation is a helper class which holds the data describing
    // a compiler invocation (eg include paths, code generation options,
    // warning flags, ..).
    if (!CompilerInvocation::CreateFromArgs(cc.getInvocation(), ArrayRef<const char*>({code_fname,
                                                                                       "-stdlib=libc++",
                                                                             "-isystem", "/usr/include/c++/v1",
                                                                "-isystem", "/usr/local/lib/clang/20/include",
                                                                "-isystem", "/usr/include",
                                                                "-isystem", "/usr/include/x86_64-linux-gnu"}),
                                            *diag_eng)) {
        std::puts("Failed to create CompilerInvocation!");
        return 1;
    }

    // Setup a TextDiagnosticPrinter printer with our diagnostic options to
    // handle diagnostic messaged.
    //
    // The compiler will NOT take ownership of the DiagnosticConsumer object.
    cc.createDiagnostics(diag_print.get(), false /* own DiagnosticConsumer */);

    // Create in-memory readonly buffer with pointing to our C code.
    std::unique_ptr<MemoryBuffer> code_buffer = MemoryBuffer::getMemBuffer(code_input);
    // Configure remapping from pseudo file name to in-memory code buffer
    // code_fname -> code_buffer.
    //
    // Ownership of the MemoryBuffer object is moved, except we would set
    // `RetainRemappedFileBuffers = 1` in the PreprocessorOptions.
    cc.getPreprocessorOpts().addRemappedFile(code_fname, code_buffer.release());

    // Create action to generate LLVM IR.
    //
    // If created with default arguments, the EmitLLVMOnlyAction will allocate
    // an owned LLVMContext and free it once the action goes out of scope.
    //
    // To keep the context after the action goes out of scope, either pass a
    // LLVMContext (borrowed) when creating the EmitLLVMOnlyAction or call
    // takeLLVMContext() to move ownership out of the action.
    EmitLLVMOnlyAction action;
    // Run action against our compiler instance.
    if (!cc.ExecuteAction(action)) {
        std::puts("Failed to run EmitLLVMOnlyAction!");
        return 1;
    }

    // Take generated LLVM IR module and print to stdout.
    if (auto mod = action.takeModule()) {
        mod->print(llvm::outs(), nullptr);
    }

    return 0;
}
