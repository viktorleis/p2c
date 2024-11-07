#include <iostream>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>

#include "ccompiler.h"
#include "jit.h"

struct P2CJit {
    static void execute(const std::string& gen_code){
    	llvm::DebugFlag = true;
        std::string finalized_code =
            "#include <functional>\n"
			"#include <tuple>\n"
			"#include <algorithm>\n"
			"#include <iostream>\n"
			"#include <string>\n"
			"#include <unordered_map>\n"
			"#include <vector>\n"
			"#include <cassert>\n"
            "#include \"io.hpp\"\n"
            "#include \"types.hpp\"\n"
			"#include \"tpch.hpp\"\n"
			"using namespace std;\n"
			"using namespace p2c;\n"

    		"void*   __dso_handle = (void*) &__dso_handle;\n"
            "void std::__libcpp_verbose_abort(char const* format, ...) noexcept {std::abort();}"
            "void jitted(){\n"
    	"TPCH db(\"data-generator/output/\");\n"
    	"for (uint64_t perfRepeat15 = 0; perfRepeat15 != 1; perfRepeat15++) {\n"
    	"	vector<tuple<int32_t, char>> vector14;\n"
    	"	map<tuple<char>, tuple<int32_t>> aggHT10;\n"
    	"	for (uint64_t i = 0; i != db.orders.tupleCount; i++) {\n"
    	"		char o_orderstatus3 = db.orders.o_orderstatus[i];\n"
    	"		double o_totalprice4 = db.orders.o_totalprice[i];\n"
    	"		date o_orderdate5 = db.orders.o_orderdate[i];\n"
    	"		std::string_view o_orderpriority6 = db.orders.o_orderpriority[i];\n"
    	"		if (std::less()(o_orderdate5, 2449792)) {\n"
    	"			if (std::equal_to()(o_orderpriority6, \"1-URGENT\")) {\n"
    	"				auto it = aggHT10.find({ o_orderstatus3 });\n"
    	"				if (it == aggHT10.end()) {\n"
    	"					aggHT10.insert({ { o_orderstatus3 }, { 1 } });\n"
    	"				} else {\n"
    	"					get<0>(it->second)++;\n"
    	"				}\n"
    	"			}\n"
    	"		}\n"
    	"	}\n"
    	"	for (auto& it : aggHT10) {\n"
    	"		char o_orderstatus3 = get<0>(it.first);\n"
    	"		int32_t cnt11 = get<0>(it.second);\n"
    	"		vector14.push_back({ cnt11, o_orderstatus3 });\n"
    	"	}\n"
    	"	sort(vector14.begin(), vector14.end(), [](const auto& t1, const auto& t2) { return t1 < t2; });\n"
    	"	for (auto& t : vector14) {\n"
    	"		int32_t cnt11 = get<0>(t);\n"
    	"		char o_orderstatus3 = get<1>(t);\n"
    	"		cout << o_orderstatus3 << \" \";\n"
    	"		cout << cnt11 << \" \";\n"
    	"		cout << endl;\n"
    	"	}\n"
    	"}\n"

        "\n}";
        auto R = cc::CCompiler().compile(finalized_code.c_str());
        // Abort if compilation failed.
        auto [C, M] = llvm::cantFail(std::move(R));
        M->print(llvm::errs(), nullptr);

        // -- JIT compiler the IR module.

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();

    	auto JIT = jit::Jit::Create();
    	auto TSM = llvm::orc::ThreadSafeModule(std::move(M), std::move(C));
    	std::string mangledInit = "_Z6jittedv";


    	auto RT = JIT->addModule(std::move(TSM));
    	if (auto E = RT.takeError()) {
    		llvm::errs() << llvm::toString(std::move(E)) << '\n';
    		exit(1);
    	}

    	if (auto ADDR = JIT->lookup(mangledInit)) {
    		std::printf("JIT ADDR 0x%lx\n", (*ADDR).getAddress().getValue());
    		auto JIT_FN = (*ADDR).getAddress().toPtr<void()>();
    		JIT_FN();
    		std::cout << "returned from function" << std::endl;
    	}
    }
};

