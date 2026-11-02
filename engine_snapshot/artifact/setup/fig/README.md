
Finite Improbability Generator (FIG tool)
=========================================

The goal of the FIG tool is to estimate the probability of rare events using importance splitting techniques, with the highest possible degree of automation.
FIG is software developed in FAMAF, UNC, Argentina.

This tool is designed as the simulation engine backend of a wider toolset which will support analysis of a subset of Stochastic Automata (SA), namely Input/Output Stochastic Automata with Urgency (IOSA).
For a detailed and formal description of those systems and this project visit [the webpage of the DSG group](http://dsg.famaf.unc.edu.ar "DSG webpage") from [FAMAF](http://www.famaf.unc.edu.ar/ "Facultad de Matemática, Astronomía, Física, y Computación"), [UNC](https://www.unc.edu.ar/ "Universidad Nacional de Córdoba, Argentina").

Written in C++ 14, the project has been tested on linux and mac systems: see below how to install it.
FIG is free open source software (FOSS) released under the terms of the GNU General Public License version 3.
FIG's logo is the Fig of Gold, by Paula M. Cocucci, distributed under the CC-BY-SA license.

[![FIG logo](etc/logo/FIG_small.png "The Fig of Gold")](etc/logo)

### Author

- [Carlos E. Budde](https://webapps.unitn.it/du/it/Persona/PER0235360/Curriculum)

### Other main contributors

- [Raúl E. Monti](https://people.utwente.nl/r.e.monti)
- Leonardo M. Rodriguez


## Installation instructions

The native environment of the FIG tool is Linux, and it has been tested in Mac (although there's no official support).<br>
Requirements:

- C compiler collection: `gcc >= 5.4` or `clang >= 4.0`
- CMake buildystem generator: `cmake >= 2.8.13`
- Bison parser generator (dev-lib): `bison >= 3.0.4`
- Flex lexer generator (dev-lib): `flex >= 2.6.4`
- Z3 theorem prover (dev-lib): `z3 >= 4.4.1`

Arch Linux offers homonymous packages in its `core` and `extra` repositories; in Debian-based distributions such as Ubuntu, the corresponding packages are `gcc-<version>` and `clang-<version>` (e.g. `gcc-10` and `clang-11`), `cmake`, `libbison-dev`, `libfl-dev`, and `libz3-dev`.
For libraries, local copies of the corresponding `.so` files can be placed in the `lib/` subdirectory, checked at compilation.

To install the FIG tool first make sure your system satisfies all requirements listed above. Then download the source code from this repository, and execute the `build_fig.sh` script at the base.
A compile option allows choosing between the main FIG project (`-p main`, which compiles the fig executable) and the tests (`-p tests`, which compiles test). Other options offered are the compiler (`-c gcc` and `-c clang`) and the compile mode (`-m release` and `-m debug`). Invoking the script without arguments defaults to `-p main -m release -c clang`.

The resulting binary, located in a new `build/` subdirectory, is soft-linked at the base directory. If you compiled the main project the resulting executable is `fig`, so calling `./fig` after compilation will run the FIG tool. If you compiled the test suite the same applies to an executable `test`. To see the invocation syntax and all options of the FIG tool, compile the main project as indicated above and run `./fig --help`.

## Directory structure

DIRECTORY/ or FILE | CONTENT
-------------------|---------------------------------------
`doc/`             | Documentation (Doxygen)
`etc/`             | Miscelanea, e.g. Kepler parser for FTs
`experiments/`     | Shell scripts with FIG runs (legacy)
`ext/`             | Built-in third party static libraries
`include/`         | Header files of FIG
`lib/`             | Placeholder for third party dynamic libraries
`models/`          | IOSA model files, e.g. used for experiments
`src/`             | Source files of FIG
`tests/`           | Test suite files
`CMakeLists.txt`   | CMake build-configuration file
`LICENSE`          | The FIG tool license: GPLv3
`README.md`        | `goto top`
`build_fig.sh`     | Project building script
