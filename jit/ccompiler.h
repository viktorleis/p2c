#ifndef CCOMPILER_H
#define CCOMPILER_H

#include <clang/Basic/DiagnosticOptions.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/PreprocessorOptions.h>

#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>

namespace cc {

using clang::CompilerInstance;
using clang::CompilerInvocation;
using clang::DiagnosticConsumer;
using clang::DiagnosticOptions;
using clang::DiagnosticsEngine;
using clang::EmitLLVMOnlyAction;
using clang::TextDiagnosticPrinter;

using llvm::ArrayRef;
using llvm::Expected;
using llvm::IntrusiveRefCntPtr;
using llvm::LLVMContext;
using llvm::MemoryBuffer;
using llvm::Module;
using llvm::StringError;

class CCompiler {
public:
  CCompiler() {
    // Setup custom diagnostic options.
    auto DO = IntrusiveRefCntPtr<DiagnosticOptions>(new DiagnosticOptions());
    DO->ShowColors = 1;

    // Setup stderr custom diagnostic consumer.
    DC = std::make_unique<TextDiagnosticPrinter>(llvm::errs(), DO.get());

    // Create custom diagnostics engine.
    // The engine will NOT take ownership of the DiagnosticConsumer object.
    DE = std::make_unique<DiagnosticsEngine>(
        nullptr /* DiagnosticIDs */, std::move(DO), DC.get(),
        false /* own DiagnosticConsumer */);
  }

  struct CompileResult {
    std::unique_ptr<LLVMContext> C;
    std::unique_ptr<Module> M;
  };

  Expected<CompileResult> compile(const char* code) const {
    using std::errc;
    const auto err = [](errc ec) { return std::make_error_code(ec); };

    const char code_fname[] = "jit.cpp";

    // Create compiler instance.
    CompilerInstance CC;

    // Setup compiler invocation.
    bool ok = CompilerInvocation::CreateFromArgs(CC.getInvocation(),
           {code_fname,
             "-stdlib=libc++",
             "-isystem", "/usr/local/include/c++/v1",
             "-isystem", "/usr/local/lib/clang/20/include",
             "-isystem", "/usr/include",
             "-isystem", "/usr/include/x86_64-linux-gnu",
             "-fcxx-exceptions",
             "-std=c++20"}, *DE);
    // We control the arguments, so we assert.
    assert(ok);

    // Setup custom diagnostic printer.
    CC.createDiagnostics(DC.get(), false /* own DiagnosticConsumer */);

    // Configure remapping from pseudo file name to in-memory code buffer
    // code_fname -> code_buffer.
    //
    // PreprocessorOptions take ownership of MemoryBuffer.
    CC.getPreprocessorOpts().addRemappedFile(
        code_fname, MemoryBuffer::getMemBuffer(code).release());

    // Configure codegen options.
    //auto& CG = CC.getCodeGenOpts();
    //CG.OptimizationLevel = 3;
    //CG.setInlining(clang::CodeGenOptions::NormalInlining);

    // Generate LLVM IR.
    std::cout << "before llvm gen" << std::endl;
    EmitLLVMOnlyAction A;
    if (!CC.ExecuteAction(A)) {
      return llvm::make_error<StringError>(
          "Failed to generate LLVM IR from C code!",
          err(errc::invalid_argument));
    }

    // Take generated LLVM IR module and the LLVMContext.
    auto M = A.takeModule();
    M->print(llvm::outs(), nullptr);
    auto C = std::unique_ptr<LLVMContext>(A.takeLLVMContext());

    // TODO: Can this become nullptr when the action succeeds?
    assert(M);
    return CompileResult{std::move(C), std::move(M)};
  }

private:
  std::unique_ptr<DiagnosticConsumer> DC;
  std::unique_ptr<DiagnosticsEngine> DE;
};

}  // namespace cc

#endif
