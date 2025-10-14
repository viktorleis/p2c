# P2C: Plan-to-Code Query Compiler

p2c is an educational compiling query engine.
Given an operator tree (query plan), it generates C++ code (hence plan-to-code).
The generated code is nicely formatted and can be inspected in `gen.cpp`.

Components:
- **`p2c.cpp`** - Main query compiler that generates C++ code from operator trees
- **`types.hpp`** - Type system supporting integers, doubles, strings, dates
- **`tpch.hpp`** - TPC-H schema definitions and database autoloading
- **`io.hpp`** - Memory-mapped I/O with columnar data access
- **`queryFrame.cpp`** - Runtime framework that executes generated code

## Getting Started
Prerequisites:
- C++23 compiler (gcc >= 14, clang >= 19)
- clang-format

To run a query, follow these steps:
1. **Data Generation**: Convert TPC-H CSV data to optimized binary columnar format
2. **Code Generation**: Transform query operators into optimized C++ code
3. **Compilation**: Build executable with generated code and runtime framework
4. **Execution**: Load data using memory-mapped files and execute generated code

### Data Generation:
```bash
cd data-generator
./generate-data.sh
```

This creates scale factor 1 TPC-H data in `data-generator/output/`.
The script first uses the `dbgen` tool to generate csv files, then reads and converts them to binary data. 

### Code Generation & Compilation:
```bash
make p2c   # Build the query compiler and sample query in p2c.cpp#main
make query # Compile generated query code
make       # Does all of the above 
```

### Execution:
```bash
# Run with default data location
./query

# Specify data path and run count
./query data-generator/output 3
```

The current implementation includes a sample query equivalent to:

```sql
SELECT o_orderstatus, count(*), min(o_totalprice), sum(o_totalprice)
FROM orders  
WHERE o_orderdate < '1995-03-15' AND o_orderpriority = '1-URGENT'
GROUP BY o_orderstatus
ORDER BY count(*)
```
