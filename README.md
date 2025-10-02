# SirioC
SirioC 1.0 is a top tier UCI chess engine written in C++ and currently maintained by Jorge Ruiz with credits to Codex ChatGPT.

You can find the latest news, binaries and documentation on the official website: <https://ijccrl.com>.

## Building
```
git clone https://github.com/jordiqui/SirioC -b device
cd SirioC
make -j nopgo build=ARCH
```
ARCH choice: native, sse2, ssse3, x86-64-sse41-popcnt, avx2, avx2-pext, avx512. `native` is recommended however.
Recommended presets:

* Ivy Bridge or similar (SSE4.1 + POPCNT, sin AVX2/BMI2): `make -j nopgo build=x86-64-sse41-popcnt`
* Universal Windows 64-bit build: `make -j nopgo build=sse2`
You can remove the `nopgo` flag to enable profile guided optimization.


## Neural network
SirioC evaluates positions with a neural network trained on Lc0 data using the fastchess training framework.

If the engine prints `info string NNUE: no network found`, download the default weights with `make download-net` or place the requested `.nnue/.bin` file (default: `net89perm.bin`) next to the executable and/or set the `EvalFile` UCI option. Running the engine without weights falls back to a simple material evaluation and bench performance will be noticeably slower.


## Credits
* Jorge Ruiz and Codex ChatGPT for the current maintenance and guidance of the engine.
* To Styxdoto (or Styx), he has an incredible machine with 128 threads and he has donated CPU time
* To Witek902, for letting me in his OpenBench instance, allowing me to use massive hardware for my tests
* To fireandice, for training the neural network of SirioC 9.0
* The neural network of SirioC is trained with the fastchess framework
