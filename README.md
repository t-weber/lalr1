# lalr1
[![DOI: 10.5281/zenodo.6987396](https://zenodo.org/badge/DOI/10.5281/zenodo.6987396.svg)](https://doi.org/10.5281/zenodo.6987396)

LALR(1) parser generator library in C++20. Generates either a recursive ascent parser or LALR(1) tables out of a given grammar. Supports parser generation for multiple target languages.

Forked on 30 July 2022 from https://github.com/t-weber/lr1 [![DOI: 10.5281/zenodo.3965097](https://zenodo.org/badge/DOI/10.5281/zenodo.3965097.svg)](https://doi.org/10.5281/zenodo.3965097).


## Installation
- Create the build directory: `mkdir build && pushd build`.
- Build the libraries: `cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(($(nproc)/2+1))`.
- Install the libraries: `make install`.


## Examples
### Script Compiler
- After building the libraries (see "Installation" above), run `./script_parsergen` from the build directory to create the script compiler's parser component.
- Re-run `cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(($(nproc)/2+1))` to build the script compiler.
- Run an example script using `./script_compiler -r ../script_tests/fac.scr`.

### Expression Parser
- After building the libraries (see "Installation" above), run `./expr_parsergen` from the build directory to create the expression parser.
- A graph of the LALR(1) closures is written to the file "expr.svg".
- Re-run `cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(($(nproc)/2+1))` to build the expression parser.
- Run the expression parser using `./expr_compiler`.
