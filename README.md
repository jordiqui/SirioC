# SirioC
A top tier UCI chess engine written in C++ that I started developing in April 2023.

You can find the latest news, binaries and documentation on the official website: <https://ijccrl.com>.

## Building
```
git clone https://github.com/gab8192/SirioC
cd SirioC
make -j nopgo build=ARCH
```
ARCH choice: native, sse2, ssse3, avx2, avx2-pext, avx512. `native` is recommended however.
You can remove the `nopgo` flag to enable profile guided optimization.


## Neural network
SirioC evaluates positions with a neural network trained on Lc0 data using the fastchess training framework.


## Credits
* Credits to Codex Chatgpt for ongoing development support.
* To Styxdoto (or Styx), he has an incredible machine with 128 threads and he has donated CPU time
* To Witek902, for letting me in his OpenBench instance, allowing me to use massive hardware for my tests
* To fireandice, for training the neural network of SirioC 9.0
* The neural network of SirioC is trained with the fastchess framework
